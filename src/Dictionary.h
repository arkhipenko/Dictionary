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



#define DICTIONARY_OK   0
#define DICTIONARY_ERR   (-1)
#define DICTIONARY_MEM   (-2)

#define DICTIONARY_COMMA    (-20)
#define DICTIONARY_COLON    (-21)
#define DICTIONARY_QUOTE    (-22)
#define DICTIONARY_BCKSL    (-23)
#define DICTIONARY_EOF      (-99)

#ifndef _DICT_CRC
#define _DICT_CRC  32
#endif

#if !( _DICT_CRC == 16 || _DICT_CRC == 32 || _DICT_CRC == 64)
#define _DICT_CRC  32
#endif

#if _DICT_CRC == 16
#define uintNN_t uint16_t
#define _DICT_LEN   2
#endif

#if _DICT_CRC == 32
#define uintNN_t uint32_t
#define _DICT_LEN   4
#endif

#if _DICT_CRC == 64
#define uintNN_t uint64_t
#define _DICT_LEN   8
#endif

#include <Arduino.h>
#include "NodeArray.h"

class Dictionary {
  public:
    Dictionary(size_t init_size = 10);
    ~Dictionary();

    int8_t insert(String keystr, String valstr);
    int8_t insert(const char* keystr, const char* valstr);
    String search(String keystr);
    String search(const char* keystr);
    void destroy();
    int8_t remove(String keystr);
    size_t size();
    size_t jsize();
    String json();
    int8_t jload (String json, int num=0);
    int8_t merge (Dictionary& dict);
    
    void operator = (Dictionary& dict) {
        destroy();
        merge(dict);
    }
    
    String key(size_t i) {
      if (Q) {
        node* p = (*Q)[i];
        if (p) return String(p->keystr);
      }
      return String();
    }
    String value(size_t i) {
      if (Q) {
        node* p = (*Q)[i];
        if (p) return String(p->valstr);
      }
      return String();
    }
    String operator [] (String keystr) {
      return search(keystr);
    }
    String operator [] (size_t i) {
      return value(i);
    }
    int8_t operator () (String keystr, String valstr) {
      return insert(keystr, valstr);
    }

    bool operator () (String keystr);

    String operator () (size_t i) {
      return key(i);
    }
    bool operator == (Dictionary& b);
    inline bool operator != (Dictionary& b) {
      return (!(*this == b));
    }
    inline const size_t count() {
      return ( Q ? Q->count() : 0);
    }

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
    void     destroy_tree(node* leaf);
    int8_t   insert(uintNN_t key, const char* keystr, const char* valstr, node* leaf);
    node*    search(uintNN_t key, node* leaf, const char* keystr);
    uintNN_t crc(const void* data, size_t n_bytes);

    node*    deleteNode(node* root, uintNN_t key, const char* keystr);
    node*    minValueNode(node* n);

    node*               iRoot;
    NodeArray*          Q;
    size_t              initSize;
};

Dictionary::Dictionary(size_t init_size) {
  iRoot = NULL;

  // This is unlikely to fail as practically no memory is allocated by the NodeArray
  // All memory allocation is delegated to the first append
  Q = new NodeArray(init_size);
  initSize = init_size;
}


Dictionary::~Dictionary() {
  destroy();
  delete Q;
}

void Dictionary::destroy_tree(node* leaf) {
  if (leaf != NULL) {
    destroy_tree(leaf->left);
    destroy_tree(leaf->right);
    delete leaf; // node destructor takes care of the key and value strings
    leaf = NULL;
  }
}


int8_t Dictionary::insert(uintNN_t key, const char* keystr, const char* valstr, node* leaf) {
  if ( key < leaf->key() ) {
    if (leaf->left != NULL)
      return insert(key, keystr, valstr, leaf->left);
    else {
      int8_t rc;

      leaf->left = new node;
      if (!leaf->left) return DICTIONARY_MEM;
      rc = leaf->left->create(keystr, valstr, NULL, NULL);
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
      return insert(key, keystr, valstr, leaf->right);
    else {
      int8_t rc;

      leaf->right = new node;
      if (!leaf->right) return DICTIONARY_MEM;
      rc = leaf->right->create(keystr, valstr, NULL, NULL);
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
    if (strcmp(leaf->keystr, keystr) == 0) {
      if (leaf->updateValue(valstr) != NODEARRAY_OK) return DICTIONARY_MEM;
    }
    else {
      if (leaf->right != NULL)
        return insert(key, keystr, valstr, leaf->right);
      else {
        int8_t rc;

        leaf->right = new node;
        if (!leaf->right) return DICTIONARY_MEM;
        rc = leaf->right->create(keystr, valstr, NULL, NULL);
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
  return DICTIONARY_OK;
}


bool Dictionary::operator () (String keystr) {
  if ( keystr.length() > _DICT_KEYLEN ) return false;

  uintNN_t key = crc(keystr.c_str(), keystr.length());

  node* p = search(key, iRoot, keystr.c_str());
  if (p) return true;
  return false;
}


node* Dictionary::search(uintNN_t key, node* leaf, const char* keystr) {
  if (leaf != NULL) {
    if ( key == leaf->key() && strcmp(leaf->keystr, keystr) == 0 )
      return leaf;
    if ( key < leaf->key() )
      return search( key, leaf->left, keystr );
    else
      return search( key, leaf->right, keystr );
  }
  else return NULL;
}


int8_t Dictionary::insert(String keystr, String valstr) {
  return insert(keystr.c_str(), valstr.c_str());
}

int8_t Dictionary::insert(const char* keystr, const char* valstr) {
  // TODO: decide if to check for length here
  size_t l = strnlen(keystr, _DICT_KEYLEN + 1);

  if ( l > _DICT_KEYLEN ) return DICTIONARY_ERR;
  if ( strnlen(valstr, _DICT_VALLEN + 1) > _DICT_VALLEN ) return DICTIONARY_ERR;

  uintNN_t key = crc(keystr, l);

  if (iRoot != NULL)
    return insert(key, keystr, valstr, iRoot);
  else {
    int8_t rc;

    iRoot = new node;
    if (!iRoot) return DICTIONARY_MEM;
    rc = iRoot->create(keystr, valstr, NULL, NULL);

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


String Dictionary::search(String keystr) {
  return search(keystr.c_str());
}

String Dictionary::search(const char* keystr) {
  size_t l = strnlen(keystr, _DICT_KEYLEN + 1);

  if (l <= _DICT_KEYLEN) {
    uintNN_t key = crc(keystr, l);

    node* p = search(key, iRoot, keystr);
    if (p) return String(p->valstr);
  }
  return String("");
}


void Dictionary::destroy() {
  destroy_tree(iRoot);
  iRoot = NULL;
  delete Q;
  Q = new NodeArray(initSize);
}


size_t Dictionary::size() {
  size_t ct = count();
  size_t sz = 0;
  for (size_t i = 0; i < ct; i++) {
    sz += key(i).length() + 1;
    sz += value(i).length() + 1;
    sz += sizeof(node);  // to account for size of the node itself
  }
  return sz;
}


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


String Dictionary::json() {
    String s;
    
    s.reserve( jsize() );
    s = '{';
    
    size_t ct = count();
    for (size_t i = 0; i < ct; i++) {
        s += '"' + key(i) + "\":\"" + value(i) + '"';
        if ( i < ct-1 ) s += ',';
    }
    s += '}';
    
    return s;
}


int8_t Dictionary::merge (Dictionary& dict) {
    size_t ct = dict.count();
    int8_t rc;
    
    for (size_t i = 0; i < ct; i++) {
        rc = insert( dict(i), dict[i] );
        if (rc) return rc;
    }
    return DICTIONARY_OK;
}


int8_t Dictionary::jload (String json, int num){
    bool insideQoute = false;
    bool nextVerbatim = false;
    bool isValue = false;
    const char* c = json.c_str();
    size_t len = json.length();
    int p = 0;
    String currentKey;
    String currentValue;

    for (size_t i = 0; i < len; i++, c++) {
        if (nextVerbatim) {
            nextVerbatim = false;
        }
        else {
            // process all special cases: '\', '"', ':', and ','
            if (*c == '\\') {
                nextVerbatim = true;
                continue;
            }
            if (*c == '\"') {
                if (!insideQoute) {
                    insideQoute = true;
                    continue;
                }
                else {
                    insideQoute = false;
                    if (isValue) {
                        insert(currentKey, currentValue);
                        currentValue = String();
                        currentKey = String();
                        if (num > 0 && p >= num) break;
                    }
                }
            }
            if (*c == '\n') {
                if (insideQoute) {
                    return DICTIONARY_QUOTE;
                }
                if (nextVerbatim) {
                    return DICTIONARY_BCKSL;
                }
            }
            if (!insideQoute) {
                if (*c == ':') {
                    if (isValue) {
                        return DICTIONARY_COMMA; //missing comma probably
                    }
                    isValue = true;
                    continue;
                }
                if (*c == ',') {
                    if (!isValue) {
                        return DICTIONARY_COLON; //missing colon probably
                    }
                    isValue = false;
                    continue;
                }
            }
        }
        if (insideQoute) {
            if (isValue) currentValue.concat(*c);
            else currentKey.concat(*c);
        }
    }
    if (insideQoute || nextVerbatim || (num > 0 && p < num)) return DICTIONARY_EOF;
    return DICTIONARY_OK;
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


int8_t Dictionary::remove(String keystr) {
#ifdef _LIBDEBUG_
  Serial.printf("Dictionary::remove: %s\n", keystr.c_str());
#endif
  if (keystr.length() > _DICT_KEYLEN) return DICTIONARY_ERR;

  uintNN_t key = crc(keystr.c_str(), keystr.length());
  node* p = search(key, iRoot, keystr.c_str());

  if (p) {
#ifdef _LIBDEBUG_
    Serial.printf("Found key to delete int: %u\n", p->key());
    Serial.printf("Found key to delete ptr: %u\n", (uint32_t)p);
    Serial.printf("Found key to delete str: %s\n", keystr.c_str());
#endif
    iRoot = deleteNode(iRoot, p->key(), keystr.c_str());
  }
  return DICTIONARY_OK;
}


node* Dictionary::deleteNode(node* root, uintNN_t key, const char* keystr) {
  if (root == NULL) return root;

  if (key < root->key() ) {
    root->left = deleteNode(root->left, key, keystr);
  }
  // If the key to be deleted is greater than the root's key,
  // then it lies in right subtree
  else if (key > root->key() ) {
    root->right = deleteNode(root->right, key, keystr);
  }
  // if key is same as root's key, then This is the node
  // to be deleted
  else {
    if ( strcmp(root->keystr, keystr) == 0 ) {
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
      root->updateKey(/*temp->key, */temp->keystr);
      root->updateValue(temp->valstr); 
      
      // Delete the in-order successor
      root->right = deleteNode(root->right, temp->key(), temp->keystr);
    }
    else {
      root->right = deleteNode(root->right, key, keystr);
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
    Serial.printf("%u: (%u:%s,%s %u:%u) [l:%u, r:%u]\n", (uint32_t)root, root->key(), root->keystr, root->valstr, (uint32_t)root->keystr, (uint32_t)root->valstr, (uint32_t)root->left, (uint32_t)root->right);
  }
  else {
    Serial.println("NULL:");
  }
}
#endif


uintNN_t Dictionary::crc(const void* data, size_t n_bytes) {
    uintNN_t    a;
    size_t      sz = sizeof(uintNN_t);
    
    size_t      n = n_bytes < sz ? n_bytes : sz;
    
    memset( (void*) &a, 0, sz );
    memcpy( (void*) &a, data, n );
    
    return a;
}


#endif // #define _DICTIONARY_H_






