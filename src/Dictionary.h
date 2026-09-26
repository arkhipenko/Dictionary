// Implementation of the Dictionary data type (declarations: DictionaryDeclarations.h)
// Copyright (c) 2020-2026 Anatoli Arkhipenko
// Distributed under the BSD 3-Clause License. See LICENSE.txt.

#include "DictionaryDeclarations.h"

#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef _DICT_COMPRESS
#ifdef _DICT_CODEC_TABLE
#include _DICT_CODEC_TABLE
#else
#include "DictionaryCodecTable.h"
#endif
#endif

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const uint16_t*)(addr))
#endif


// ==== Internal helpers ============================================================
namespace dict_detail {

// ---- output sinks: put(char) / put(const char*, size_t) ----
struct CountSink {
  size_t n;
  CountSink() : n(0) {}
  void put(char) { n++; }
  void put(const char*, size_t len) { n += len; }
};

// Appends to a String in chunks (one concat per 32 bytes). Text never contains
// a NUL, so a NUL-terminated chunk is safe to append.
struct StringSink {
  String& s;
  char    buf[33];
  uint8_t n;
  explicit StringSink(String& str) : s(str), n(0) {}
  void put(char c) { buf[n++] = c; if (n == 32) flush(); }
  void put(const char* p, size_t len) { while (len--) put(*p++); }
  void flush() { if (n) { buf[n] = 0; s += buf; n = 0; } }
};

// snprintf-style: writes what fits, always counts the full length.
struct BufSink {
  char*  b;
  size_t size;
  size_t len;
  BufSink(char* buf, size_t sz) : b(buf), size(sz), len(0) {}
  void put(char c) { if (len + 1 < size) b[len] = c; len++; }
  void put(const char* p, size_t n) { while (n--) put(*p++); }
  void finish() { if (size) b[len < size ? len : size - 1] = 0; }
};

struct PrintSink {
  Print& p;
  size_t n;
  explicit PrintSink(Print& out) : p(out), n(0) {}
  void put(char c) { n += p.write((uint8_t)c); }
  void put(const char* s, size_t len) { n += p.write((const uint8_t*)s, len); }
};


// ---- allocation: ESP32 PSRAM first when enabled and present ----
inline void* alloc(size_t n) {
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    void* p = ps_malloc(n);
    if (p) return p;
  }
#endif
  return malloc(n);
}

// FNV-1a, 32 bit.
inline uint32_t hashKey(const char* k, size_t n) {
  uint32_t h = 2166136261u;
  while (n--) {
    h ^= (uint8_t)*k++;
    h *= 16777619u;
  }
  return h;
}


// ---- number formatting (no printf: AVR lacks 64-bit and float printf) ----
inline size_t fmtUInt(char* buf, unsigned long long v) {
  char tmp[21];
  size_t n = 0;
  do { tmp[n++] = (char)('0' + (v % 10)); v /= 10; } while (v);
  for (size_t i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
  return n;
}

inline size_t fmtInt(char* buf, long long v) {
  if (v < 0) {
    buf[0] = '-';
    return 1 + fmtUInt(buf + 1, (unsigned long long)(-(v + 1)) + 1);   // safe for LLONG_MIN
  }
  return fmtUInt(buf, (unsigned long long)v);
}

// Fixed notation with `dec` decimals (trailing zeros trimmed); exponent notation
// for very large or very small magnitudes. buf must hold 40 bytes.
inline size_t fmtFloat(char* buf, double v, int dec) {
  if (v != v) { memcpy(buf, "nan", 3); return 3; }
  if (v - v != 0) {
    if (v < 0) { memcpy(buf, "-inf", 4); return 4; }
    memcpy(buf, "inf", 3); return 3;
  }
  if (dec < 0) dec = 0;
  if (dec > 9) dec = 9;
  size_t n = 0;
  if (v < 0) { buf[n++] = '-'; v = -v; }
  int exp10 = 0;
  bool sci = (v != 0) && (v >= 1e15 || v < 1e-5);
  if (sci) {
    exp10 = (int)floor(log10(v));
    v = v / pow(10.0, exp10);
    if (v >= 10) { v /= 10; exp10++; }
    if (v < 1)   { v *= 10; exp10--; }
  }
  unsigned long long scale = 1;
  for (int i = 0; i < dec; i++) scale *= 10;
  unsigned long long ip = (unsigned long long)v;
  unsigned long long fp = (unsigned long long)((v - (double)ip) * (double)scale + 0.5);
  if (fp >= scale) { ip++; fp -= scale; }
  if (sci && ip >= 10) { ip = 1; fp = 0; exp10++; }   // rounding carried into the next power
  n += fmtUInt(buf + n, ip);
  if (fp) {
    char d[10];
    for (int i = dec - 1; i >= 0; i--) { d[i] = (char)('0' + fp % 10); fp /= 10; }
    int last = dec - 1;
    while (last >= 0 && d[last] == '0') last--;
    buf[n++] = '.';
    for (int i = 0; i <= last; i++) buf[n++] = d[i];
  }
  if (sci) {
    buf[n++] = 'e';
    n += fmtInt(buf + n, exp10);
  }
  return n;
}


// ---- number parsing (whole text must match) ----
inline bool parseInt64(const char* s, size_t n, int64_t& out) {
  size_t i = 0;
  bool neg = false;
  if (i < n && (s[i] == '-' || s[i] == '+')) { neg = (s[i] == '-'); i++; }
  if (i == n) return false;
  unsigned long long v = 0;
  const unsigned long long lim = neg ? 9223372036854775808ULL : 9223372036854775807ULL;
  for (; i < n; i++) {
    char c = s[i];
    if (c < '0' || c > '9') return false;
    unsigned d = (unsigned)(c - '0');
    if (v > (lim - d) / 10) return false;   // overflow
    v = v * 10 + d;
  }
  out = neg ? (int64_t)(0 - v) : (int64_t)v;
  return true;
}

// s must be NUL-terminated at s[n].
inline bool parseDouble(const char* s, size_t n, double& out) {
  if (n == 0) return false;
  char c = s[0];
  if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.')) return false;
  char* end = NULL;
  double d = strtod(s, &end);
  if (end != s + n) return false;
  out = d;
  return true;
}

inline bool parseBool(const char* s, size_t n, bool& out) {
  if (n == 1 && (s[0] == '1' || s[0] == '0')) { out = (s[0] == '1'); return true; }
  const char* t = "true";
  const char* f = "false";
  if (n == 4) {
    size_t i = 0;
    while (i < 4 && (s[i] | 0x20) == t[i]) i++;
    if (i == 4) { out = true; return true; }
  }
  if (n == 5) {
    size_t i = 0;
    while (i < 5 && (s[i] | 0x20) == f[i]) i++;
    if (i == 5) { out = false; return true; }
  }
  return false;
}

// JSON number grammar: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
inline bool isJsonNumber(const char* s, size_t n, bool& integral) {
  size_t i = 0;
  integral = true;
  if (i < n && s[i] == '-') i++;
  if (i == n) return false;
  if (s[i] == '0') i++;
  else if (s[i] >= '1' && s[i] <= '9') { while (i < n && s[i] >= '0' && s[i] <= '9') i++; }
  else return false;
  if (i < n && s[i] == '.') {
    integral = false;
    i++;
    size_t start = i;
    while (i < n && s[i] >= '0' && s[i] <= '9') i++;
    if (i == start) return false;
  }
  if (i < n && (s[i] == 'e' || s[i] == 'E')) {
    integral = false;
    i++;
    if (i < n && (s[i] == '+' || s[i] == '-')) i++;
    size_t start = i;
    while (i < n && s[i] >= '0' && s[i] <= '9') i++;
    if (i == start) return false;
  }
  return i == n;
}


// ---- JSON text escaping ----
template<class S> inline void jsonChar(S& out, char ch) {
  static const char hex[] = "0123456789abcdef";
  uint8_t c = (uint8_t)ch;
  switch (c) {
    case '"':  out.put('\\'); out.put('"');  return;
    case '\\': out.put('\\'); out.put('\\'); return;
    case '\n': out.put('\\'); out.put('n');  return;
    case '\r': out.put('\\'); out.put('r');  return;
    case '\t': out.put('\\'); out.put('t');  return;
    case '\b': out.put('\\'); out.put('b');  return;
    case '\f': out.put('\\'); out.put('f');  return;
    default: break;
  }
  if (c < 0x20) {
    out.put('\\'); out.put('u'); out.put('0'); out.put('0');
    out.put(hex[c >> 4]); out.put(hex[c & 15]);
    return;
  }
  out.put(ch);
}


#ifdef _DICT_COMPRESS
// ---- value codec ----
// Bytes 0x00-0x7F are literals, 0x80 + i is token i of the table, 0xFF escapes the
// next byte (a literal byte of 0x80 or above). Tokens are sorted by first byte and,
// within one first byte, longest first, so the first match found is the longest.

inline uint8_t codecFirst(uint8_t t) {
  return pgm_read_byte(DictCodecTable::DATA + pgm_read_word(DictCodecTable::OFFS + t));
}

// Encodes s[0..n) into out, or only measures when out is NULL. Returns the length.
inline size_t codecEncode(const char* s, size_t n, char* out) {
  const uint8_t count = DictCodecTable::COUNT;
  size_t len = 0;
  size_t i = 0;
  while (i < n) {
    uint8_t c = (uint8_t)s[i];
    size_t match = 0;
    uint8_t tok = 0;
    // lower bound of tokens whose first byte is c
    uint8_t lo = 0, hi = count;
    while (lo < hi) {
      uint8_t mid = (uint8_t)((lo + hi) / 2);
      if (codecFirst(mid) < c) lo = (uint8_t)(mid + 1); else hi = mid;
    }
    for (uint8_t t = lo; t < count && codecFirst(t) == c; t++) {
      uint16_t off = pgm_read_word(DictCodecTable::OFFS + t);
      uint16_t tl  = (uint16_t)(pgm_read_word(DictCodecTable::OFFS + t + 1) - off);
      if (tl > n - i) continue;
      uint16_t k = 1;
      while (k < tl && (uint8_t)s[i + k] == pgm_read_byte(DictCodecTable::DATA + off + k)) k++;
      if (k == tl) { match = tl; tok = t; break; }
    }
    if (match) {
      if (out) out[len] = (char)(0x80 + tok);
      len++;
      i += match;
    }
    else if (c >= 0x80) {
      if (out) { out[len] = (char)0xFF; out[len + 1] = (char)c; }
      len += 2;
      i++;
    }
    else {
      if (out) out[len] = (char)c;
      len++;
      i++;
    }
  }
  return len;
}

template<class F> inline void codecDecode(const char* p, size_t n, F emit) {
  size_t i = 0;
  while (i < n) {
    uint8_t b = (uint8_t)p[i++];
    if (b < 0x80) emit((char)b);
    else if (b == 0xFF) { if (i < n) emit(p[i++]); }
    else {
      uint8_t t = (uint8_t)(b - 0x80);
      if (t >= DictCodecTable::COUNT) continue;   // not produced by the encoder
      uint16_t off = pgm_read_word(DictCodecTable::OFFS + t);
      uint16_t end = pgm_read_word(DictCodecTable::OFFS + t + 1);
      for (uint16_t k = off; k < end; k++) emit((char)pgm_read_byte(DictCodecTable::DATA + k));
    }
  }
}
#endif // _DICT_COMPRESS


// ---- JSON input: one byte of lookahead over a source with next() ----
struct MemSource {
  const char* p;
  explicit MemSource(const char* s) : p(s) {}
  int next() { if (!*p) return -1; return (uint8_t)*p++; }
};

struct StreamSource {
  Stream& s;
  explicit StreamSource(Stream& st) : s(st) {}
  int next() { return s.read(); }
};

template<class Src> struct Reader {
  Src&  src;
  int   la;
  bool  has;
  explicit Reader(Src& s) : src(s), la(-1), has(false) {}
  int raw() {
    for (;;) {
      int c = src.next();
#ifdef _DICT_ASCII_ONLY
      if (c >= 0x80) continue;    // drop non-ASCII bytes (c is 0..255 or -1)
#endif
      return c;
    }
  }
  int peek() { if (!has) { la = raw(); has = true; } return la; }
  int get()  { int c = peek(); has = false; return c; }
};

} // namespace dict_detail


// ==== Construction ===================================================================
Dictionary::Dictionary(size_t init_size) {
  iItems = NULL;
  iIndex = NULL;
  iCount = 0;
  iCapacity = 0;
  iInitCap = init_size;
  iIndexCap = 0;
}

Dictionary::~Dictionary() {
  destroy();
}

Dictionary::Dictionary(Dictionary&& other) noexcept {
  iItems = other.iItems;
  iIndex = other.iIndex;
  iCount = other.iCount;
  iCapacity = other.iCapacity;
  iInitCap = other.iInitCap;
  iIndexCap = other.iIndexCap;
  other.iItems = NULL;
  other.iIndex = NULL;
  other.iCount = 0;
  other.iCapacity = 0;
  other.iIndexCap = 0;
}

Dictionary& Dictionary::operator = (Dictionary&& other) noexcept {
  if (this != &other) {
    destroy();
    iItems = other.iItems;
    iIndex = other.iIndex;
    iCount = other.iCount;
    iCapacity = other.iCapacity;
    iInitCap = other.iInitCap;
    iIndexCap = other.iIndexCap;
    other.iItems = NULL;
    other.iIndex = NULL;
    other.iCount = 0;
    other.iCapacity = 0;
    other.iIndexCap = 0;
  }
  return *this;
}

Dictionary& Dictionary::operator = (const Dictionary& other) {
  if (this != &other) {
    destroy();
    merge(other);
  }
  return *this;
}


// ==== Storage ========================================================================
size_t Dictionary::payloadBytes(uint8_t type, size_t vlen) {
  // plain text keeps a terminating NUL; everything else stores exactly vlen bytes
  return vlen + (((type & TYPE_MASK) == DICT_STR && !(type & PACKED)) ? 1 : 0);
}

size_t Dictionary::find(const char* key, size_t klen, uint32_t hash) const {
  uint8_t hb = (uint8_t)(hash >> 24);
  if (iIndex) {
    size_t mask = iIndexCap - 1;
    for (size_t s = hash & mask; ; s = (s + 1) & mask) {
      size_t v = iIndex[s];
      if (v == 0) return NPOS;
      const Entry* e = iItems[v - 1];
      if (e->hash == hb && e->klen == klen && memcmp(e->key(), key, klen) == 0) return v - 1;
    }
  }
  for (size_t i = 0; i < iCount; i++) {
    const Entry* e = iItems[i];
    if (e->hash == hb && e->klen == klen && memcmp(e->key(), key, klen) == 0) return i;
  }
  return NPOS;
}

const Dictionary::Entry* Dictionary::lookup(Key key) const {
  if (!key.p) return NULL;
  size_t klen = strnlen(key.p, _DICT_KEYLEN + 1);
  if (klen == 0 || klen > _DICT_KEYLEN) return NULL;
  size_t pos = find(key.p, klen, dict_detail::hashKey(key.p, klen));
  return pos == NPOS ? NULL : iItems[pos];
}

// Insert or replace one pair. `type` is the stored type byte without PACKED; for
// DICT_STR, data/len is the raw text, otherwise the numeric payload.
// Atomic: on any failure nothing changes.
int8_t Dictionary::put(Key key, uint8_t type, const void* data, size_t len) {
  if (!key.p) return DICTIONARY_ERR;
  size_t klen = strnlen(key.p, _DICT_KEYLEN + 1);
  if (klen == 0 || klen > _DICT_KEYLEN) return DICTIONARY_ERR;

  uint8_t stype = type;
  size_t  vlen = len;
  size_t  pbytes = len;
  bool    text = ((type & TYPE_MASK) == DICT_STR);
  if (text) {
    if (len > _DICT_VALLEN) return DICTIONARY_ERR;
    pbytes = len + 1;
#ifdef _DICT_COMPRESS
    size_t enc = dict_detail::codecEncode((const char*)data, len, NULL);
    if (enc < len) {                // keep it packed only when that is smaller
      stype |= PACKED;
      vlen = enc;
      pbytes = enc;
    }
#endif
  }

  uint32_t h = dict_detail::hashKey(key.p, klen);
  size_t pos = find(key.p, klen, h);

  Entry* e = NULL;
  bool inPlace = false;
  if (pos != NPOS) {
    Entry* old = iItems[pos];
    inPlace = (payloadBytes(old->type, old->vlen) == pbytes);   // same footprint: rewrite in place
    e = old;
  }
  else if (iCount >= _DICT_MAX_ENTRIES) {
    return DICTIONARY_OOB;
  }
  if (!inPlace) {
    e = (Entry*)dict_detail::alloc(sizeof(Entry) + klen + 1 + pbytes);
    if (!e) return DICTIONARY_MEM;
    e->hash = (uint8_t)(h >> 24);
    e->klen = (_DICT_KLEN_T)klen;
    memcpy(e->key(), key.p, klen);    // before the old entry (which may hold key.p) is freed
    e->key()[klen] = 0;
  }
  e->type = stype;
  e->vlen = (_DICT_VLEN_T)vlen;
  char* dst = e->val();
  if (text) {
#ifdef _DICT_COMPRESS
    if (stype & PACKED) dict_detail::codecEncode((const char*)data, len, dst);
    else
#endif
    {
      if (len) memmove(dst, data, len);   // memmove: data may point into this entry
      dst[len] = 0;
    }
  }
  else if (len) {
    memcpy(dst, data, len);
  }

  if (inPlace) return DICTIONARY_OK;
  if (pos != NPOS) {
    free(iItems[pos]);
    iItems[pos] = e;                  // same position: the index is unaffected
    return DICTIONARY_OK;
  }
  if (grow(iCount + 1)) {
    free(e);
    return DICTIONARY_MEM;
  }
  iItems[iCount++] = e;
  indexAdd(iCount - 1, h);
  return DICTIONARY_OK;
}

// Insert or replace with a copy of an entry from another dictionary (same layout).
int8_t Dictionary::putCopy(const Entry* src) {
  size_t klen = src->klen;
  size_t bytes = sizeof(Entry) + klen + 1 + payloadBytes(src->type, src->vlen);
  uint32_t h = dict_detail::hashKey(src->key(), klen);
  size_t pos = find(src->key(), klen, h);
  if (pos != NPOS) {
    Entry* old = iItems[pos];
    if (sizeof(Entry) + klen + 1 + payloadBytes(old->type, old->vlen) == bytes) {
      memcpy(old, src, bytes);
      return DICTIONARY_OK;
    }
  }
  else if (iCount >= _DICT_MAX_ENTRIES) {
    return DICTIONARY_OOB;
  }
  Entry* e = (Entry*)dict_detail::alloc(bytes);
  if (!e) return DICTIONARY_MEM;
  memcpy(e, src, bytes);
  if (pos != NPOS) {
    free(iItems[pos]);
    iItems[pos] = e;
    return DICTIONARY_OK;
  }
  if (grow(iCount + 1)) {
    free(e);
    return DICTIONARY_MEM;
  }
  iItems[iCount++] = e;
  indexAdd(iCount - 1, h);
  return DICTIONARY_OK;
}

// Make room for `need` entry pointers. The old array is freed only after the new
// one is filled, so a failure changes nothing.
int8_t Dictionary::grow(size_t need) {
  if (need <= iCapacity) return DICTIONARY_OK;
  size_t cap = iCapacity ? iCapacity * 2 : (iInitCap ? iInitCap : 1);
  while (cap < need) cap *= 2;
  if (cap > _DICT_MAX_ENTRIES) cap = _DICT_MAX_ENTRIES;
  if (cap < need) return DICTIONARY_OOB;
  Entry** p = (Entry**)dict_detail::alloc(cap * sizeof(Entry*));
  if (!p) return DICTIONARY_MEM;
  if (iCount) memcpy(p, iItems, iCount * sizeof(Entry*));
  free(iItems);
  iItems = p;
  iCapacity = cap;
  return DICTIONARY_OK;
}


// ---- hash index: an accelerator only. Losing it costs speed, never data. ----
void Dictionary::indexPlace(size_t pos, uint32_t hash) {
  size_t mask = iIndexCap - 1;
  size_t s = hash & mask;
  while (iIndex[s]) s = (s + 1) & mask;
  iIndex[s] = (_DICT_SLOT_T)(pos + 1);
}

// Replace the index with one of `cap` slots holding every entry. On allocation
// failure the old index is dropped and lookups fall back to scanning.
bool Dictionary::indexResize(size_t cap) {
  _DICT_SLOT_T* slots = (_DICT_SLOT_T*)dict_detail::alloc(cap * sizeof(_DICT_SLOT_T));
  free(iIndex);
  iIndex = slots;
  iIndexCap = slots ? cap : 0;
  if (!slots) return false;
  memset(iIndex, 0, cap * sizeof(_DICT_SLOT_T));
  for (size_t i = 0; i < iCount; i++) {
    const Entry* e = iItems[i];
    indexPlace(i, dict_detail::hashKey(e->key(), e->klen));
  }
  return true;
}

// Drop the entry at `pos` (key hash `hash`) from the index before it leaves the
// array. Linear-probing deletion without tombstones: later members of the probe
// cluster move back into the hole unless their home slot lies between the hole
// and their current slot. Then every stored position after `pos` moves down by
// one, matching the shift of the array. Only the cluster's keys are re-hashed;
// nothing is allocated.
void Dictionary::indexDelete(size_t pos, uint32_t hash) {
  size_t mask = iIndexCap - 1;
  size_t hole = hash & mask;
  while ((size_t)iIndex[hole] != pos + 1) hole = (hole + 1) & mask;
  size_t i = hole;
  for (;;) {
    i = (i + 1) & mask;
    size_t v = iIndex[i];
    if (v == 0) break;
    const Entry* e = iItems[v - 1];
    size_t home = dict_detail::hashKey(e->key(), e->klen) & mask;
    bool stays = (hole < i) ? (home > hole && home <= i) : (home > hole || home <= i);
    if (!stays) {
      iIndex[hole] = iIndex[i];
      hole = i;
    }
  }
  iIndex[hole] = 0;
  if (pos + 1 == iCount) return;        // last entry: no position moves
  const size_t p1 = pos + 1;
  for (size_t s = 0; s < iIndexCap; s++) {
    iIndex[s] = (_DICT_SLOT_T)(iIndex[s] - ((size_t)iIndex[s] > p1));   // branch-free
  }
}

static inline size_t dictIndexCapFor(size_t n) {
  size_t c = 8;
  while (c * 3 < n * 4) c <<= 1;    // keep the load at or below 3/4
  return c;
}

// Called after appending the entry at `pos`.
void Dictionary::indexAdd(size_t pos, uint32_t hash) {
#if _DICT_INDEX_MIN > 0
  if (iIndex) {
    if (iCount * 4 <= iIndexCap * 3) indexPlace(pos, hash);
    else indexResize(dictIndexCapFor(iCount));      // rehashes every entry, including pos
    return;
  }
  if (iCount >= _DICT_INDEX_MIN) indexResize(dictIndexCapFor(iCount));
#else
  (void)pos;
  (void)hash;
#endif
}


// ==== Write API ======================================================================
int8_t Dictionary::set(Key key, const char* value) {
  if (!value) return setNull(key);
  return put(key, DICT_STR, value, strnlen(value, _DICT_VALLEN + 1));
}

int8_t Dictionary::set(Key key, bool value) {
  return put(key, (uint8_t)(DICT_BOOL | (value ? BOOL_TRUE : 0)), NULL, 0);
}

int8_t Dictionary::set(Key key, float value) {
  return put(key, DICT_FLOAT, &value, sizeof(float));
}

int8_t Dictionary::set(Key key, double value) {
#ifdef _DICT_WIDE_NUMBERS
  return put(key, DICT_DOUBLE, &value, sizeof(double));
#else
  float f = (float)value;
  return put(key, DICT_FLOAT, &f, sizeof(float));
#endif
}

int8_t Dictionary::setNull(Key key) {
  return put(key, DICT_NULL, NULL, 0);
}

// Integers are stored as int32 (int64 with _DICT_WIDE_NUMBERS). A value outside
// that range is stored as decimal text, so it is never altered.
int8_t Dictionary::setSigned(Key key, long long value) {
  if (value >= -2147483647LL - 1 && value <= 2147483647LL) {
    int32_t v = (int32_t)value;
    return put(key, DICT_INT, &v, sizeof(v));
  }
#ifdef _DICT_WIDE_NUMBERS
  int64_t v = (int64_t)value;
  return put(key, DICT_INT64, &v, sizeof(v));
#else
  char buf[24];
  size_t n = dict_detail::fmtInt(buf, value);
  return put(key, DICT_STR, buf, n);
#endif
}

int8_t Dictionary::setUnsigned(Key key, unsigned long long value) {
  if (value <= 2147483647ULL) return setSigned(key, (long long)value);
#ifdef _DICT_WIDE_NUMBERS
  if (value <= 9223372036854775807ULL) return setSigned(key, (long long)value);
#endif
  char buf[24];
  size_t n = dict_detail::fmtUInt(buf, value);
  return put(key, DICT_STR, buf, n);
}

int8_t Dictionary::remove(Key key) {
  if (!key.p) return DICTIONARY_ERR;
  size_t klen = strnlen(key.p, _DICT_KEYLEN + 1);
  if (klen > _DICT_KEYLEN) return DICTIONARY_ERR;
  if (klen == 0) return DICTIONARY_OK;
  uint32_t h = dict_detail::hashKey(key.p, klen);
  size_t pos = find(key.p, klen, h);
  if (pos == NPOS) return DICTIONARY_OK;
  if (iIndex) indexDelete(pos, h);      // while the positions are still valid
  free(iItems[pos]);
  memmove(iItems + pos, iItems + pos + 1, (iCount - pos - 1) * sizeof(Entry*));
  iCount--;
  return DICTIONARY_OK;
}

void Dictionary::destroy() {
  for (size_t i = 0; i < iCount; i++) free(iItems[i]);
  free(iItems);
  free(iIndex);
  iItems = NULL;
  iIndex = NULL;
  iCount = 0;
  iCapacity = 0;
  iIndexCap = 0;
}

int8_t Dictionary::reserve(size_t n) {
  if (n > _DICT_MAX_ENTRIES) return DICTIONARY_OOB;
  int8_t rc = grow(n);
  if (rc) return rc;
#if _DICT_INDEX_MIN > 0
  if (n >= _DICT_INDEX_MIN) {
    size_t cap = dictIndexCapFor(n);
    if (cap > iIndexCap && !indexResize(cap)) return DICTIONARY_MEM;
  }
#endif
  return DICTIONARY_OK;
}

int8_t Dictionary::merge(const Dictionary& other) {
  if (&other == this) return DICTIONARY_OK;
  for (size_t i = 0; i < other.iCount; i++) {
    int8_t rc = putCopy(other.iItems[i]);
    if (rc) return rc;
  }
  return DICTIONARY_OK;
}


// ==== Read API =======================================================================
bool Dictionary::has(Key key) const {
  return lookup(key) != NULL;
}

DictType Dictionary::type(Key key) const {
  const Entry* e = lookup(key);
  return e ? (DictType)(e->type & TYPE_MASK) : DICT_NONE;
}

// Copies a text value, NUL-terminated. False when not text or it does not fit.
bool Dictionary::textOf(const Entry* e, char* buf, size_t size) const {
  if ((e->type & TYPE_MASK) != DICT_STR) return false;
  dict_detail::BufSink b(buf, size);
  renderValue(e, b, false);
  b.finish();
  return b.len < size;
}

bool Dictionary::integerOf(const Entry* e, int64_t& out) const {
  switch (e->type & TYPE_MASK) {
    case DICT_INT:   { int32_t v; memcpy(&v, e->val(), sizeof(v)); out = v; return true; }
    case DICT_INT64: { int64_t v; memcpy(&v, e->val(), sizeof(v)); out = v; return true; }
#ifndef _DICT_STRICT_GET
    case DICT_FLOAT:
    case DICT_DOUBLE: {
      double d;
      if (!numberOf(e, d)) return false;
      if (d != d || d < -9.2e18 || d > 9.2e18 || d != floor(d)) return false;   // integral only
      out = (int64_t)d;
      return true;
    }
    case DICT_BOOL: out = (e->type & BOOL_TRUE) ? 1 : 0; return true;
    case DICT_STR: {
      char buf[24];
      if (!textOf(e, buf, sizeof(buf))) return false;
      return dict_detail::parseInt64(buf, strlen(buf), out);
    }
#endif
    default: return false;
  }
}

bool Dictionary::numberOf(const Entry* e, double& out) const {
  switch (e->type & TYPE_MASK) {
    case DICT_INT:    { int32_t v; memcpy(&v, e->val(), sizeof(v)); out = v; return true; }
    case DICT_INT64:  { int64_t v; memcpy(&v, e->val(), sizeof(v)); out = (double)v; return true; }
    case DICT_FLOAT:  { float v;   memcpy(&v, e->val(), sizeof(v)); out = v; return true; }
    case DICT_DOUBLE: { double v;  memcpy(&v, e->val(), sizeof(v)); out = v; return true; }
#ifndef _DICT_STRICT_GET
    case DICT_BOOL: out = (e->type & BOOL_TRUE) ? 1 : 0; return true;
    case DICT_STR: {
      char buf[48];
      if (!textOf(e, buf, sizeof(buf))) return false;
      return dict_detail::parseDouble(buf, strlen(buf), out);
    }
#endif
    default: return false;
  }
}

bool Dictionary::boolOf(const Entry* e, bool& out) const {
  switch (e->type & TYPE_MASK) {
    case DICT_BOOL: out = (e->type & BOOL_TRUE) != 0; return true;
#ifndef _DICT_STRICT_GET
    case DICT_INT:
    case DICT_INT64: {
      int64_t v;
      if (!integerOf(e, v)) return false;
      out = (v != 0);
      return true;
    }
    case DICT_STR: {
      char buf[8];
      if (!textOf(e, buf, sizeof(buf))) return false;
      return dict_detail::parseBool(buf, strlen(buf), out);
    }
#endif
    default: return false;
  }
}

int32_t Dictionary::getInt(Key key, int32_t def) const {
  const Entry* e = lookup(key);
  int64_t v;
  if (!e || !integerOf(e, v) || v < -2147483647LL - 1 || v > 2147483647LL) return def;
  return (int32_t)v;
}

float Dictionary::getFloat(Key key, float def) const {
  const Entry* e = lookup(key);
  double d;
  if (!e || !numberOf(e, d)) return def;
  return (float)d;
}

bool Dictionary::getBool(Key key, bool def) const {
  const Entry* e = lookup(key);
  bool b;
  if (!e || !boolOf(e, b)) return def;
  return b;
}

#ifdef _DICT_WIDE_NUMBERS
int64_t Dictionary::getInt64(Key key, int64_t def) const {
  const Entry* e = lookup(key);
  int64_t v;
  if (!e || !integerOf(e, v)) return def;
  return v;
}

double Dictionary::getDouble(Key key, double def) const {
  const Entry* e = lookup(key);
  double d;
  if (!e || !numberOf(e, d)) return def;
  return d;
}
#endif

String Dictionary::getString(Key key, const char* def) const {
  const Entry* e = lookup(key);
  if (!e) return String(def ? def : "");
  String s;
  dict_detail::StringSink out(s);
  renderValue(e, out, false);
  out.flush();
  return s;
}

size_t Dictionary::getString(Key key, char* buf, size_t size) const {
  const Entry* e = lookup(key);
  dict_detail::BufSink out(buf, size);
  if (e) renderValue(e, out, false);
  out.finish();
  return out.len;
}

const char* Dictionary::peek(Key key) const {
  const Entry* e = lookup(key);
  if (!e || (e->type & TYPE_MASK) != DICT_STR || (e->type & PACKED)) return NULL;
  return e->val();
}

const char* Dictionary::keyAt(size_t i) const {
  return i < iCount ? iItems[i]->key() : NULL;
}

DictType Dictionary::typeAt(size_t i) const {
  return i < iCount ? (DictType)(iItems[i]->type & TYPE_MASK) : DICT_NONE;
}

String Dictionary::key(size_t i) const {
  return i < iCount ? String(iItems[i]->key()) : String();
}

String Dictionary::value(size_t i) const {
  if (i >= iCount) return String();
  String s;
  dict_detail::StringSink out(s);
  renderValue(iItems[i], out, false);
  out.flush();
  return s;
}

bool Dictionary::operator == (const Dictionary& b) const {
  if (iCount != b.iCount) return false;
  for (size_t i = 0; i < iCount; i++) {
    const Entry* e = iItems[i];
    size_t pos = b.find(e->key(), e->klen, dict_detail::hashKey(e->key(), e->klen));
    if (pos == NPOS) return false;
    const Entry* f = b.iItems[pos];
    if (f->type != e->type || f->vlen != e->vlen) return false;
    if (memcmp(f->val(), e->val(), payloadBytes(e->type, e->vlen)) != 0) return false;
  }
  return true;
}

size_t Dictionary::size() const {
  size_t sz = iCapacity * sizeof(Entry*) + iIndexCap * sizeof(_DICT_SLOT_T);
  for (size_t i = 0; i < iCount; i++) {
    const Entry* e = iItems[i];
    sz += sizeof(Entry) + e->klen + 1 + payloadBytes(e->type, e->vlen);
  }
  return sz;
}

size_t Dictionary::esize() const {
  size_t sz = 0;
  for (size_t i = 0; i < iCount; i++) {
    dict_detail::CountSink c;
    renderValue(iItems[i], c, false);
    sz += iItems[i]->klen + 1 + c.n + 1;
  }
  return sz;
}


// ==== Rendering ======================================================================
// Writes a value as text, or as a JSON value when `json` is set.
template<class S> void Dictionary::renderValue(const Entry* e, S& out, bool json) const {
  char buf[40];
  switch (e->type & TYPE_MASK) {
    case DICT_STR: {
      if (json) out.put('"');
#ifdef _DICT_COMPRESS
      if (e->type & PACKED) {
        if (json) dict_detail::codecDecode(e->val(), e->vlen, [&out](char c) { dict_detail::jsonChar(out, c); });
        else      dict_detail::codecDecode(e->val(), e->vlen, [&out](char c) { out.put(c); });
      }
      else
#endif
      {
        const char* p = e->val();
        if (json) { for (size_t i = 0; i < e->vlen; i++) dict_detail::jsonChar(out, p[i]); }
        else out.put(p, e->vlen);
      }
      if (json) out.put('"');
      return;
    }
    case DICT_INT: {
      int32_t v;
      memcpy(&v, e->val(), sizeof(v));
      out.put(buf, dict_detail::fmtInt(buf, v));
      return;
    }
    case DICT_INT64: {
      int64_t v;
      memcpy(&v, e->val(), sizeof(v));
      out.put(buf, dict_detail::fmtInt(buf, v));
      return;
    }
    case DICT_FLOAT:
    case DICT_DOUBLE: {
      double d;
      numberOf(e, d);
      if (json && (d != d || d - d != 0)) { out.put("null", 4); return; }   // JSON has no nan/inf
      out.put(buf, dict_detail::fmtFloat(buf, d, _DICT_FLOAT_DECIMALS));
      return;
    }
    case DICT_BOOL:
      if (e->type & BOOL_TRUE) out.put("true", 4); else out.put("false", 5);
      return;
    case DICT_NULL:
      out.put("null", 4);
      return;
    default:
      return;
  }
}

template<class S> void Dictionary::emitJson(S& out) const {
  out.put('{');
  for (size_t i = 0; i < iCount; i++) {
    const Entry* e = iItems[i];
    if (i) out.put(',');
    out.put('"');
    const char* k = e->key();
    for (size_t j = 0; j < e->klen; j++) dict_detail::jsonChar(out, k[j]);
    out.put('"');
    out.put(':');
    renderValue(e, out, true);
  }
  out.put('}');
}

size_t Dictionary::jsize() const {
  dict_detail::CountSink c;
  emitJson(c);
  return c.n + 1;
}

String Dictionary::json() const {
  String s;
  s.reserve(jsize());
  dict_detail::StringSink out(s);
  emitJson(out);
  out.flush();
  return s;
}

size_t Dictionary::json(Print& p) const {
  dict_detail::PrintSink out(p);
  emitJson(out);
  return out.n;
}


// ==== JSON input =====================================================================
// A lenient, flat JSON-like format:
//   - optional braces; pairs separated by ',', a newline or '}'; trailing comma allowed
//   - keys and values quoted ("...", standard escapes) or bare (no escapes decoded;
//     a backslash takes the next character literally)
//   - '#' outside quotes starts a comment that runs to the end of the line
//   - nested objects and arrays are rejected (DICTIONARY_FMT)
// Values are stored as text as given, unless _DICT_TYPED_JSON (bare numbers,
// true/false and null become typed). Not atomic: pairs loaded before an error stay.
namespace dict_detail {

// Skip spaces, tabs, CRs, comments and (optionally) newlines; returns the next
// byte without consuming it, or -1 at the end.
template<class R> inline int skipBlank(R& in, bool newlines) {
  for (;;) {
    int c = in.peek();
    if (c == ' ' || c == '\t' || c == '\r' || (newlines && c == '\n')) { in.get(); continue; }
    if (c == '#') {                                   // leave the '\n': it may end a pair
      while ((c = in.peek()) >= 0 && c != '\n') in.get();
      continue;
    }
    return c;
  }
}

inline int hexVal(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

template<class R> inline int8_t readHex4(R& in, uint32_t& cp) {
  cp = 0;
  for (int i = 0; i < 4; i++) {
    int c = in.get();
    if (c < 0) return DICTIONARY_EOF;
    int v = hexVal(c);
    if (v < 0) return DICTIONARY_BCKSL;
    cp = (cp << 4) | (uint32_t)v;
  }
  return DICTIONARY_OK;
}

inline int8_t appendByte(char* buf, size_t max, size_t& len, char c) {
  if (len >= max) return DICTIONARY_OOB;
  buf[len++] = c;
  return DICTIONARY_OK;
}

// After a backslash inside quotes.
template<class R> inline int8_t readEscape(R& in, char* buf, size_t max, size_t& len) {
  int c = in.get();
  if (c < 0) return DICTIONARY_EOF;
  switch (c) {
    case 'b': return appendByte(buf, max, len, '\b');
    case 'f': return appendByte(buf, max, len, '\f');
    case 'n': return appendByte(buf, max, len, '\n');
    case 'r': return appendByte(buf, max, len, '\r');
    case 't': return appendByte(buf, max, len, '\t');
    case 'u': {
      uint32_t cp;
      int8_t rc = readHex4(in, cp);
      if (rc) return rc;
      if (cp >= 0xD800 && cp <= 0xDBFF) {             // high surrogate: a low one must follow
        if (in.get() != '\\' || in.get() != 'u') return DICTIONARY_BCKSL;
        uint32_t lo;
        rc = readHex4(in, lo);
        if (rc) return rc;
        if (lo < 0xDC00 || lo > 0xDFFF) return DICTIONARY_BCKSL;
        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
      }
      else if (cp >= 0xDC00 && cp <= 0xDFFF) return DICTIONARY_BCKSL;
      if (cp == 0) return DICTIONARY_BCKSL;           // a value cannot hold a NUL
#ifdef _DICT_ASCII_ONLY
      if (cp > 0x7F) return DICTIONARY_OK;            // dropped, like raw non-ASCII bytes
#endif
      char u[4];
      size_t n;
      if (cp < 0x80)         { u[0] = (char)cp; n = 1; }
      else if (cp < 0x800)   { u[0] = (char)(0xC0 | (cp >> 6));  u[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
      else if (cp < 0x10000) { u[0] = (char)(0xE0 | (cp >> 12)); u[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                               u[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
      else                   { u[0] = (char)(0xF0 | (cp >> 18)); u[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                               u[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
      if (len + n > max) return DICTIONARY_OOB;
      memcpy(buf + len, u, n);
      len += n;
      return DICTIONARY_OK;
    }
    default:                                          // \" \\ \/ and anything else: literal
      return appendByte(buf, max, len, (char)c);
  }
}

// Reads one key or value token into buf (NUL-terminated). Bare keys end at
// ':' too; bare values may contain ':' (e.g. URLs).
template<class R> inline int8_t readToken(R& in, char* buf, size_t max, size_t& len, bool& quoted, bool isKey) {
  len = 0;
  int c = in.peek();
  quoted = (c == '"');
  if (quoted) {
    in.get();
    for (;;) {
      c = in.get();
      if (c < 0) return DICTIONARY_EOF;
      if (c == '"') break;
      if (c == '\n') return DICTIONARY_QUOTE;
      int8_t rc = (c == '\\') ? readEscape(in, buf, max, len) : appendByte(buf, max, len, (char)c);
      if (rc) return rc;
    }
  }
  else {
    for (;;) {
      c = in.peek();
      if (c < 0 || c == ',' || c == '\n' || c == '}' || c == '#' || c == '"') break;
      if (isKey && (c == ':' || c == '{' || c == '[' || c == ']')) break;
      in.get();
      if (c == '\\') {
        c = in.get();
        if (c < 0) return DICTIONARY_EOF;
      }
      int8_t rc = appendByte(buf, max, len, (char)c);
      if (rc) return rc;
    }
    while (len && (buf[len - 1] == ' ' || buf[len - 1] == '\t' || buf[len - 1] == '\r')) len--;
  }
  buf[len] = 0;
  return DICTIONARY_OK;
}

} // namespace dict_detail

int8_t Dictionary::storeBare(const char* key, const char* text, size_t len) {
#ifdef _DICT_TYPED_JSON
  if (len == 4 && memcmp(text, "true", 4) == 0)  return set(key, true);
  if (len == 5 && memcmp(text, "false", 5) == 0) return set(key, false);
  if (len == 4 && memcmp(text, "null", 4) == 0)  return setNull(key);
  bool integral;
  if (dict_detail::isJsonNumber(text, len, integral)) {
    if (integral) {
      int64_t v;
      if (dict_detail::parseInt64(text, len, v)) return setSigned(key, v);
    }
    else {
      double d = strtod(text, NULL);
      if (!(d != d) && d - d == 0) return set(key, d);
    }
  }
#endif
  return put(key, DICT_STR, text, len);
}

template<class R> int8_t Dictionary::parse(R& in, int n) {
  char kbuf[_DICT_KEYLEN + 1];
#if (_DICT_KEYLEN + _DICT_VALLEN) <= 512
  char vbuf[_DICT_VALLEN + 1];
#else
  // Large limits: keep the value buffer off the stack.
  struct Buf {
    char* p;
    Buf() : p((char*)malloc(_DICT_VALLEN + 1)) {}
    ~Buf() { free(p); }
  } heap;
  if (!heap.p) return DICTIONARY_MEM;
  char* vbuf = heap.p;
#endif
  int loaded = 0;
  for (;;) {
    // key position
    int c = dict_detail::skipBlank(in, true);
    if (c < 0) break;
    if (c == '{' || c == '}') { in.get(); continue; }   // braces are optional
    if (c == ',' || c == ':' || c == '[' || c == ']') return DICTIONARY_FMT;
    size_t klen;
    bool quoted;
    int8_t rc = dict_detail::readToken(in, kbuf, _DICT_KEYLEN, klen, quoted, true);
    if (rc) return rc;
    if (klen == 0) return DICTIONARY_FMT;
    c = dict_detail::skipBlank(in, false);
    if (c != ':') return (c < 0) ? DICTIONARY_EOF : DICTIONARY_COLON;
    in.get();

    // value position (JSON allows a line break before the value)
    c = dict_detail::skipBlank(in, true);
    if (c < 0) return DICTIONARY_EOF;
    if (c == '{' || c == '[' || c == ']') return DICTIONARY_FMT;   // nested structures are not supported
    size_t vlen = 0;
    quoted = false;
    if (c != ',' && c != '}') {                         // otherwise: an empty bare value
      rc = dict_detail::readToken(in, vbuf, _DICT_VALLEN, vlen, quoted, false);
      if (rc) return rc;
    }
    vbuf[vlen] = 0;
    c = dict_detail::skipBlank(in, false);
    if (c >= 0 && c != ',' && c != '\n' && c != '}') return DICTIONARY_COMMA;
    if (c == ',' || c == '\n') in.get();                // '}' is consumed at the key position

    rc = quoted ? put(kbuf, DICT_STR, vbuf, vlen) : storeBare(kbuf, vbuf, vlen);
    if (rc) return rc;
    loaded++;
    if (n > 0 && loaded >= n) return DICTIONARY_OK;
  }
  if (n > 0 && loaded < n) return DICTIONARY_EOF;
  return DICTIONARY_OK;
}

int8_t Dictionary::jload(const char* json, int n) {
  if (!json) return DICTIONARY_ERR;
  dict_detail::MemSource src(json);
  dict_detail::Reader<dict_detail::MemSource> in(src);
  return parse(in, n);
}

int8_t Dictionary::jload(Stream& json, int n) {
  dict_detail::StreamSource src(json);
  dict_detail::Reader<dict_detail::StreamSource> in(src);
  return parse(in, n);
}

#endif // _DICTIONARY_H_
