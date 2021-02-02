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
                 
  v3.2.1:
    2021-01-04 - bug fix: import of files with windows-style CR/LF
    
  v3.2.2:
    2021-01-08 - bug fix: should not allow keys with zero length (crashes search)
    
 */


#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

#ifndef _DICT_KEYLEN
#define _DICT_KEYLEN 64
#endif

#if _DICT_KEYLEN < UINT8_MAX
#define _DICT_KEY_TYPE  uint8_t
#endif

#if _DICT_KEYLEN >= UINT8_MAX && _DICT_KEYLEN < UINT16_MAX
#define _DICT_KEY_TYPE  uint16_t
#endif

#if _DICT_KEYLEN >= UINT16_MAX && _DICT_KEYLEN < UINT32_MAX
#define _DICT_KEY_TYPE  uint32_t
#endif

// What is the likelihood of a microcontroller having that much memory?
#if _DICT_KEYLEN >= UINT32_MAX
#define _DICT_KEY_TYPE  uint64_t
#endif



#ifndef _DICT_VALLEN
#define _DICT_VALLEN 254
#endif

#if _DICT_VALLEN < UINT8_MAX
#define _DICT_VAL_TYPE  uint8_t
#endif

#if _DICT_VALLEN >= UINT8_MAX && _DICT_VALLEN < UINT16_MAX
#define _DICT_VAL_TYPE  uint16_t
#endif

#if _DICT_VALLEN >= UINT16_MAX && _DICT_VALLEN < UINT32_MAX
#define _DICT_VAL_TYPE  uint32_t
#endif

#if _DICT_VALLEN >= UINT32_MAX
#define _DICT_VAL_TYPE  uint64_t
#endif



#define DICTIONARY_OK         0
#define DICTIONARY_ERR      (-1)
#define DICTIONARY_MEM      (-2)
#define DICTIONARY_OOB      (-3)

#define DICTIONARY_COMMA    (-20)
#define DICTIONARY_COLON    (-21)
#define DICTIONARY_QUOTE    (-22)
#define DICTIONARY_BCKSL    (-23)
#define DICTIONARY_FMT      (-25)
#define DICTIONARY_EOF      (-99)


// There is no CRC calculation anymore, but the naming stuck
#ifndef _DICT_CRC
#define _DICT_CRC  32
#endif

#if !( _DICT_CRC == 16 || _DICT_CRC == 32 || _DICT_CRC == 64)
#define _DICT_CRC  32
#endif

#if _DICT_CRC == 16
#define uintNN_t uint16_t
#endif

#if _DICT_CRC == 32
#define uintNN_t uint32_t
#endif

#if _DICT_CRC == 64
#define uintNN_t uint64_t
#endif

#if defined(_DICT_COMPRESS_SHOCO)

#define _DICT_COMPRESS
#define _DICT_EXTRA 0
#include "shoco/shoco.h"

#elif defined (_DICT_COMPRESS_SMAZ)

#define _DICT_COMPRESS
#define _DICT_EXTRA 0
extern "C" {
#include "smaz/smaz.h"
}
#endif

#ifndef _DICT_EXTRA
#define _DICT_EXTRA 1
#endif


#include <Arduino.h>
#include "NodeArray.h"
using namespace NodeArray;

#ifdef _DICT_PACK_STRUCTURES
class __attribute((__packed__)) Dictionary {
#else
class Dictionary {
#endif
  public:
    Dictionary(size_t init_size = 10);
    ~Dictionary();

    inline int8_t       insert(String keystr, String valstr);
    int8_t              insert(const char* keystr, const char* valstr);
    
    inline String       search(const String& keystr);
    String              search(const char* keystr);
    String              key(size_t i);
    String              value(size_t i);

    void                destroy();
    inline int8_t       remove(const String& keystr);
    int8_t              remove(const char* keystr);

    size_t              size();
    size_t              jsize();
    size_t              esize();
    
    String              json();
    int8_t              jload (const String& json, int aNum = 0);
    int8_t              merge (Dictionary& dict);


    void operator = (Dictionary& dict) {
      destroy();
      merge(dict);
    }

    inline String operator [] (const String& keystr) { return search(keystr); }
    inline String operator [] (size_t i) { return value(i); }
    inline int8_t operator () (String keystr, String valstr) { return insert(keystr, valstr); }
    inline int8_t operator () (const char* keystr, const char* valstr) { return insert(keystr, valstr); }

    bool operator () (const String& keystr);

    String operator () (size_t i) { return key(i); }
    inline bool operator == (Dictionary& b);
    inline bool operator != (Dictionary& b) { return (!(*this == b)); }
    inline size_t count() { return ( Q ? Q->count() : 0); }

#ifdef _LIBDEBUG_
    void printNode(node* root);
    void printDictionary(node* root);
    void printDictionary() {
      Serial.printf("\nDictionary::printDictionary:\n");
      printDictionary(iRoot);
      Serial.println();
    };
    void printArray() {
      Q->printArray();
    };
#endif

  private:
// methods
    int8_t              insert(uintNN_t key, const char* keystr, _DICT_KEY_TYPE keylen, const char* valstr, _DICT_VAL_TYPE vallen, node* leaf);
    node*               search(uintNN_t key, node* leaf, const char* keystr, _DICT_KEY_TYPE keylen);

    void                destroy_tree(node* leaf);
    node*               deleteNode(node* root, uintNN_t key, const char* keystr, _DICT_KEY_TYPE keylen);
    node*               minValueNode(node* n);

    uintNN_t            crc(const void* data, size_t n_bytes);

#ifdef _DICT_COMPRESS
    int8_t              compressKey(const char* aStr);
    int8_t              compressValue(const char* aStr);
    void                decompressKey(const char* aBuf, _DICT_KEY_TYPE aLen);
    void                decompressValue(const char* aBuf, _DICT_VAL_TYPE aLen);
#endif

// data
    node*               iRoot;
    NodeArray::NodeArray* Q;
    size_t              initSize;

    char*            iKeyTemp;
    _DICT_KEY_TYPE      iKeyLen;
    char*            iValTemp;
    _DICT_VAL_TYPE      iValLen;
};



// ==== CONSTRUCTOR / DESTRUCTOR ==================================
Dictionary::Dictionary(size_t init_size) {
  iRoot = NULL;

  // This is unlikely to fail as practically no memory is allocated by the NodeArray
  // All memory allocation is delegated to the first append
  Q = new NodeArray::NodeArray(init_size);
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
  delete Q;
#ifdef _DICT_COMPRESS
  free(iKeyTemp); iKeyTemp = NULL;
  free(iValTemp); iValTemp = NULL;
#endif
}



// ==== PUBLIC METHODS ===============================================

// ===== INSERTS =====================================================
int8_t Dictionary::insert(String keystr, String valstr) {
  return insert(keystr.c_str(), valstr.c_str());
}

int8_t Dictionary::insert(const char* keystr, const char* valstr) {
  // TODO: decide if to check for length here
  iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);
#ifdef _DICT_COMPRESS
  int8_t rc;
#endif

  if ( iKeyLen == 0 || iKeyLen > _DICT_KEYLEN ) return DICTIONARY_ERR;
  if ( (iValLen = strnlen(valstr, _DICT_VALLEN + 1)) > _DICT_VALLEN ) return DICTIONARY_ERR;

#ifdef _DICT_COMPRESS
  if ( (rc = compressKey(keystr)) ) return rc;
  if ( (rc = compressValue(valstr)) ) return rc;
#else
  iKeyTemp = (char*) keystr;
  iValTemp = (char*) valstr;
#endif

  uintNN_t key = crc(iKeyTemp, iKeyLen);

  if (iRoot != NULL)
    return insert(key, iKeyTemp, iKeyLen, iValTemp, iValLen, iRoot);
  else {
    int8_t rc;

    iRoot = new node;
    if (!iRoot) return DICTIONARY_MEM;
    rc = iRoot->create(iKeyTemp, iKeyLen, iValTemp, iValLen, NULL, NULL);

#ifdef _LIBDEBUG_
    Serial.printf("DICT-insert: creating root entry. rc = %d\n", rc);
#endif

    if (rc) {
      delete iRoot;
      return rc;
    }
    rc = Q->append(iRoot);
    if (rc) {
      delete iRoot;
      return rc;
    }
  }
  return DICTIONARY_OK;
}


// ==== SEARCHES AND LOOKUPS ===============================================
String Dictionary::search(const String& keystr) {
    return search(keystr.c_str());
}

String Dictionary::search(const char* keystr) {
    iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);
#ifdef _DICT_COMPRESS
    int8_t rc;
#endif

    if (iKeyLen != 0 && iKeyLen <= _DICT_KEYLEN) {
#ifdef _DICT_COMPRESS
        if ( (rc = compressKey(keystr)) ) return String("");
#else
        iKeyTemp = (char*) keystr;
#endif
        uintNN_t key = crc(iKeyTemp, iKeyLen);
//        iKeyLen = ( iKeyLen < _DICT_LEN ? _DICT_LEN : iKeyLen );

        node* p = search(key, iRoot, iKeyTemp, iKeyLen);
        if (p) {
#ifdef _DICT_COMPRESS
            decompressValue(p->valbuf, p->vsize);
#else
            iValTemp = p->valbuf;
            iValTemp[p->vsize] = 0;
#endif
            return String(iValTemp);
        }
    }
    return String("");
}

String Dictionary::key(size_t i) {
  if (Q) {
    node* p = (*Q)[i];
    if (p) {
#ifdef _DICT_COMPRESS
        decompressKey(p->keybuf, p->ksize);
#else
        iKeyTemp = p->keybuf;
        iKeyTemp[p->ksize] = 0;
#endif
        return String(iKeyTemp);
    }
  }
  return String();
}

String Dictionary::value(size_t i) {
    if (Q) {
        node* p = (*Q)[i];
        if (p) {
#ifdef _LIBDEBUG_
    Serial.printf("Dictionary::value:\n");
    Serial.printf("\tFound ptr = %u (%u:%d)\n", (uint32_t)p, (uint32_t)p->valbuf, p->vsize);
#endif
#ifdef _DICT_COMPRESS
            decompressValue(p->valbuf, p->vsize);
#else
            iValTemp = p->valbuf;
            iValTemp[p->vsize] = 0;
#endif
            return String(iValTemp);
        }
    }
    return String();
}


// ==== DELETES =====================================================
void Dictionary::destroy() {
    destroy_tree(iRoot);
    iRoot = NULL;
    delete Q;
    Q = new NodeArray::NodeArray(initSize);
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

    uintNN_t key = crc(iKeyTemp, iKeyLen);
    node* p = search(key, iRoot, iKeyTemp, iKeyLen);

    if (p) {
#ifdef _LIBDEBUG_
        Serial.printf("Found key to delete int: %u\n", p->key());
        Serial.printf("Found key to delete ptr: %u\n", (uint32_t)p);
//        Serial.printf("Found key to delete str: %s\n", keystr);
#endif
        iRoot = deleteNode(iRoot, p->key(), iKeyTemp, iKeyLen);
    }
    return DICTIONARY_OK;
}


// ==== SIZES ============================================================================
// This is the size of the Dictionary in memory (just data, not object)
size_t Dictionary::size() {
    size_t ct = count();
    size_t sz = 0;
    for (size_t i = 0; i < ct; i++) {
        sz += (*Q)[i]->ksize;
        sz += (*Q)[i]->vsize;
        sz += sizeof(node);  // to account for size of the node itself
    }
    return sz;
}

// This is size of JSON file to be created out of this dictionary
size_t Dictionary::jsize() {
    size_t ct = count();
    // {"key":"value","key":"value"}\0:
    // 3 (2 brackets and 1 zero terminator) + 4 quotes, a comma and a semicolon = 6 chars)
    // minus one last comma
    size_t sz = 2 + ct * 6;
    for (size_t i = 0; i < ct; i++) {
        sz += key(i).length();
        sz += value(i).length();
    }
    return sz;
}

// This is size method for storing in EEPROM
size_t Dictionary::esize() {
    size_t ct = count();
    size_t sz = 0;

    for (size_t i = 0; i < ct; i++) {
        sz += key(i).length() + 1;
        sz += value(i).length() + 1;
    }
    return sz;
}


// ==== JSON RELATED ================================================
String Dictionary::json() {
    String s;

    s.reserve(jsize());
    s = '{';

    size_t ct = count();
    for (size_t i = 0; i < ct; i++) {
        String vv = value(i);
        vv.replace("\"","\\\"");
        s += '"' + key(i) + "\":\"" + vv + '"';
        if (i < ct - 1) s += ',';
    }
    s += '}';

    return s;
}


int8_t Dictionary::jload(const String& json, int aNum) {
    bool insideQoute = false;
    bool nextVerbatim = false;
    bool isValue = false;
    bool isComment = false;
    const char* jc = json.c_str();
    size_t len = json.length();
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
            if ( c == '{' || c == '}' || c == ' ' || c == '\t' || c == '\r' ) continue;
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


int8_t Dictionary::merge(Dictionary& dict) {
    size_t ct = dict.count();
    int8_t rc;

    for (size_t i = 0; i < ct; i++) {
        rc = insert(dict(i), dict[i]);
        if (rc) return rc;
    }
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

    uintNN_t key = crc(iKeyTemp, iKeyLen);

    node* p = search(key, iRoot, iKeyTemp, iKeyLen);
    if (p) return true;
    return false;
}


bool Dictionary::operator == (Dictionary& b) {
    if (b.size() != size()) return false;
    if (b.count() != count()) return false;
    size_t ct = count();
    for (size_t i = 0; i < ct; i++) {
        if (value(i) != b[key(i)]) return false;
    }
    return true;
}




// ==== PRIVATE METHODS ====================================================
// ==== INSERTS ============================================================
int8_t Dictionary::insert(uintNN_t key, const char* keystr, _DICT_KEY_TYPE keylen, const char* valstr, _DICT_VAL_TYPE vallen, node* leaf) {
    if (key < leaf->key()) {
        if (leaf->left != NULL)
            return insert(key, keystr, keylen, valstr, vallen, leaf->left);
        else {
            int8_t rc;

            leaf->left = new node;
            if (!leaf->left) return DICTIONARY_MEM;
            rc = leaf->left->create(keystr, keylen, valstr, vallen, NULL, NULL);
            if (rc) {
                delete leaf->left;
                return rc;
            }
            rc = Q->append(leaf->left);
            if (rc) {
                delete leaf->left;
                return rc;
            }
        }
    }
    else if (key > leaf->key()) {
        if (leaf->right != NULL)
            return insert(key, keystr, keylen, valstr, vallen, leaf->right);
        else {
            int8_t rc;

            leaf->right = new node;
            if (!leaf->right) return DICTIONARY_MEM;
            rc = leaf->right->create(keystr, keylen, valstr, vallen, NULL, NULL);
            if (rc) {
                delete leaf->right;
                return rc;
            }
            rc = Q->append(leaf->right);
            if (rc) {
                delete leaf->right;
                return rc;
            }
        }
    }
    else if (key == leaf->key()) {
        int cmpres = keylen != leaf->ksize ? keylen - leaf->ksize : memcmp(leaf->keybuf, keystr, keylen);
        if (cmpres == 0) {
            if (leaf->updateValue(valstr, vallen) != NODEARRAY_OK) return DICTIONARY_MEM;
        }
        else {

            if (cmpres < 0) {
                if (leaf->left != NULL)
                    return insert(key, keystr, keylen, valstr, vallen, leaf->left);
                else {
                    int8_t rc;

                    leaf->left = new node;
                    if (!leaf->left) return DICTIONARY_MEM;
                    rc = leaf->left->create(keystr, keylen, valstr, vallen, NULL, NULL);
                    if (rc) {
                        delete leaf->left;
                        return rc;
                    }
                    rc = Q->append(leaf->left);
                    if (rc) {
                        delete leaf->left;
                        return rc;
                    }
                }
            }
            else if (cmpres > 0) {
                if (leaf->right != NULL)
                    return insert(key, keystr, keylen, valstr, vallen, leaf->right);
                else {
                    int8_t rc;

                    leaf->right = new node;
                    if (!leaf->right) return DICTIONARY_MEM;
                    rc = leaf->right->create(keystr, keylen, valstr, vallen, NULL, NULL);
                    if (rc) {
                        delete leaf->right;
                        return rc;
                    }
                    rc = Q->append(leaf->right);
                    if (rc) {
                        delete leaf->right;
                        return rc;
                    }
                }
            }
        }
    }
    return DICTIONARY_OK;
}


// ==== SEARCH ===========================================================================
node* Dictionary::search(uintNN_t key, node* leaf, const char* keystr, _DICT_KEY_TYPE keylen) {
    if (leaf != NULL) {
        if ( key == leaf->key() ) {
            int cmpres = keylen != leaf->ksize ? keylen - leaf->ksize : memcmp(leaf->keybuf, keystr, keylen);
#ifdef _LIBDEBUG_
            Serial.println("DICT-SEARCH:");
            Serial.printf("key    = %u, leaf-key   = %u\n", (uint32_t)key, (uint32_t)leaf->key());
            Serial.printf("keylen = %u, leaf-ksize = %u\n", keylen, leaf->ksize);
            Serial.printf("cmpres = %d, memcmp     = %d\n", cmpres, memcmp(leaf->keybuf, keystr, keylen));
            printNode(leaf);
#endif
            if (cmpres == 0) {
                return leaf;
            }
            else {
                if ( cmpres < 0 )
                    return search(key, leaf->left, keystr, keylen);
                else
                    return search(key, leaf->right, keystr, keylen);
            }
        }
        else {
            if ( key < leaf->key() )
                return search(key, leaf->left, keystr, keylen);
            else
                return search(key, leaf->right, keystr, keylen);
        }
    }
    else return NULL;
}


// ==== DELETES ==========================================================================
void Dictionary::destroy_tree(node* leaf) {
  if (leaf != NULL) {
    destroy_tree(leaf->left);
    destroy_tree(leaf->right);
    delete leaf; // node destructor takes care of the key and value strings
    leaf = NULL;
  }
}


node* Dictionary::deleteNode(node* root, uintNN_t key, const char* keystr, _DICT_KEY_TYPE keylen) {
  if (root == NULL) return root;

  if (key < root->key() ) {
    root->left = deleteNode(root->left, key, keystr, keylen);
  }
  // If the key to be deleted is greater than the root's key,
  // then it lies in right subtree
  else if (key > root->key() ) {
    root->right = deleteNode(root->right, key, keystr, keylen);
  }
  // if key is same as root's key, then This is the node
  // to be deleted
  else {
    int cmpres = (keylen != root->ksize) ? keylen - root->ksize : memcmp(root->keybuf, keystr, keylen);
    if (cmpres == 0 ) {
      // node with only one child or no child
      if (root->left == NULL) {
#ifdef _LIBDEBUG_
        Serial.println("Replacing RIGHT node");
        printNode(root);
        printNode(root->right);
#endif
        node* temp = root->right;
        Q->remove(root);
        delete root;
        root = NULL;
        return temp;
      }
      else if (root->right == NULL) {
#ifdef _LIBDEBUG_
        Serial.println("Replacing LEFT node");
        printNode(root);
        printNode(root->left);
#endif
        node* temp = root->left;
        Q->remove(root);
        delete root;
        root = NULL;
        return temp;
      }

      // node with two children: Get the in-order successor (smallest
      // in the right subtree)
      node* temp = minValueNode(root->right);
#ifdef _LIBDEBUG_
      Serial.println("Replacing minValueNode node");
      printNode(root);
      printNode(temp);
#endif

      // Copy the in-order successor's content to this node
      root->updateKey(temp->keybuf, temp->ksize);
      root->updateValue(temp->valbuf, temp->vsize);

      // Delete the in-order successor
      root->right = deleteNode(root->right, temp->key(), temp->keybuf, temp->ksize);
    }
    else {
        if ( cmpres < 0 ) {
            root->left = deleteNode(root->left, key, keystr, keylen);
        }
        // If the key to be deleted is greater than the root's key,
        // then it lies in right subtree
        else if ( cmpres > 0 ) {
            root->right = deleteNode(root->right, key, keystr, keylen);
        }
    }
  }
  return root;
}


node* Dictionary::minValueNode(node* n) {
  node* current = n;

  /* loop down to find the leftmost leaf */
  while (current && current->left) current = current->left;

  return current;
}


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

#endif // #define _DICTIONARY_H_






