/*
  Implementation of the Dictionary data type
  for key-value pairs: an insertion-ordered entry array
  with an optional hash index, typed values and optional value compression

  ---

  Copyright (c) 2020-2026 Anatoli Arkhipenko
  All rights reserved.

  Distributed under the BSD 3-Clause License. See LICENSE.txt.

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

  v3.6.0:
    2026-07-17 - update: PlatformIO / non-Arduino-IDE builds now supported via the
                 bundled Dictionary.cpp gated by #define _DICT_HEADER_AND_CPP,
                 mirroring the TaskScheduler approach. Include DictionaryDeclarations.h
                 in your code and define _DICT_HEADER_AND_CPP as a build flag; no need
                 to hand-create a Dictionary.cpp anymore.
               - bug fix: node::create left valbuf uninitialized on a failed key
                 allocation, so a subsequent delete freed a garbage pointer (crash
                 on out-of-memory). Both buffers are now cleared up front.
               - bug fix: deleteNode ignored updateKey/updateValue failures while
                 promoting the in-order successor, corrupting the tree on OOM. It
                 now uses an atomic node::updateKeyValue and remove() returns the
                 error code.
               - bug fix: on a failed NodeArray append the new child was deleted but
                 still linked into the tree (dangling pointer). It is now linked only
                 after the append succeeds.
               - bug fix: on a failed root-node creation/append, insert() deleted the
                 node but left iRoot dangling, so the next insert dereferenced freed
                 memory. iRoot is now reset to NULL on that failure path.
               - test: added native Google Test suites (tests/) and CI (unit tests +
                 ASan/UBSan + example builds), mirroring the TaskScheduler harness.
               - update: search/insert/deleteNode/destroy are now iterative to avoid
                 stack overflow on a deep/unbalanced tree (e.g. sorted-order inserts).
               - update: NodeArray grows geometrically (was a fixed increment).
               - update: json() now escapes '"' and '\' in both keys and values.
               - update: read operations no longer mutate node buffers (non-compressed
                 builds); jsize()/esize() read node sizes directly.

  v3.6.1:
    2026-09-26 - bug fix: the implicit copy constructor made a shallow copy, so
                 Dictionary b(a) or passing a Dictionary by value double freed. Copy
                 construction is now deleted (a constructor cannot report an allocation
                 failure); move construction and move assignment are supported.
               - bug fix: d = d emptied the dictionary. Assignment now ignores
                 self-assignment and returns Dictionary& (was void).
               - bug fix: a compressed key or value that did not fit had its length
                 truncated to the length type (256 became 0 with default settings) and
                 was stored empty with a success code. The full-width length is now
                 checked first and DICTIONARY_OOB returned. A compressed form must fit
                 in _DICT_KEYLEN / _DICT_VALLEN bytes (was one byte more on insert only).
               - bug fix: split-header builds (_DICT_HEADER_AND_CPP) failed to link a call
                 to remove(const String&), which was declared inline.
               - bug fix: compressed builds did not check the scratch-buffer allocations.
                 They are now allocated on first use and a failure returns an error.
               - bug fix: destroy() allocated a new NodeArray and crashed on the next insert
                 if that failed. NodeArray is now a member of Dictionary; neither the
                 constructor nor destroy() allocates.
               - bug fix: node::operator new returned NULL without being noexcept
                 (undefined behavior). It is now noexcept, and node frees its buffers in
                 a destructor instead of in operator delete.


  v4.0.0:
    2026-09-26 - redesign. Storage: one insertion-ordered array of entries, each entry a
                 single allocation holding type, lengths, key and value, plus an optional
                 hash index (open addressing) built once _DICT_INDEX_MIN pairs are stored.
                 The binary tree, NodeArray, key-prefix integer and _DICT_CRC are gone.
                 remove() keeps the order of the remaining pairs and never allocates.
               - feature: typed values (string, int32, float, bool, null; int64 and double
                 with _DICT_WIDE_NUMBERS) with set()/getInt()/getFloat()/getBool()/
                 getString()/peek()/has()/type(). Typed getters convert text on read unless
                 _DICT_STRICT_GET. jload() stores values as given; _DICT_TYPED_JSON stores
                 bare numbers, true/false and null typed. json() writes typed values unquoted.
               - feature: one built-in value codec (_DICT_COMPRESS), values only, applied only
                 when it makes the value smaller, no scratch buffers. SHOCO and SMAZ removed.
               - feature: jload() rewritten: empty values, trailing comments, a last pair
                 without a separator, standard escapes (including \uXXXX), token length cap,
                 nested objects/arrays rejected, insert errors propagated, _DICT_ASCII_ONLY
                 fixed. BufferStream removed (jload reads a Stream directly).
               - feature: reserve(n), json(Print&), keyAt()/typeAt(), move semantics, const
                 read methods, jsize() is exact.
               - deprecated (compile-time warning, silenced by _DICT_NO_DEPRECATION_WARNINGS):
                 insert(), search(), d(key, value), d(key), d(i), d[i]. They keep 3.x behavior.
               - removed build flags, accepted with #warning: _DICT_CRC, _DICT_PACK_STRUCTURES,
                 _DICT_COMPRESS_SHOCO and _DICT_COMPRESS_SMAZ (both now mean _DICT_COMPRESS).
               - license: BSD 3-Clause in every file, as in LICENSE.txt (this header used to
                 say GPL v3).
               - example: Dict_Benchmark, an on-device benchmark of every operation.

 */


// The following "define" controls how the library is compiled and should be set
// as a build flag (e.g. platformio.ini build_flags), NOT in the sketch, because
// the bundled Dictionary.cpp is a separate translation unit:
//
// #define _DICT_HEADER_AND_CPP   // PlatformIO style: separate header and CPP file.
//                                // Include <DictionaryDeclarations.h> in your code;
//                                // the bundled Dictionary.cpp compiles the
//                                // implementation exactly once. Without this define
//                                // (Arduino IDE style) just #include <Dictionary.h>.

#ifndef _DICTIONARYDECLARATIONS_H_
#define _DICTIONARYDECLARATIONS_H_

#include <Arduino.h>


// ==== 3.x build flags that no longer apply =====================================
#ifdef _DICT_CRC
#warning "Dictionary 4.0: _DICT_CRC has no effect (keys are hashed) and is ignored"
#endif

#ifdef _DICT_PACK_STRUCTURES
#warning "Dictionary 4.0: _DICT_PACK_STRUCTURES has no effect (entries are byte-packed) and is ignored"
#endif

#if defined(_DICT_COMPRESS_SHOCO) || defined(_DICT_COMPRESS_SMAZ)
#warning "Dictionary 4.0: SHOCO and SMAZ were removed; this flag now enables the built-in value codec (_DICT_COMPRESS)"
#ifndef _DICT_COMPRESS
#define _DICT_COMPRESS
#endif
#endif


// ==== Limits ===================================================================
// Maximum key and value text lengths in bytes. The length fields use the
// smallest unsigned type that holds them.
#ifndef _DICT_KEYLEN
#define _DICT_KEYLEN 64
#endif

#ifndef _DICT_VALLEN
#define _DICT_VALLEN 254
#endif

#if _DICT_KEYLEN < 1
#error "_DICT_KEYLEN must be at least 1"
#endif

#if _DICT_KEYLEN <= 255
#define _DICT_KLEN_T  uint8_t
#elif _DICT_KEYLEN <= 65535
#define _DICT_KLEN_T  uint16_t
#else
#define _DICT_KLEN_T  uint32_t
#endif

// The value length field also holds the size of a numeric payload (up to 8).
#if _DICT_VALLEN <= 255
#define _DICT_VLEN_T  uint8_t
#elif _DICT_VALLEN <= 65535
#define _DICT_VLEN_T  uint16_t
#else
#define _DICT_VLEN_T  uint32_t
#endif

// Maximum number of pairs. Selects the width of a hash index slot.
#ifndef _DICT_MAX_ENTRIES
#define _DICT_MAX_ENTRIES 65534
#endif

#if _DICT_MAX_ENTRIES <= 65534
#define _DICT_SLOT_T  uint16_t
#else
#define _DICT_SLOT_T  uint32_t
#endif

// Pair count at which the hash index is built. 0 = never (lookups scan).
#ifndef _DICT_INDEX_MIN
#define _DICT_INDEX_MIN 32
#endif

// Decimal places used when a float is rendered as text (trailing zeros trimmed).
#ifndef _DICT_FLOAT_DECIMALS
#define _DICT_FLOAT_DECIMALS 6
#endif

#ifdef _DICT_NO_DEPRECATION_WARNINGS
#define _DICT_DEPRECATED(msg)
#else
#define _DICT_DEPRECATED(msg) __attribute__((deprecated(msg)))
#endif


// ==== Result codes ==============================================================
#define DICTIONARY_OK         0
#define DICTIONARY_ERR      (-1)    // invalid argument (e.g. key or value length)
#define DICTIONARY_MEM      (-2)    // memory allocation failed
#define DICTIONARY_OOB      (-3)    // does not fit (token longer than the limit, too many pairs)

#define DICTIONARY_COMMA    (-20)   // jload: expected a separator after a value
#define DICTIONARY_COLON    (-21)   // jload: expected ':' after a key
#define DICTIONARY_QUOTE    (-22)   // jload: newline inside a quoted string
#define DICTIONARY_BCKSL    (-23)   // jload: invalid escape sequence
#define DICTIONARY_FMT      (-25)   // jload: malformed input
#define DICTIONARY_EOF      (-99)   // jload: input ended early


// ==== Value types ===============================================================
enum DictType : uint8_t {
  DICT_NONE   = 0,    // key not present
  DICT_STR    = 1,    // text
  DICT_INT    = 2,    // int32_t
  DICT_FLOAT  = 3,    // float
  DICT_BOOL   = 4,
  DICT_NULL   = 5,
  DICT_INT64  = 6,    // int64_t  (_DICT_WIDE_NUMBERS)
  DICT_DOUBLE = 7     // double   (_DICT_WIDE_NUMBERS)
};


class Dictionary {
  public:
    // A key given as const char* or String. Lets every method accept both
    // without doubling its overloads; the pointer is only used during the call.
    class Key {
      public:
        Key(const char* s) : p(s) {}
        Key(const String& s) : p(s.c_str()) {}
        // Flash keys are not supported (they are not addressable as RAM on every
        // core). Use String(F("key")).
        Key(const __FlashStringHelper* s) = delete;
        const char* p;
    };

    explicit Dictionary(size_t init_size = 10);
    ~Dictionary();

    // Copy construction is not supported: a constructor cannot report an
    // allocation failure. Copy with assignment or merge(), which can.
    Dictionary(const Dictionary&) = delete;
    Dictionary(Dictionary&& other) noexcept;
    Dictionary& operator = (Dictionary&& other) noexcept;
    Dictionary& operator = (const Dictionary& other);   // errors are not reported; see merge()

    // ---- write --------------------------------------------------------------
    // Insert or replace. Return DICTIONARY_OK (0) or a negative code.
    int8_t      set(Key key, const char* value);          // NULL stores a null value
    int8_t      set(Key key, const String& value)         { return set(key, value.c_str()); }
    int8_t      set(Key key, const __FlashStringHelper* value) { return set(key, String(value)); }   // not bool
    int8_t      set(Key key, bool value);
    int8_t      set(Key key, signed char value)           { return setSigned(key, value); }
    int8_t      set(Key key, unsigned char value)         { return setUnsigned(key, value); }
    int8_t      set(Key key, short value)                 { return setSigned(key, value); }
    int8_t      set(Key key, unsigned short value)        { return setUnsigned(key, value); }
    int8_t      set(Key key, int value)                   { return setSigned(key, value); }
    int8_t      set(Key key, unsigned int value)          { return setUnsigned(key, value); }
    int8_t      set(Key key, long value)                  { return setSigned(key, value); }
    int8_t      set(Key key, unsigned long value)         { return setUnsigned(key, value); }
    int8_t      set(Key key, long long value)             { return setSigned(key, value); }
    int8_t      set(Key key, unsigned long long value)    { return setUnsigned(key, value); }
    int8_t      set(Key key, float value);
    int8_t      set(Key key, double value);
    int8_t      setNull(Key key);

    int8_t      remove(Key key);                          // a missing key is not an error
    void        destroy();                                // remove everything, free all memory
    int8_t      reserve(size_t n);                        // pre-allocate for n pairs
    int8_t      merge(const Dictionary& other);           // copy every pair of other into this

    int8_t      jload(const char* json, int n = 0);       // n > 0: load at most n pairs
    int8_t      jload(const String& json, int n = 0)      { return jload(json.c_str(), n); }
    int8_t      jload(Stream& json, int n = 0);

    // ---- read (a missing key or an unconvertible value returns def) ------------
    size_t      count() const                             { return iCount; }
    bool        has(Key key) const;
    DictType    type(Key key) const;                      // DICT_NONE when missing

    int32_t     getInt(Key key, int32_t def = 0) const;
    float       getFloat(Key key, float def = 0) const;
    bool        getBool(Key key, bool def = false) const;
#ifdef _DICT_WIDE_NUMBERS
    int64_t     getInt64(Key key, int64_t def = 0) const;
    double      getDouble(Key key, double def = 0) const;
#endif
    String      getString(Key key, const char* def = "") const;   // any type as text
    size_t      getString(Key key, char* buf, size_t size) const; // no heap; returns full length
    const char* peek(Key key) const;                      // zero-copy plain text, else NULL

    // Positional access, in insertion order (preserved by remove).
    const char* keyAt(size_t i) const;                    // zero-copy; NULL when out of range
    DictType    typeAt(size_t i) const;
    String      key(size_t i) const;
    String      value(size_t i) const;

    String      json() const;
    size_t      json(Print& out) const;                   // stream it; returns bytes written
    size_t      jsize() const;                            // json() length + 1
    size_t      esize() const;                            // sum of key + 1 + value text + 1
    size_t      size() const;                             // heap bytes requested (entries, array, index)

    String      operator [] (Key key) const               { return getString(key); }
    bool        operator == (const Dictionary& b) const;
    bool        operator != (const Dictionary& b) const   { return !(*this == b); }

    // ---- 3.x API, kept for compatibility (3.x behavior: values stored as text) ----
    _DICT_DEPRECATED("use set()")         int8_t insert(Key key, const char* value)    { return set(key, value); }
    _DICT_DEPRECATED("use set()")         int8_t insert(Key key, const String& value)  { return set(key, value); }
    _DICT_DEPRECATED("use set()")         int8_t insert(Key key, int32_t value)        { return set(key, String(value)); }
    _DICT_DEPRECATED("use set()")         int8_t insert(Key key, float value)          { return set(key, String(value)); }
    _DICT_DEPRECATED("use set()")         int8_t insert(Key key, double value)         { return set(key, String(value)); }
    _DICT_DEPRECATED("use set()")         int8_t operator () (Key key, const char* value)   { return set(key, value); }
    _DICT_DEPRECATED("use set()")         int8_t operator () (Key key, const String& value) { return set(key, value); }
    _DICT_DEPRECATED("use set()")         int8_t operator () (Key key, int32_t value)       { return set(key, String(value)); }
    _DICT_DEPRECATED("use set()")         int8_t operator () (Key key, float value)         { return set(key, String(value)); }
    _DICT_DEPRECATED("use set()")         int8_t operator () (Key key, double value)        { return set(key, String(value)); }
    _DICT_DEPRECATED("use getString()")   String search(Key key) const                      { return getString(key); }
    _DICT_DEPRECATED("use has()")         bool   operator () (Key key) const                { return has(key); }
    _DICT_DEPRECATED("use key(i)")        String operator () (size_t i) const               { return key(i); }
    _DICT_DEPRECATED("use value(i)")      String operator [] (size_t i) const               { return value(i); }

  private:
    // One allocation per pair: this header, then the key bytes and a NUL, then the
    // payload (text + NUL, packed text, or a numeric value; nothing for bool/null).
    struct __attribute__((packed)) Entry {
      uint8_t       type;     // DictType in the low 4 bits, BOOL_TRUE, PACKED
      uint8_t       hash;     // top byte of the key hash: cheap prefilter for comparisons
      _DICT_KLEN_T  klen;
      _DICT_VLEN_T  vlen;     // text length, packed length, or numeric payload size

      char*         key()           { return (char*)this + sizeof(Entry); }
      const char*   key() const     { return (const char*)this + sizeof(Entry); }
      char*         val()           { return key() + klen + 1; }
      const char*   val() const     { return key() + klen + 1; }
    };

    enum : uint8_t { TYPE_MASK = 0x0F, BOOL_TRUE = 0x10, PACKED = 0x80 };
    enum : size_t  { NPOS = ~(size_t)0 };     // "not found" position

    static size_t   payloadBytes(uint8_t type, size_t vlen);
    size_t          find(const char* key, size_t klen, uint32_t hash) const;
    const Entry*    lookup(Key key) const;
    int8_t          put(Key key, uint8_t type, const void* data, size_t len);
    int8_t          putCopy(const Entry* src);
    int8_t          grow(size_t need);
    void            indexAdd(size_t pos, uint32_t hash);
    bool            indexResize(size_t cap);
    void            indexDelete(size_t pos, uint32_t hash);
    void            indexPlace(size_t pos, uint32_t hash);
    int8_t          setSigned(Key key, long long value);
    int8_t          setUnsigned(Key key, unsigned long long value);
    int8_t          storeBare(const char* key, const char* text, size_t len);
    bool            integerOf(const Entry* e, int64_t& out) const;
    bool            numberOf(const Entry* e, double& out) const;
    bool            boolOf(const Entry* e, bool& out) const;
    bool            textOf(const Entry* e, char* buf, size_t size) const;

    template<class S> void    renderValue(const Entry* e, S& out, bool json) const;
    template<class S> void    emitJson(S& out) const;
    template<class R> int8_t  parse(R& in, int n);

    Entry**         iItems;     // insertion order; the only source of truth
    _DICT_SLOT_T*   iIndex;     // optional hash index: entry position + 1, 0 = empty
    size_t          iCount;
    size_t          iCapacity;
    size_t          iInitCap;
    size_t          iIndexCap;  // slots, a power of two, or 0 when there is no index
};

#endif // _DICTIONARYDECLARATIONS_H_
