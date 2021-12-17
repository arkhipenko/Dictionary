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

  v3.2.3:
    2021-02-22 - update: added ability to ignore non-ascii characters (#define _DICT_ASCII_ONLY)
   
  v3.3.0:
    2021-05-27 - update: json import does not require quotation marks (still creates strings)
   
 */


#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

#include <Arduino.h>

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

#define NODEARRAY_OK    0
#define NODEARRAY_ERR   (-1)
#define NODEARRAY_MEM   (-2)

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
#include "BufferStream/BufferStream.h"


#ifdef _DICT_PACK_STRUCTURES
class __attribute((__packed__)) node {
#else
class node {
#endif
  public:

    void* operator new(size_t size) {

      void* p = NULL;
      if ( size ) {
#if defined (ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
        if (psramFound()) {
          p = ps_malloc(size);
        }
#endif
        if (!p) p = malloc(size);
#ifdef _LIBDEBUG_
        Serial.printf("NODE-NEW: size=%d (%d) k/v sizes=%d, %d, ptr=%u\n", size, sizeof(node), sizeof(_DICT_KEY_TYPE), sizeof(_DICT_VAL_TYPE), (uint32_t)p);
#endif    
      }
      return p;
    }

    void operator delete(void* p) {
      if ( p == NULL ) return;
      node* n = (node*)p;

      // Delete key/value strings
      if ( n->keybuf ) { 
        free(n->keybuf);
        n->keybuf = NULL;
      }
      if ( n->valbuf ) {
          free(n->valbuf);
          n->valbuf = NULL;
      }
      free(p);
#ifdef _LIBDEBUG_
      Serial.printf("NODE-DELETE: Freed memory block %u\n", (uint32_t)p);
#endif    
    }

    uintNN_t    key() {
        uintNN_t k = 0;
        
        memcpy((void*)&k, keybuf, ksize < sizeof(uintNN_t) ? ksize : sizeof(uintNN_t));
        return k;
    }
    
    int8_t      create(const char* aKey, _DICT_KEY_TYPE aKeySize, const char* aVal, _DICT_VAL_TYPE aValSize, node* aLeft, node* aRight);
    int8_t      updateValue(const char* aVal, _DICT_VAL_TYPE aValSize);
    int8_t      updateKey(const char* aKey, _DICT_KEY_TYPE aKeySize);

#ifdef _LIBDEBUG_
    void printNode();
#endif
    char*           keybuf;
    _DICT_KEY_TYPE  ksize;
    char*           valbuf;
    _DICT_VAL_TYPE  vsize;
    node*           left;
    node*           right;
};

#ifdef _DICT_PACK_STRUCTURES
class __attribute((__packed__)) NodeArray {
#else
class NodeArray {
#endif   
  public:
    // init the queue (constructor).
    NodeArray(size_t init_size = 10);

    // clear the queue (destructor).
    ~NodeArray();

    // add an item to the queue.
    int8_t append(const node* i);

    // remove an item from the queue.
    void remove(const node* i);

    // check if the queue is empty.
    bool isEmpty() const;

    //    // get the number of items in the queue.
    size_t count() const;

    // check if the queue is full.
    bool isFull() const;


    node* operator [] (const size_t i) {
      if (i >= items) {
        //        exit ("QUEUE: Out of bounds");
        return NULL;
      }
      return contents[i];
    }

#ifdef _LIBDEBUG_
    void printArray();
#endif

  private:
    // resize the size of the queue.
    int8_t resize(const size_t s);

    // exit report method in case of error.
    //    void exit (const char * m) const;

    // the initial size of the queue.
    size_t initialSize;

    node** contents;    // the array of the queue.

    size_t size;        // the size of the queue.
    size_t items;       // the number of items of the queue.
    size_t tail;        // the tail of the queue.
};



#ifdef _DICT_PACK_STRUCTURES
class __attribute((__packed__)) Dictionary {
#else
class Dictionary {
#endif
  public:
    Dictionary(size_t init_size = 10);
    ~Dictionary();

    inline int8_t       insert(String keystr, int32_t val) { return insert( keystr, String(val) ); }
    inline int8_t       insert(String keystr, float   val) { return insert( keystr, String(val) ); }
    inline int8_t       insert(String keystr, double  val) { return insert( keystr, String(val) ); }
    inline int8_t       insert(String keystr, String  valstr)  { return insert( keystr.c_str(), valstr.c_str() ); }
    int8_t              insert(const char* keystr, const char* valstr);
    
    inline String       search(const String& keystr) { return search(keystr.c_str()); }
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
    int8_t              jload (Stream& json, int aNum = 0);
    int8_t              merge (Dictionary& dict);


    void operator = (Dictionary& dict) {
      destroy();
      merge(dict);
    }

    inline String operator [] (const String& keystr) { return search(keystr); }
    inline String operator [] (size_t i) { return value(i); }
    inline int8_t operator () (String keystr, int32_t val) { return insert(keystr, val); }
    inline int8_t operator () (String keystr, float val) { return insert(keystr, val); }
    inline int8_t operator () (String keystr, double val) { return insert(keystr, val); }
    inline int8_t operator () (String keystr, String valstr) { return insert(keystr, valstr); }
    inline int8_t operator () (const char* keystr, const char* valstr) { return insert(keystr, valstr); }

    bool operator () (const String& keystr);

    String operator () (size_t i) { return key(i); }
    bool operator == (Dictionary& b);
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
    NodeArray*          Q;
    size_t              initSize;

    char*               iKeyTemp;
    _DICT_KEY_TYPE      iKeyLen;
    char*               iValTemp;
    _DICT_VAL_TYPE      iValLen;
};

#endif // #define _DICTIONARY_H_






