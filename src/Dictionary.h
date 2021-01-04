/*
  Implementation of the Dictionary data type
  for String key-value pairs, based on
  CRC32/64 has keys and binary tree search

  ---

  Copyright (C) Anatoli Arkhipenko, 2020
  All rights reserved.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.

  ---

  v1.0.0:
    2020-04-09 - Initial release

  v1.0.1:
    2020-04-10 - feature: operator (), examples, benchmarks

  v1.0.2:
    2020-04-10 - feature: operators == and !=
                 bug: memory leak after destroy method call.

  v1.1.0:
    2020-04-12 - feature: delete a node method.
                 feature: Dictionary Array optimization

  v1.1.1:
    2020-04-13 - feature: check if key exists via d("key")

  v1.2.0:
    2020-04-25 - bug: incorrect node handling during deletion
                 performance improvements

  v1.2.1:
    2020-04-26 - feature: switched to static crc tables

  v1.3.0:
    2020-04-27 - feature: crc 16/32/64 support. 32 is default

  v2.0.0:
    2020-05-14 - feature: support PSRAM for ESP32,
                 Switch to char* for key/values,
                 Error codes for memory-allocating methods
                 Key and Value max length constants

  v2.1.0:
    2020-05-21 - feature: json output and load from json string
                 feature: merge and '=' operator (proper assignment)
                 bug fix: destroy heap corruption fixed

  v2.1.1:
    2020-05-22 - bug fix: memory allocation issues during node deletion

  v2.1.2:
    2020-05-24 - consistent use of size_t type
    
  v3.0.0:
    2020-06-01 - non-CRC based search. Optimizations.

  v3.1.0:
    2020-06-03 - support for key and value compression (SHOCO and SMAZ). Optimizations.
    
  v3.1.1:
    2020-08-05 - clean-up to suppress compiler warnings
    
  v3.1.2:
    2020-09-16 - use of namespace for NodeArray
    
  v3.2.0:
    2020-12-21 - support for comments (#) in imported JSON files
                 bug fix: heap corrupt when missing a comma
                 feature: stricter JSON formatting check
 */

#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

#include "DictionaryDeclarations.h"

// ==== CONSTRUCTOR / DESTRUCTOR ==================================
Dictionary::Dictionary(size_t init_size) {

  dict.reserve(init_size);
  initSize = init_size;

#ifdef _DICT_COMPRESS
  // This however is a problem - need to think about a safer way
  iKeyTemp = (char*) malloc(sizeof(char) * (_DICT_KEYLEN + 1));
  iValTemp = (char*) malloc(sizeof(char) * (_DICT_VALLEN + 1));
#else
  iKeyTemp = NULL;
  iValTemp = NULL;
#endif
}

Dictionary::~Dictionary() {
  destroy();
#ifdef _DICT_COMPRESS
  free(iKeyTemp); iKeyTemp = NULL;
  free(iValTemp); iValTemp = NULL;
#endif
}



// ==== PUBLIC METHODS ===============================================

// ===== INSERTS =====================================================
int8_t Dictionary::insert(const char* keystr, const char* valstr) {
  // TODO: decide if to check for length here
  iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);

  if ( iKeyLen > _DICT_KEYLEN ) return DICTIONARY_ERR;
  if ( (iValLen = strnlen(valstr, _DICT_VALLEN + 1)) > _DICT_VALLEN ) return DICTIONARY_ERR;

#ifdef _DICT_COMPRESS
  int8_t rc;
  if ( (rc = compressKey(keystr)) ) return rc;
  if ( (rc = compressValue(valstr)) ) return rc;
#else
  iKeyTemp = (char*) keystr;
  iValTemp = (char*) valstr;
#endif

  //uintNN_t key = crc(iKeyTemp, iKeyLen);

  set(iKeyTemp, iValTemp);
  return (DICTIONARY_OK);
}

int8_t Dictionary::update(const char* keystr, const char* valstr) {
  // TODO: decide if to check for length here
  iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);

  if ( iKeyLen > _DICT_KEYLEN ) return DICTIONARY_ERR;
  if ( (iValLen = strnlen(valstr, _DICT_VALLEN + 1)) > _DICT_VALLEN ) return DICTIONARY_ERR;

#ifdef _DICT_COMPRESS
  int8_t rc;
  if ( (rc = compressKey(keystr)) ) return rc;
  if ( (rc = compressValue(valstr)) ) return rc;
#else
  iKeyTemp = (char*) keystr;
  iValTemp = (char*) valstr;
#endif

  auto p = dict.find(iKeyTemp);
  if (p != dict.end()){
    dict[keystr] = valstr;
  }

  return (DICTIONARY_OK);
}

int8_t Dictionary::emplace(const char* keystr, const char* valstr) {
  // TODO: decide if to check for length here
  iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);

  if ( iKeyLen > _DICT_KEYLEN ) return DICTIONARY_ERR;
  if ( (iValLen = strnlen(valstr, _DICT_VALLEN + 1)) > _DICT_VALLEN ) return DICTIONARY_ERR;

#ifdef _DICT_COMPRESS
  int8_t rc;
  if ( (rc = compressKey(keystr)) ) return rc;
  if ( (rc = compressValue(valstr)) ) return rc;
#else
  iKeyTemp = (char*) keystr;
  iValTemp = (char*) valstr;
#endif

  bool status = dict.emplace(iKeyTemp, iValTemp).second;

#ifdef _LIBDEBUG_
  status ? Serial.printf("DICT-implace: Insert key:val '%s:%s'\n", iKeyTemp, iValTemp) : Serial.printf("DICT-implace: key: '%s' already exist\n", iKeyTemp);
#endif

  return (DICTIONARY_OK);
}

// ==== SEARCHES AND LOOKUPS ===============================================
String Dictionary::search(const char* keystr) {
    iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);
#ifdef _DICT_COMPRESS
    int8_t rc;
#endif

    if (iKeyLen <= _DICT_KEYLEN) {
#ifdef _DICT_COMPRESS
        if ( (rc = compressKey(keystr)) ) return String("");
#else
        iKeyTemp = (char*) keystr;
#endif

        auto p = dict.find(iKeyTemp);
        if (p != dict.end()){

#ifdef _DICT_COMPRESS
            decompressValue(p->second.c_str(), strlen(p->second.c_str()));
            return String(iValTemp);
#else
            return String(p->second.c_str()); 
#endif
        }
    }
    return String("");
}

String Dictionary::key(size_t i) {
/*
This doesn't work with unordered_map
or we (might) return an iterator here?)
*/
  return String("");
}

String Dictionary::value(size_t i) {
/*
This doesn't work with unordered_map
or we (might) return an iterator here?)
*/
    return String("");
}


// ==== DELETES =====================================================
void Dictionary::destroy() {
    dict.clear();
}

int8_t Dictionary::remove(const String& keystr) {
    return remove(keystr.c_str());
}

int8_t Dictionary::remove(const char* keystr) {
#ifdef _LIBDEBUG_
    Serial.printf("Dictionary::remove: %s\n", keystr);
#endif
#ifdef _DICT_COMPRESS
    int8_t rc;
#endif
    iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);
    if (iKeyLen > _DICT_KEYLEN) return DICTIONARY_ERR;

#ifdef _DICT_COMPRESS
    if ( (rc = compressKey(keystr)) ) return rc;
#else
    iKeyTemp = (char*) keystr;
#endif

    dict.erase(iKeyTemp);
#ifdef _LIBDEBUG_
      Serial.printf("Removed key: %s\n", keystr);
#endif

    return DICTIONARY_OK;
}


// ==== SIZES ============================================================================
// This is the size of the Dictionary in memory (just data, not object)
size_t Dictionary::size() {
    size_t sz = 0;
    for (const auto& x : dict){
      sz += x.first.length() + x.second.length();
    }

    return sz;
}

// This is size of JSON file to be created out of this dictionary
size_t Dictionary::jsize() {
    // {"key":"value","key":"value"}\0:
    // 3 (2 brackets and 1 zero terminator) + 4 quotes, a comma and a semicolon = 6 chars)
    // minus one last comma
    size_t sz = 2 + count() * 6;
    sz += size();

    return sz;
}

// This is size method for storing in EEPROM
size_t Dictionary::esize() {
    return size();
}


// ==== JSON RELATED ================================================
String Dictionary::json() {
    String s((char *)0);    // do not pre-allocate anything, reserve() will follow

    size_t size = jsize();
    s.reserve(size);
    s += '{';

    for (const auto& x : dict){
        String vv = x.second.c_str();
        vv.replace("\"","\\\"");
        s += '"';
        s += x.first.c_str();
        s += "\":\"";
        s += vv;
        s += "\",";
    }
    s.remove(size-1,1);  // chop last coma
    s += '}';

    return s;
}

int8_t Dictionary::jload(const String& json, int num) {
    return jload(json.c_str(), num);
}

int8_t Dictionary::jload(const char* jc, int aNum) {
    bool insideQoute = false;
    bool nextVerbatim = false;
    bool isValue = false;
    size_t len = strlen(jc);
    bool isComment = false;
    int p = 0;
    int8_t rc;
    String currentKey;
    String currentValue;

    for (size_t i = 0; i < len; i++, jc++) {
        char c = *jc;
        
        if ( isComment ) {
          if ( c == '\n' ) {
            isComment = false;
            isValue = false;
          }
          continue;
        }
        if (nextVerbatim) {
          nextVerbatim = false;
        }
        else {
          // process all special cases: '\', '"', ':', and ','
          if (c == '\\' ) {
            nextVerbatim = true;
            continue;
          }
          if ( c == '#' ) {
            if ( !insideQoute ) {
              isComment = true;
              continue;
            }
          }
          if ( c == '\"' ) {
            if (!insideQoute) {
              insideQoute = true;
              continue;
            }
            else {
              insideQoute = false;
              if (isValue) {
                rc = insert( currentKey, currentValue );
                if (rc) return DICTIONARY_MEM;  // if error - exit with an error code
                currentValue = String();
                currentKey = String();
                p++;
                if (aNum > 0 && p >= aNum) break;
              }
              continue;
            }
          }
          if (c == '\n') {
            if ( insideQoute ) {
              return DICTIONARY_QUOTE;
            }
            if ( nextVerbatim ) {
              return DICTIONARY_BCKSL;
            }
            isValue = false;  // missing comma, but let's forgive that
            continue;
          }
          if (!insideQoute) {
            if (c == ':') {
              if ( isValue ) {
                return DICTIONARY_COMMA; //missing comma probably
              }
              isValue = true;
              continue;
            }
            if (c == ',') {
              if ( !isValue ) {
                return DICTIONARY_COLON; //missing colon probably
              }
              isValue = false;
              continue;
            }
            if ( c == '{' || c == '}' || c == ' ' || c == '\t' ) continue;
            return DICTIONARY_FMT;
          }
        }
        if (insideQoute) {
          if (isValue) currentValue.concat(c);
          else currentKey.concat(c);
        }
      }
      if (insideQoute || nextVerbatim || (aNum > 0 && p < aNum )) return DICTIONARY_EOF;
    #ifdef _LIBDEBUG_
        Serial.printf("Dictionary::jload: DICTIONARY_OK\n");
    #endif
      return DICTIONARY_OK;
}


int8_t Dictionary::merge(Dictionary& source) {
    // think about optimizing it with move
    auto iterator = dict;
    dict.insert(source.dict.begin(), source.dict.end());

    return DICTIONARY_OK;
}

// ==== OPERATORS ====================================

bool Dictionary::operator () (const String& keystr) {
    iKeyLen = keystr.length();
    if (iKeyLen > _DICT_KEYLEN) return false;

#ifdef _DICT_COMPRESS
    int8_t rc;
    if ( (rc = compressKey(keystr.c_str())) ) return false;
#else
    iKeyTemp = (char*) keystr.c_str();
#endif

    auto lookup = dict.find(iKeyTemp);
    if (lookup != dict.end())
      return true;
    else
      return false;
}


bool Dictionary::operator == (Dictionary& b) {
  return (dict == b.dict);
}




// ==== PRIVATE METHODS ====================================================
// ==== INSERTS ============================================================

// ==== KEY/CRC METHODS ===============================================

uintNN_t Dictionary::crc(const void* data, size_t n_bytes) {
    uintNN_t    a = 0;

    memcpy((void*)&a, data, n_bytes < sizeof(uintNN_t) ? n_bytes : sizeof(uintNN_t));
    return a;
}

#ifdef _DICT_COMPRESS

// ==== COMPRESS METHODS =============================================
int8_t Dictionary::compressKey(const char* aStr) {
    memset(iKeyTemp, 0, sizeof(uintNN_t));

#if defined (_DICT_COMPRESS_SHOCO)
    iKeyLen = shoco_compress(aStr, 0, iKeyTemp, _DICT_KEYLEN + 1);

#elif defined (_DICT_COMPRESS_SMAZ)
    iKeyLen = smaz_compress((char*) aStr, strlen(aStr), iKeyTemp, _DICT_KEYLEN + 1);

// #else
//     iKeyLen = strlen(aStr);
//     memcpy(iKeyTemp, aStr, iKeyLen);

#endif

    if (iKeyLen > _DICT_KEYLEN + 1) return DICTIONARY_OOB;

#ifdef _LIBDEBUG_
    Serial.println("DICT-COMPKEY:");
    Serial.printf("\t string = %s, iKeyLen = %d\n", aStr, iKeyLen);
#endif
    return DICTIONARY_OK;
}

int8_t Dictionary::compressValue(const char* aStr) {

#if defined (_DICT_COMPRESS_SHOCO) 
    iValLen = shoco_compress(aStr, 0, iValTemp, _DICT_VALLEN + 1);

#elif defined (_DICT_COMPRESS_SMAZ)
    iValLen = smaz_compress((char*) aStr, strlen(aStr), iValTemp, _DICT_VALLEN + 1);

// #else
//     iValLen = strlen(aStr);
//     memcpy(iValTemp, aStr, iValLen);

#endif

    if (iValLen > _DICT_VALLEN + 1) return DICTIONARY_OOB;
    
#ifdef _LIBDEBUG_
    Serial.println("DICT-COMPVAL:");
    Serial.printf("\t string = %s, iValLen = %d\n", aStr, iValLen);
#endif
    return DICTIONARY_OK;
}

void Dictionary::decompressKey(const char* aBuf, _DICT_KEY_TYPE aLen) {

#if defined (_DICT_COMPRESS_SHOCO)
    iKeyLen = shoco_decompress(aBuf, aLen, iKeyTemp, _DICT_KEYLEN + 1);
    iKeyTemp[iKeyLen] = 0;

#elif defined (_DICT_COMPRESS_SMAZ)
    iKeyLen = smaz_decompress((char*) aBuf, (int) aLen, iKeyTemp, (int) (_DICT_KEYLEN + 1));
    iKeyTemp[iKeyLen] = 0;

// #else
//     memcpy(iKeyTemp, aBuf, aLen);
//     iKeyTemp[aLen] = 0;
//     iKeyLen = aLen;

#endif
}

void Dictionary::decompressValue(const char* aBuf, _DICT_VAL_TYPE aLen) {
    
#if defined (_DICT_COMPRESS_SHOCO)
    iValLen = shoco_decompress(aBuf, aLen, iValTemp, _DICT_VALLEN + 1);
    iValTemp[iValLen] = 0;

#elif defined (_DICT_COMPRESS_SMAZ)
    iValLen = smaz_decompress((char*) aBuf, (int) aLen, iValTemp, (int) (_DICT_VALLEN + 1) );
    iValTemp[iValLen] = 0;

// #else
//     memcpy(iValTemp, aBuf, aLen);
//     iValTemp[aLen] = 0;
//     iValLen = aLen;

#endif

#ifdef _LIBDEBUG_
    Serial.printf("\t iValTemp = %s, iValLen = %d\n", iValTemp, iValLen);
#endif
}

#endif // _DICT_COMPRESS


// ==== DEBUG METHODS ===================================================
/*
#ifdef _LIBDEBUG_
void Dictionary::printDictionary(node* root) {
  if (root != NULL)
  {
    printDictionary(root->left);
    printNode(root);
    printDictionary(root->right);
  }
}

void Dictionary::printNode(node* root) {
  if (root != NULL) {
//    Serial.printf("%u: (%u:%s,%s %u:%u) [l:%u, r:%u]\n", (uint32_t)root, root->key(), root->keybuf, root->valbuf, (uint32_t)root->keybuf, (uint32_t)root->valbuf, (uint32_t)root->left, (uint32_t)root->right);
    Serial.printf("%u: (%u:%u:%u) [l:%u, r:%u]\n", (uint32_t)root, root->key(), (uint32_t)root->keybuf, (uint32_t)root->valbuf, (uint32_t)root->left, (uint32_t)root->right);
  }
  else {
    Serial.println("NULL:");
  }
}
#endif
*/

#endif // #define _DICTIONARY_H_
