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
*/

#pragma once

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
#include <unordered_map>

#ifdef _DICT_PACK_STRUCTURES
class __attribute((__packed__)) Dictionary {
#else
class Dictionary {
#endif
  public:
    Dictionary(size_t init_size = 10);
    ~Dictionary();

    std::unordered_map<std::string, std::string> dict;    // yes, map is public, 'cause it might be convenient to access it directly for iterations, etc...

    /**
     * @brief - creates new, or updates existing key:value pair
     * this does NOT match with underlaying std::unordered_map::insert, which only inserts new vals
     * This is for compatibility with Dictionary API. 
     */
    inline int8_t       insert(const String& keystr, const String& valstr){return insert(keystr.c_str(), valstr.c_str());};
    int8_t       insert(const char* keystr, const char* valstr);

    /**
     * @brief - creates new key:value pair, does nothing  if key already exist
     * this method could be used instead of insert() and it also matches std::unordered_map::implace
     */
    int8_t              emplace(const char* keystr, const char* valstr);
    inline int8_t       emplace(const String& keystr, const String& valstr){return emplace(keystr.c_str(), valstr.c_str());};

    /**
     * @brief - update existing key with a new val, do nothing if key does not exist
     */
    int8_t              update(const char* keystr, const char* valstr);
    inline int8_t       update(const String& keystr, const String& valstr){return update(keystr.c_str(), valstr.c_str());};

    inline String       search(const String& keystr){return search(keystr.c_str());};
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
    inline int8_t       jload (const String& json, int num = 0);
    int8_t              jload (const char* json, int num = 0);
    int8_t              merge (Dictionary& source);

    void operator = (Dictionary& dict) {
      destroy();
      merge(dict);
    }

    inline String operator [] (const String& keystr) { return search(keystr); }
    inline String operator [] (size_t i) { return value(i); }
    inline int8_t operator () (const String& keystr, const String& valstr) { return insert(keystr, valstr); }
    inline int8_t operator () (const char* keystr, const char* valstr) { return insert(keystr, valstr); }

    bool operator () (const String& keystr);

    String operator () (size_t i) { return key(i); }
    bool operator == (Dictionary& b);
    inline bool operator != (Dictionary& b) { return (!(*this == b)); }
    inline size_t count() { return ( dict.size()); }

#ifdef _LIBDEBUG_
    void printDictionary() {
      Serial.printf("\nDictionary::printDictionary:\n");
        for (const auto &p : dict) {
            // need to decompress fisrt?
            Serial.printf("key=%s\tvalue=%s\n", p.first.c_str(), p.second.c_str());
        }
    };
    void printArray() {
      //Q->printArray();
    };
#endif

  private:
// methods

    /**
     *  @brief insert new key:val or update existing one with a new val
     *  key:val should be already (de)compressed
     */
    inline int8_t         set(const char* keystr, const char* valstr){ dict[keystr] = valstr; return DICTIONARY_OK; };  // TODO: checks/asserts?

    uintNN_t            crc(const void* data, size_t n_bytes);

#ifdef _DICT_COMPRESS
    int8_t              compressKey(const char* aStr);
    int8_t              compressValue(const char* aStr);
    void                decompressKey(const char* aBuf, _DICT_KEY_TYPE aLen);
    void                decompressValue(const char* aBuf, _DICT_VAL_TYPE aLen);
#endif

// data
    size_t              initSize;

    char*            iKeyTemp;    // (de)compressed key goes here
    _DICT_KEY_TYPE      iKeyLen;
    char*            iValTemp;    // (de)compressed value goes here
    _DICT_VAL_TYPE      iValLen;
};
