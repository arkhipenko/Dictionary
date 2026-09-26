/*
  Dictionary Benchmark
  Copyright (c) 2020-2026 Anatoli Arkhipenko
  Distributed under the BSD 3-Clause License. See LICENSE.txt.

  Times every Dictionary operation on the board it runs on and reports memory use.

  Data: BENCH_PAIRS pairs, generated the same way on every board. It is the same kind
  of data as the benchmark in the README: keys are two words joined by '-' (about 13
  characters), values are four words (about 28 characters). Generating a key or value
  costs time too, so each timed loop has an untimed twin that only generates the data,
  and that cost is subtracted: "us per op" is the library's own cost.

  Each run also checks its results (counts, lookups, values, JSON round trip, empty
  after removal, heap returned) and prints "CHECK FAILED" if one is wrong.

  Output (115200 baud): a table while the phases run, then two CSV lines (names and
  values) for comparing boards and build options in a spreadsheet.

  Runs on ESP32 and ESP8266 (heap figures are reported there) and on other boards with
  enough RAM (an Arduino Mega works; an Uno or Nano has too little). To compare build
  options, uncomment them below and run again. On a fast PC the cheapest operations
  are close to the 1 us timer resolution and can come out slightly negative.
*/

// ---- build options under test ---------------------------------------------------
//#define _DICT_COMPRESS          // built-in value compression
//#define _DICT_USE_PSRAM         // ESP32: entries, array and index in PSRAM
//#define _DICT_INDEX_MIN 0       // no hash index: every lookup scans the array

// ---- benchmark size ---------------------------------------------------------------
// Number of pairs, at most 10000. The defaults fit the free heap of each board; to
// choose another, uncomment and set BENCH_PAIRS.
//#define BENCH_PAIRS 500
#if defined(BENCH_PAIRS)
static const uint32_t PAIRS = BENCH_PAIRS;
#elif defined(ESP32) && defined(_DICT_USE_PSRAM)
static const uint32_t PAIRS = 10000;
#elif defined(ESP32)
static const uint32_t PAIRS = 1000;
#elif defined(ESP8266)
static const uint32_t PAIRS = 200;
#elif defined(__AVR__)
static const uint32_t PAIRS = 20;
#else
static const uint32_t PAIRS = 100;
#endif
static_assert(PAIRS >= 1 && PAIRS <= 10000, "BENCH_PAIRS must be between 1 and 10000");

// Read-only phases go over all pairs this many times (more samples, same data).
static const uint32_t READ_PASSES = 3;

#include <Dictionary.h>

#if defined(__AVR__) && RAMEND < 0x1000
#error "Dict_Benchmark needs more RAM than this board has: use a Mega, an ESP32 or an ESP8266"
#endif


// ---- test data --------------------------------------------------------------------
// 100 words from the README benchmark word list (none contains '-' or '+').
static const char* const WORDS[100] = {
  "subdued", "unequal", "toes", "invincible", "dry", "roof", "delightful", "gather", "ripe", "approve",
  "occur", "dust", "slope", "hang", "utopian", "scintillating", "magenta", "abandoned", "icy", "hum",
  "truculent", "squirrel", "exist", "piquant", "enchanting", "hapless", "slip", "caption", "faded", "near",
  "stereotyped", "uptight", "makeshift", "scale", "aromatic", "taboo", "flowery", "greedy", "military", "rake",
  "rinse", "striped", "new", "permit", "belief", "buzz", "acid", "goofy", "flawless", "impress",
  "coal", "settle", "tired", "smelly", "vagabond", "afterthought", "belligerent", "arch", "regular", "balance",
  "needle", "voice", "mourn", "blind", "self", "toothbrush", "borrow", "death", "sweet", "match",
  "thread", "whimsical", "cracker", "knot", "bitter", "hurry", "surround", "bow", "deer", "steadfast",
  "female", "pizzas", "gusty", "saw", "uneven", "lyrical", "governor", "knowing", "club", "watch",
  "five", "pet", "towering", "outgoing", "torpid", "bee", "miniature", "dangerous", "pour", "redundant"
};

// Key number i (i < 10000): two words, a different pair for every i. Stored keys use
// sep '-'; keys built with '+' are never stored (lookup misses).
static void makeKey(uint32_t i, char sep, char* out) {
  const char* a = WORDS[i % 100];
  const char* b = WORDS[(i / 100 + 7 * (i % 100)) % 100];
  size_t n = strlen(a);
  size_t m = strlen(b);
  memcpy(out, a, n);
  out[n] = sep;
  memcpy(out + n + 1, b, m);
  out[n + 1 + m] = 0;
}

// Value for key number i: four words from a fixed pseudo-random sequence (the same
// on every board). variant 1 gives other words, and mostly another length.
static void makeValue(uint32_t i, uint32_t variant, char* out) {
  uint32_t x = (uint32_t)((i + 1) * 2654435761UL) ^ (uint32_t)((variant + 1) * 0x9E3779B9UL);
  if (x == 0) x = 1;
  size_t n = 0;
  for (uint8_t w = 0; w < 4; w++) {
    x ^= x << 13;  x ^= x >> 17;  x ^= x << 5;     // xorshift32
    const char* s = WORDS[x % 100];
    size_t m = strlen(s);
    if (w) out[n++] = ' ';
    memcpy(out + n, s, m);
    n += m;
  }
  out[n] = 0;
}

// Removal order "random": step j removes key number (j * 10007) % pairs. 10007 is a
// prime above 10000, so this visits every key exactly once.
static uint32_t shuffled(uint32_t j, uint32_t n) {
  return (uint32_t)((j * 10007UL) % n);
}

// Tells the compiler the buffer is read, so it cannot drop the data generation from
// the untimed twin loops (GCC syntax; every Arduino core uses GCC).
static inline void keep(const char* p) {
  __asm__ __volatile__("" : : "r"(p) : "memory");
}


// ---- heap figures (ESP32 and ESP8266; 0 elsewhere) ---------------------------------
#if defined(ESP32) || defined(ESP8266)
#define BENCH_HEAP
#endif

static uint32_t heapFree() {
#if defined(ESP32)
  return ESP.getFreeHeap() + ESP.getFreePsram();   // internal RAM + PSRAM (0 without PSRAM)
#elif defined(ESP8266)
  return ESP.getFreeHeap();
#else
  return 0;
#endif
}

static uint32_t heapBlock() {                       // largest block that can be allocated
#if defined(ESP32)
  return ESP.getMaxAllocHeap();
#elif defined(ESP8266)
  return ESP.getMaxFreeBlockSize();
#else
  return 0;
#endif
}


// ---- results -------------------------------------------------------------------------
enum {
  R_INSERT, R_RESERVE, R_HIT, R_MISS, R_GET, R_ITER, R_UPDATE, R_JSON, R_JSONP, R_JLOAD,
  R_DESTROY, R_REM_OLD, R_REM_NEW, R_REM_RND, R_COUNT
};
static const char* const CSV_NAMES[R_COUNT] = {
  "insert", "insert_reserve", "lookup_hit", "lookup_miss", "get", "iterate", "update",
  "json", "json_print", "jload", "destroy", "remove_oldest", "remove_newest", "remove_random"
};
static float results[R_COUNT];      // microseconds per operation
static float bytesPerPair;          // size() / pairs after the first insert phase
static float heapPerPair = -1;      // heap used / pairs after the first insert phase
static int   failures;

static void check(bool ok, const char* what) {
  if (ok) return;
  failures++;
  Serial.print(F("  CHECK FAILED: "));
  Serial.println(what);
}

static uint8_t digits(unsigned long v) {
  uint8_t n = 1;
  while (v >= 10) { v /= 10; n++; }
  return n;
}

static void printLeft(const char* s, uint8_t width) {
  Serial.print(s);
  for (size_t n = strlen(s); n < width; n++) Serial.print(' ');
}

static void printRightU(unsigned long v, uint8_t width) {
  for (uint8_t n = digits(v); n < width; n++) Serial.print(' ');
  Serial.print(v);
}

static void printRightF(float v, uint8_t width) {     // 3 decimals
  uint8_t n = digits((unsigned long)(v < 0 ? -v : v)) + 4 + (v < 0 ? 1 : 0);
  for (; n < width; n++) Serial.print(' ');
  Serial.print(v, 3);
}

// One timed phase: `total` microseconds for `ops` operations, less `base`
// microseconds per operation spent generating the data.
static void report(uint8_t id, const char* name, uint32_t ops, unsigned long total, float base) {
  float per = ((float)total - base * (float)ops) / (float)ops;
  results[id] = per;
  Serial.print(F("  "));
  printLeft(name, 34);
  printRightU(ops, 7);
  printRightU(total, 11);
  printRightF(per, 11);
  Serial.println();
}

// Microseconds per key generated, for `n` keys, READ_PASSES times.
static float baseKeys(uint32_t n, char sep, bool shuffle) {
  char k[32];
  unsigned long t = micros();
  for (uint32_t r = 0; r < READ_PASSES; r++) {
    for (uint32_t j = 0; j < n; j++) {
      makeKey(shuffle ? shuffled(j, n) : j, sep, k);
      keep(k);
    }
  }
  return (float)(micros() - t) / (float)(n * READ_PASSES);
}

// Microseconds per key and value generated.
static float basePairs(uint32_t n) {
  char k[32], v[64];
  unsigned long t = micros();
  for (uint32_t i = 0; i < n; i++) {
    makeKey(i, '-', k);
    makeValue(i, 0, v);
    keep(k);
    keep(v);
  }
  return (float)(micros() - t) / (float)n;
}

static int populate(Dictionary& d, uint32_t n) {
  char k[32], v[64];
  int errors = 0;
  for (uint32_t i = 0; i < n; i++) {
    makeKey(i, '-', k);
    makeValue(i, 0, v);
    errors += d.set(k, v) != DICTIONARY_OK;
  }
  return errors;
}

// Every stored value equals the generated one (untimed).
static bool valuesMatch(const Dictionary& d, uint32_t n, uint32_t variant) {
  char k[32], v[64], got[64];
  for (uint32_t i = 0; i < n; i++) {
    makeKey(i, '-', k);
    makeValue(i, variant, v);
    if (d.getString(k, got, sizeof(got)) != strlen(v) || strcmp(got, v) != 0) return false;
  }
  return true;
}

static void reportMemory(const Dictionary& d, uint32_t n, uint32_t heapBefore, uint32_t blockBefore, bool record) {
  Serial.print(F("    memory: size() "));
  Serial.print((unsigned long)d.size());
  Serial.print(F(" bytes ("));
  Serial.print((float)d.size() / (float)n, 1);
  Serial.print(F(" per pair)"));
  if (record) bytesPerPair = (float)d.size() / (float)n;
#ifdef BENCH_HEAP
  uint32_t used = heapBefore - heapFree();
  Serial.print(F(", heap used "));
  Serial.print((unsigned long)used);
  Serial.print(F(" bytes ("));
  Serial.print((float)used / (float)n, 1);
  Serial.print(F(" per pair), largest free block "));
  Serial.print((unsigned long)blockBefore);
  Serial.print(F(" -> "));
  Serial.print((unsigned long)heapBlock());
  if (record) heapPerPair = (float)used / (float)n;
#else
  (void)heapBefore;
  (void)blockBefore;
#endif
  Serial.println();
}

// Writes nothing: times json(Print&) without the cost of a real output.
class NullPrint : public Print {
  public:
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t*, size_t size) { return size; }
};

static void printBoard() {
#if defined(ESP32)
  Serial.print(F("board: "));
  Serial.print(ESP.getChipModel());
  Serial.print(F(", "));
  Serial.print((unsigned long)ESP.getCpuFreqMHz());
  Serial.print(F(" MHz, SDK "));
  Serial.print(ESP.getSdkVersion());
#ifdef ESP_ARDUINO_VERSION_MAJOR
  Serial.print(F(", Arduino core "));
  Serial.print(ESP_ARDUINO_VERSION_MAJOR);
  Serial.print('.');
  Serial.print(ESP_ARDUINO_VERSION_MINOR);
  Serial.print('.');
  Serial.print(ESP_ARDUINO_VERSION_PATCH);
#endif
#elif defined(ESP8266)
  Serial.print(F("board: ESP8266, "));
  Serial.print((unsigned long)ESP.getCpuFreqMHz());
  Serial.print(F(" MHz, SDK "));
  Serial.print(ESP.getSdkVersion());
  Serial.print(F(", Arduino core "));
  Serial.print(ESP.getCoreVersion());
#else
  Serial.print(F("board: not an ESP32 or ESP8266 (no heap figures)"));
#endif
  Serial.println();
}

static void printCsv(uint32_t n) {
  Serial.print(F("CSV,board,mhz,pairs,compress,psram,index_min"));
  for (uint8_t i = 0; i < R_COUNT; i++) {
    Serial.print(',');
    Serial.print(CSV_NAMES[i]);
  }
  Serial.println(F(",bytes_per_pair,heap_per_pair"));

#if defined(ESP32)
  Serial.print(F("CSV,"));
  Serial.print(ESP.getChipModel());
  Serial.print(',');
  Serial.print((unsigned long)ESP.getCpuFreqMHz());
#elif defined(ESP8266)
  Serial.print(F("CSV,ESP8266,"));
  Serial.print((unsigned long)ESP.getCpuFreqMHz());
#else
  Serial.print(F("CSV,other,"));
#endif
  Serial.print(',');
  Serial.print((unsigned long)n);
#ifdef _DICT_COMPRESS
  Serial.print(F(",1"));
#else
  Serial.print(F(",0"));
#endif
#ifdef _DICT_USE_PSRAM
  Serial.print(F(",1,"));
#else
  Serial.print(F(",0,"));
#endif
  Serial.print((unsigned long)_DICT_INDEX_MIN);
  for (uint8_t i = 0; i < R_COUNT; i++) {
    Serial.print(',');
    Serial.print(results[i], 3);
  }
  Serial.print(',');
  Serial.print(bytesPerPair, 1);
  Serial.print(',');
  if (heapPerPair >= 0) Serial.print(heapPerPair, 1);
  Serial.println();
}


// ---- the benchmark ----------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  const uint32_t N = PAIRS;
  const uint32_t R = READ_PASSES;
  char k[32], v[64], buf[64];
  unsigned long t;
  int errors;

  Serial.println();
  Serial.println(F("Dictionary benchmark"));
  printBoard();

  Serial.print(F("build: compress "));
#ifdef _DICT_COMPRESS
  Serial.print(F("on"));
#else
  Serial.print(F("off"));
#endif
  Serial.print(F(", PSRAM "));
#if defined(_DICT_USE_PSRAM) && defined(ESP32)
  Serial.print(psramFound() ? F("on") : F("on, but none found"));
#else
  Serial.print(F("off"));
#endif
  Serial.print(F(", hash index from "));
  Serial.print((unsigned long)_DICT_INDEX_MIN);
  Serial.print(F(" pairs (0 = never), key limit "));
  Serial.print((unsigned long)_DICT_KEYLEN);
  Serial.print(F(", value limit "));
  Serial.println((unsigned long)_DICT_VALLEN);

  uint32_t keyChars = 0, valueChars = 0;
  for (uint32_t i = 0; i < N; i++) {
    makeKey(i, '-', k);
    makeValue(i, 0, v);
    keyChars += strlen(k);
    valueChars += strlen(v);
  }
  Serial.print(F("data: "));
  Serial.print((unsigned long)N);
  Serial.print(F(" pairs, keys "));
  Serial.print((float)keyChars / (float)N, 1);
  Serial.print(F(" chars, values "));
  Serial.print((float)valueChars / (float)N, 1);
  Serial.print(F(" chars on average; read phases run "));
  Serial.print((unsigned long)R);
  Serial.println(F(" passes"));
  Serial.println();

  const float bKey  = baseKeys(N, '-', false);
  const float bMiss = baseKeys(N, '+', false);
  const float bRnd  = baseKeys(N, '-', true);
  const float bPair = basePairs(N);

  const uint32_t heap0  = heapFree();
  const uint32_t block0 = heapBlock();

  Serial.println(F("  phase                                 ops   total us   us per op"));
  {
    Dictionary d;

    // Insert new keys, one at a time, into an empty dictionary.
    errors = 0;
    t = micros();
    for (uint32_t i = 0; i < N; i++) {
      makeKey(i, '-', k);
      makeValue(i, 0, v);
      errors += d.set(k, v) != DICTIONARY_OK;
    }
    t = micros() - t;
    report(R_INSERT, "insert", N, t, bPair);
    reportMemory(d, N, heap0, block0, true);
    check(errors == 0 && d.count() == N, "insert: every set() succeeds, count() == pairs");
    check(valuesMatch(d, N, 0), "insert: every value reads back");
    yield();

    // Lookups of keys that are present, then of keys that are not.
    uint32_t hits = 0;
    t = micros();
    for (uint32_t r = 0; r < R; r++) {
      for (uint32_t i = 0; i < N; i++) {
        makeKey(i, '-', k);
        keep(k);
        hits += d.has(k);
      }
    }
    report(R_HIT, "lookup, key present (has)", N * R, micros() - t, bKey);
    check(hits == N * R, "lookup: every stored key is found");

    hits = 0;
    t = micros();
    for (uint32_t r = 0; r < R; r++) {
      for (uint32_t i = 0; i < N; i++) {
        makeKey(i, '+', k);
        keep(k);
        hits += d.has(k);
      }
    }
    report(R_MISS, "lookup, key missing (has)", N * R, micros() - t, bMiss);
    check(hits == 0, "lookup: no missing key is found");
    yield();

    // Read values by key into a buffer (no heap use).
    uint32_t chars = 0;
    t = micros();
    for (uint32_t r = 0; r < R; r++) {
      for (uint32_t i = 0; i < N; i++) {
        makeKey(i, '-', k);
        keep(k);
        chars += d.getString(k, buf, sizeof(buf));
      }
    }
    report(R_GET, "read value (getString to buffer)", N * R, micros() - t, bKey);
    check(chars == valueChars * R, "read: total length of the values");
    yield();

    // Walk all pairs in insertion order.
    chars = 0;
    t = micros();
    for (uint32_t r = 0; r < R; r++) {
      for (size_t i = 0; i < d.count(); i++) {
        const char* key = d.keyAt(i);
        String value = d.value(i);
        chars += strlen(key) + value.length();
      }
    }
    report(R_ITER, "iterate (keyAt + value)", N * R, micros() - t, 0);
    check(chars == (keyChars + valueChars) * R, "iterate: total length of keys and values");
    yield();

    // Replace every value with a different one.
    errors = 0;
    t = micros();
    for (uint32_t i = 0; i < N; i++) {
      makeKey(i, '-', k);
      makeValue(i, 1, v);
      errors += d.set(k, v) != DICTIONARY_OK;
    }
    report(R_UPDATE, "update (set, other value)", N, micros() - t, bPair);
    check(errors == 0 && d.count() == N, "update: every set() succeeds, count() unchanged");
    check(valuesMatch(d, N, 1), "update: every new value reads back");
    yield();

    // JSON out and back in. Times are per pair.
    {
      t = micros();
      String js = d.json();
      t = micros() - t;
      report(R_JSON, "json() to String, per pair", N, t, 0);
      check(js.length() + 1 == d.jsize(), "json: length + 1 == jsize()");

      NullPrint sink;
      t = micros();
      size_t written = d.json(sink);
      report(R_JSONP, "json(Print&), per pair", N, micros() - t, 0);
      check(written == js.length(), "json(Print&): same length as json()");
      yield();

      Dictionary d2;
      t = micros();
      int8_t rc = d2.jload(js);
      report(R_JLOAD, "jload() from String, per pair", N, micros() - t, 0);
      check(rc == DICTIONARY_OK && d2.count() == N && d2 == d, "jload: loads back equal to the original");
      yield();

      t = micros();
      d2.destroy();
      report(R_DESTROY, "destroy(), per pair", N, micros() - t, 0);
      check(d2.count() == 0, "destroy: count() == 0");
    }
    yield();

    // Remove every pair, oldest first (the documented bulk-delete order).
    t = micros();
    for (uint32_t i = 0; i < N; i++) {
      makeKey(i, '-', k);
      keep(k);
      d.remove(k);
    }
    report(R_REM_OLD, "remove, oldest first", N, micros() - t, bKey);
    check(d.count() == 0, "remove oldest first: dictionary empty");
    yield();

    // Insert again after reserve(): the array and index are allocated once.
    d.destroy();
    const uint32_t heap1  = heapFree();
    const uint32_t block1 = heapBlock();
    errors = 0;
    t = micros();
    errors += d.reserve(N) != DICTIONARY_OK;
    for (uint32_t i = 0; i < N; i++) {
      makeKey(i, '-', k);
      makeValue(i, 0, v);
      errors += d.set(k, v) != DICTIONARY_OK;
    }
    t = micros() - t;
    report(R_RESERVE, "insert after reserve(pairs)", N, t, bPair);
    reportMemory(d, N, heap1, block1, false);
    check(errors == 0 && d.count() == N, "insert after reserve: every call succeeds");
    yield();

    // Remove every pair, newest first.
    t = micros();
    for (uint32_t i = N; i-- > 0;) {
      makeKey(i, '-', k);
      keep(k);
      d.remove(k);
    }
    report(R_REM_NEW, "remove, newest first", N, micros() - t, bKey);
    check(d.count() == 0, "remove newest first: dictionary empty");
    yield();

    // Remove every pair in a scattered order.
    check(populate(d, N) == 0, "insert before random removal: every set() succeeds");
    t = micros();
    for (uint32_t j = 0; j < N; j++) {
      makeKey(shuffled(j, N), '-', k);
      keep(k);
      d.remove(k);
    }
    report(R_REM_RND, "remove, random order", N, micros() - t, bRnd);
    check(d.count() == 0, "remove random order: dictionary empty");

    d.destroy();
  }

#ifdef BENCH_HEAP
  long leftover = (long)heap0 - (long)heapFree();
  Serial.print(F("    heap after destroy(): "));
  Serial.print(leftover);
  Serial.println(F(" bytes below the start"));
  check(leftover <= 64, "heap: memory returned after destroy()");
#endif

  Serial.println();
  if (failures) {
    Serial.print(failures);
    Serial.println(F(" CHECKS FAILED"));
  }
  else {
    Serial.println(F("all checks passed"));
  }
  Serial.println();
  printCsv(N);
}

void loop() {
}
