# Dictionary

## Dictionary data type for Arduino / ESP8266 / ESP32

[![arduino-library-badge](https://www.ardu-badge.com/badge/Dictionary.svg?)](https://www.ardu-badge.com/Dictionary)
[![Unit Tests](https://github.com/arkhipenko/Dictionary/actions/workflows/test.yml/badge.svg)](https://github.com/arkhipenko/Dictionary/actions/workflows/test.yml)
[![Examples Build](https://github.com/arkhipenko/Dictionary/actions/workflows/main.yml/badge.svg)](https://github.com/arkhipenko/Dictionary/actions/workflows/main.yml)
[![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](LICENSE.txt)

A small key-value store for microcontrollers. Keys are short strings; values are
text, integers, floats, booleans or null. Pairs keep their insertion order, lookups
use a hash index, and every pair is a single heap allocation. Built for holding
configuration loaded from JSON on ESP8266 and ESP32, but architecture-independent.

## Table of Contents

- [What is new in 4.0](#what-is-new-in-40)
- [Migrating from 3.x](#migrating-from-3x)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick start](#quick-start)
- [API reference](#api-reference)
- [Typed values and conversion](#typed-values-and-conversion)
- [JSON](#json)
- [Configuration reference](#configuration-reference)
- [Memory](#memory)
- [Compression](#compression)
- [Error codes](#error-codes)
- [Limitations](#limitations)
- [PlatformIO support](#platformio-support)
- [Testing](#testing)
- [Examples](#examples)
- [Benchmarks](#benchmarks)
- [Credits](#credits)
- [License](#license)

## What is new in 4.0

- **One structure, one allocation per pair.** An insertion-ordered array of entries,
  each holding its type, lengths, key and value in one block, plus an optional hash
  index for large dictionaries. The binary tree and the second index array are gone.
- **Order survives removal.** `remove()` keeps the order of the remaining pairs and
  never allocates.
- **Typed values.** `set()` stores text, integers, floats, booleans and null;
  `getInt()`, `getFloat()`, `getBool()`, `getString()` read them back, converting
  when needed. `json()` writes numbers and booleans unquoted.
- **One built-in compressor** for values, used only when it makes a value smaller.
  SHOCO and SMAZ were removed.
- **Rewritten JSON loader**: empty values, standard escapes (including `\uXXXX`),
  a length cap on every token, clear error codes, nested structures rejected.
- **Less memory**: on the benchmark data a 4.0 dictionary without compression is about
  as small as a 3.6 dictionary with compression, and one with compression is about 37%
  smaller than 3.6 without (see [Benchmarks](#benchmarks)).
- `reserve()`, `json(Print&)`, zero-copy `peek()` and `keyAt()`, `const` read methods,
  move semantics.

## Migrating from 3.x

Existing sketches keep compiling. The 3.x calls are kept as wrappers with their 3.x
behavior (values stored as text) and produce a compile-time deprecation warning:

| 3.x call | 4.0 replacement |
|---|---|
| `d.insert(key, value)`, `d(key, value)` | `d.set(key, value)` |
| `d.search(key)` | `d.getString(key)` (or `d[key]`, still supported) |
| `d(key)` (exists?) | `d.has(key)` |
| `d(i)` (i-th key) | `d.key(i)` or `d.keyAt(i)` |
| `d[i]` (i-th value) | `d.value(i)` |

Define `_DICT_NO_DEPRECATION_WARNINGS` to silence the warnings.

Behavior that changed:

- Positions no longer shuffle after `remove()`; the remaining pairs keep their order.
- `d == other` compares types as well as values: the text `"80"` is not equal to the number `80`.
  Two dictionaries that differ only in keys with empty values are no longer reported equal.
- `size()` reports the heap bytes of the new layout; `jsize()` is exact (`json().length() + 1`).
- `jload()` accepts empty values, decodes standard escapes, keeps spaces inside unquoted values
  (`is ok` stays `is ok`), rejects nested objects and arrays, and returns the real error code.
- Copy construction (`Dictionary b(a)`, passing by value) does not compile (since 3.6.1).
  Use assignment or `merge()`; moving works.
- The constructor is `explicit`.
- These build flags are ignored with a `#warning`: `_DICT_CRC`, `_DICT_PACK_STRUCTURES`.
  `_DICT_COMPRESS_SHOCO` and `_DICT_COMPRESS_SMAZ` now enable the built-in compressor (`_DICT_COMPRESS`).
- `BufferStream` (`ReadBufferStream`, `WriteBufferStream`) and the `_LIBDEBUG_` print helpers were removed.
- Keys given with `F()` are not supported; use `String(F("key"))`. Values with `F()` are.

## Requirements

- An Arduino-compatible toolchain (Arduino IDE or PlatformIO) with C++11.
- Primarily targeted at **ESP8266** and **ESP32**, but `architectures=*` / `platforms=*`.
  AVR boards build too, but hold only a handful of pairs in 2 KB of RAM.

## Installation

**Arduino IDE / Library Manager:** open *Tools -> Manage Libraries...*, search for
**Dictionary** by Anatoli Arkhipenko, and click Install.

**PlatformIO:** add it to `platformio.ini`:

```ini
lib_deps =
    arkhipenko/Dictionary
```

**Manual:** download this repository as a ZIP and use *Sketch -> Include Library -> Add .ZIP Library...*.

## Quick start

```c++
#include <Dictionary.h>

Dictionary d;

void setup() {
  Serial.begin(115200);

  d.set("ssid", "my_wifi");
  d.set("port", 80);
  d.set("debug", true);

  Serial.println(d["ssid"]);                 // my_wifi
  Serial.println(d.getInt("port", 8080));    // 80
  Serial.println(d.getInt("retries", 3));    // 3 (missing: default)
  Serial.println(d.json());                  // {"ssid":"my_wifi","port":80,"debug":true}

  d.jload("{\"url\": \"http://ota.home.lan\", \"timeout\": 30}");
  Serial.println(d.getInt("timeout"));       // 30 (stored as text "30", converted on read)

  if (d.has("ssid")) d.remove("ssid");
}

void loop() {}
```

## API reference

`key` is a `const char*` or a `String`. Methods that change the dictionary return an
`int8_t` result code: `0` (`DICTIONARY_OK`) or a negative [error code](#error-codes),
so `if (d.set(...))` tests for failure.

### Writing

| Call | Description |
|---|---|
| `d.set(key, value)` | Insert or replace. `value`: `const char*`, `String`, `F("...")`, `bool`, any integer type, `float`, `double`. A replaced pair keeps its position. |
| `d.setNull(key)` | Store a null value. `d.set(key, (const char*)NULL)` does the same. |
| `d.remove(key)` | Remove a pair. A missing key is not an error. Never allocates. |
| `d.destroy()` | Remove everything and free all memory. The object stays usable. |
| `d.reserve(n)` | Pre-allocate room (array and index) for `n` pairs, e.g. before a large `jload()`. |
| `d.merge(other)` | Copy every pair of `other` into `d`; shared keys take `other`'s value. Stops at the first error. |
| `d = other` | Replace `d` with a copy of `other` (errors are not reported; use `destroy()` + `merge()` for that). |
| `Dictionary b(std::move(a))`, `b = std::move(a)` | Move: `b` takes over; `a` is left empty and usable. |
| `d.jload(json [, n])` | Load pairs from a `const char*`, `String` or `Stream` (at most `n` when `n > 0`). See [JSON](#json). |

### Reading

A missing key, or a value that cannot be converted, returns `def`.

| Call | Returns | Description |
|---|---|---|
| `d.count()` | `size_t` | Number of pairs. |
| `d.has(key)` | `bool` | Whether `key` exists. |
| `d.type(key)` | `DictType` | `DICT_STR`, `DICT_INT`, `DICT_FLOAT`, `DICT_BOOL`, `DICT_NULL` (`DICT_INT64`, `DICT_DOUBLE` with wide numbers), or `DICT_NONE` when missing. |
| `d.getInt(key, def = 0)` | `int32_t` | See [conversion](#typed-values-and-conversion). |
| `d.getFloat(key, def = 0)` | `float` | |
| `d.getBool(key, def = false)` | `bool` | |
| `d.getInt64(key, def)`, `d.getDouble(key, def)` | | With `_DICT_WIDE_NUMBERS`. |
| `d.getString(key, def = "")` / `d[key]` | `String` | Any value as text. |
| `d.getString(key, buf, size)` | `size_t` | Any value as text into your buffer, no heap. Returns the full length (like `snprintf`), so a result `>= size` means it was cut. |
| `d.peek(key)` | `const char*` | Pointer to a plain text value inside the dictionary, no copy; `NULL` for other types and compressed values. Valid until the next change to `d`. |

### Positional access (insertion order)

| Call | Returns | Description |
|---|---|---|
| `d.keyAt(i)` | `const char*` | i-th key, no copy; `NULL` when `i >= count()`. Valid until the next change. |
| `d.typeAt(i)` | `DictType` | `DICT_NONE` when out of range. |
| `d.key(i)` / `d.value(i)` | `String` | i-th key / value as text; empty when out of range. |

Delete everything one by one with `while (d.count()) d.remove(d.keyAt(0));` - or just call `destroy()`.

### JSON and sizes

| Call | Returns | Description |
|---|---|---|
| `d.json()` | `String` | JSON object of all pairs in insertion order. |
| `d.json(out)` | `size_t` | Write the JSON to any `Print` (`Serial`, a `File`, ...) without building a String. |
| `d.jsize()` | `size_t` | `json().length() + 1` (exact). |
| `d.esize()` | `size_t` | Sum of key length + 1 + value text length + 1 (e.g. to size an EEPROM area). |
| `d.size()` | `size_t` | Heap bytes requested by the dictionary (entries, array, index), excluding allocator overhead. |
| `d == other`, `d != other` | `bool` | Same pairs with the same types and values (order ignored). |

## Typed values and conversion

`set()` stores what it is given:

- text as text; `double` as `float` unless `_DICT_WIDE_NUMBERS` is defined;
- integers as `int32_t` (`int64_t` with `_DICT_WIDE_NUMBERS`); an integer outside that range
  is stored as decimal text, so it is never altered;
- `bool` and null take no space beyond the key.

Getters convert on read by default:

| Stored | `getInt` | `getFloat` | `getBool` | `getString` |
|---|---|---|---|---|
| text | the whole text parsed as a decimal integer | the whole text parsed as a number | `true`/`false` (any case), `1`/`0` | as stored |
| int | value | value | `value != 0` | decimal |
| float | value if integral and in range, else `def` | value | `def` | up to `_DICT_FLOAT_DECIMALS` decimals, trailing zeros trimmed |
| bool | 1 / 0 | 1 / 0 | value | `true` / `false` |
| null | `def` | `def` | `def` | `null` |

With `_DICT_STRICT_GET` there is no conversion: `getInt` accepts only integers, `getFloat`
only numbers, `getBool` only booleans; anything else returns `def`. `getString` always works.

## JSON

**Output.** `json()` writes a flat object in insertion order. Text is quoted and escaped
(`"`, `\`, and control characters as `\n`, `\t`, `\r`, `\b`, `\f`, `\u00XX`); numbers,
booleans and null are written unquoted. A float that is `nan` or `inf` is written as `null`.

**Input.** `jload()` reads a lenient, flat JSON-like format:

- Braces are optional. Pairs are separated by `,`, a newline or `}`. A trailing comma is fine.
- Keys and values may be quoted (`"..."`, with standard escapes including `\uXXXX`, written as UTF-8)
  or bare. A bare token runs to the next separator; spaces inside it are kept, spaces around it
  are trimmed. A bare value may contain `:` (URLs work unquoted).
- `#` outside quotes starts a comment that runs to the end of the line.
- Nested objects and arrays are rejected with `DICTIONARY_FMT`.
- Values are stored **as given**, as text, so `json()` of loaded data looks like the input
  (and like 3.x). With `_DICT_TYPED_JSON`, bare numbers, `true`/`false` and `null` are stored typed.
  Quoted values are always text.
- A later duplicate key replaces the earlier value.
- Not atomic: pairs loaded before an error stay in the dictionary.
- A `Stream` is read until `read()` returns -1. Streams that return -1 while more data is still
  coming (e.g. a network client) should be read into a buffer first.

```
# device configuration
{
    "ssid"  : "home network",
    url     : http://ota.home.lan:8080/fw.bin
    port    : 80,
    debug   : true,
}
```

## Configuration reference

Define these before including the header, or pass them as build flags (required in the
[PlatformIO split-header mode](#platformio-support)).

| Define | Default | Effect |
|---|---|---|
| `_DICT_KEYLEN` | `64` | Maximum key length (bytes). |
| `_DICT_VALLEN` | `254` | Maximum text value length (bytes). |
| `_DICT_MAX_ENTRIES` | `65534` | Maximum number of pairs; above 65534 the index uses 32-bit slots. |
| `_DICT_INDEX_MIN` | `32` | Pair count at which the hash index is built. `0` = never (lookups scan the array). |
| `_DICT_USE_PSRAM` | off | ESP32: place entries, array and index in PSRAM when present. |
| `_DICT_TYPED_JSON` | off | `jload()` stores bare numbers, booleans and null typed. |
| `_DICT_STRICT_GET` | off | Typed getters do not convert. |
| `_DICT_WIDE_NUMBERS` | off | `int64_t` and `double` values (`getInt64`, `getDouble`). |
| `_DICT_FLOAT_DECIMALS` | `6` | Decimals when a float is shown as text. |
| `_DICT_COMPRESS` | off | Built-in value compression. |
| `_DICT_CODEC_TABLE` | built-in | Header with a custom token table, e.g. `-D _DICT_CODEC_TABLE='"my_table.h"'`. |
| `_DICT_ASCII_ONLY` | off | `jload()` drops bytes above 127. |
| `_DICT_NO_DEPRECATION_WARNINGS` | off | Silence the 3.x wrapper warnings. |
| `_DICT_HEADER_AND_CPP` | off | PlatformIO / multi-file split-header build. |

## Memory

**Layout.** A dictionary is one array of pointers in insertion order. Each pair is one
heap block: a 4-byte header (type, a hash byte, key length, value length; 6 bytes when
a length limit exceeds 255), the key and a terminating zero, then the value:

| Value | Bytes after the key |
|---|---|
| text | length + 1 (or the compressed length) |
| int, float | 4 (8 for int64 / double) |
| bool, null | 0 |

The `Dictionary` object itself is 24 bytes on 32-bit targets and allocates nothing until
the first pair is stored. The pointer array starts at the constructor's size (10) and
doubles when full.

**Hash index.** Once a dictionary holds `_DICT_INDEX_MIN` pairs (32), a table of 16-bit
slots is built, kept at most 3/4 full: about 2.7 to 5.3 bytes per pair. Below the
threshold, lookups scan the array, which is fast for small dictionaries. The index is
only an accelerator: if it cannot be allocated, lookups keep working by scanning.

**Out of memory.** Every call that allocates returns `DICTIONARY_MEM` on failure and
leaves the dictionary unchanged and usable. `remove()`, `destroy()`, moving and the
constructor never allocate. This is covered by fault-injection tests.

**PSRAM (ESP32).** `#define _DICT_USE_PSRAM` puts entries, array and index in PSRAM when
present, falling back to internal RAM.

**Fragmentation.** One block per pair (3.x used three) means fewer, larger blocks.
Call `reserve(n)` before loading a known number of pairs so the array and index are
allocated once.

## Compression

`#define _DICT_COMPRESS` enables a small token codec for **values**: up to 127 common
fragments (`https://`, `192.168.`, ` the `, `tion`, ...) are each replaced by one byte.
A value is stored compressed only if that makes it smaller, so it never grows. Keys are
never compressed, so lookups never decode anything. There are no scratch buffers.
Compressed values cannot be read with `peek()`; use `getString()`.

Compressed size as a share of the original (lower is better), on data not used to build
the table:

| Data | 4.0 codec | 3.x SHOCO | 3.x SMAZ |
|---|---|---|---|
| benchmark values (4 random English words) | 63.4% | 64.7% | 61.9% |
| benchmark keys (2 random words) | 70.4% | 73.7% | 71.0% |
| typical configuration values | 70.7% | 77.4% | 84.7% |

(4.0 compresses values only; the keys row shows what the codec would do on such text.)

Flash cost on ESP32 (`-Os`): about 2.3 KB, compared with 3.7 KB for SHOCO and 4.6 KB for SMAZ.

**Custom tables.** `extras/codec/make_codec_table.py` builds a table from sample values
(one per line) and writes a header to pass as `_DICT_CODEC_TABLE`.

## Error codes

```c++
#define DICTIONARY_OK         0     // success
#define DICTIONARY_ERR      (-1)    // invalid argument (e.g. key or value length)
#define DICTIONARY_MEM      (-2)    // memory allocation failed
#define DICTIONARY_OOB      (-3)    // does not fit (token longer than the limit, too many pairs)
#define DICTIONARY_COMMA    (-20)   // jload: expected a separator after a value
#define DICTIONARY_COLON    (-21)   // jload: expected ':' after a key
#define DICTIONARY_QUOTE    (-22)   // jload: newline inside a quoted string
#define DICTIONARY_BCKSL    (-23)   // jload: invalid escape sequence
#define DICTIONARY_FMT      (-25)   // jload: malformed input (nested structure, empty key, ...)
#define DICTIONARY_EOF      (-99)   // jload: input ended early (or fewer than n pairs)
```

## Limitations

- **Concurrency.** Read methods are `const` and do not modify the dictionary, so several
  tasks may read at the same time. Any write must be guarded by your own mutex against
  all other access. Do not use it from an ISR.
- **Pointers from `peek()` and `keyAt()`** are valid until the next change to the dictionary.
- **`remove()` is O(n)** (the array shifts, and the index adjusts positions). Removing the
  most recently added pair is O(1). To empty a dictionary use `destroy()`.
- **Flat JSON only**: no nested objects or arrays.
- **Keys** cannot be empty or contain a zero byte; `F()` keys are not supported.

## PlatformIO support

Two steps:

1. Include `DictionaryDeclarations.h` instead of `Dictionary.h` in your code.
2. Define `_DICT_HEADER_AND_CPP` as a **build flag**, for example in `platformio.ini`:

```ini
build_flags =
    -D _DICT_HEADER_AND_CPP
```

The bundled `Dictionary.cpp` then compiles the implementation exactly once. Because it is
a separate translation unit, all other options (`_DICT_KEYLEN`, `_DICT_COMPRESS`, ...)
must also be build flags, so every file sees the same settings.

For the Arduino IDE, leave `_DICT_HEADER_AND_CPP` undefined and `#include <Dictionary.h>`
in one file of the sketch.

## Testing

Native (host) unit tests live in [`tests/`](tests/) and run under
[Google Test](https://github.com/google/googletest). A mock `Arduino.h` lets the library
build and run off-device; see [`tests/README.md`](tests/README.md) for the test plan.

```bash
cd tests
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Add `-DDICT_SANITIZE=ON` at configure time for an AddressSanitizer + UBSan build.
GitHub Actions runs the suite (plain and sanitized) on every push, and compiles the
example sketches on ESP32 and ESP8266 plus the PlatformIO split-header path.

## Examples

- [`Dict_Example01`](examples/Dict_Example01): the 4.0 API end to end (typed values, JSON, merge, copy, move, removal).
- [`Dict_Example02_ESP32_PSRAM`](examples/Dict_Example02_ESP32_PSRAM): a 10,000-pair load, lookup and delete benchmark in PSRAM.
- [`Dict_Benchmark`](examples/Dict_Benchmark): times every operation on the board it runs on (insert, lookup, read,
  iterate, update, JSON out and in, each removal order) and reports heap use per pair. It checks its own results and
  ends with a CSV line for comparing boards and build options. ESP32, ESP8266 and other boards with enough RAM.

## Benchmarks

1000 pairs of the benchmark data (keys: two random English words, about 13.5 characters;
values: four random words, about 27.7 characters). Bytes per pair are an ESP32 estimate
computed from struct sizes measured with the ESP32 compiler and the actual encoded lengths;
allocations are measured on the host.

| Version | Bytes per pair | Heap blocks per pair |
|---|---|---|
| 3.6, no compression | 72.4 | 3 |
| 3.6, SHOCO | 57.4 | 3 |
| 3.6, SMAZ | 56.3 | 3 |
| **4.0, no compression** | **56.5** | **1** |
| **4.0, `_DICT_COMPRESS`** | **45.4** | **1** |

Heap block headers (allocator overhead, a few bytes per block) come on top, so the
difference on a real device is larger than the table shows.

Speed, measured on a PC (x86-64) with 10,000 pairs, relative only (run
[`Dict_Benchmark`](examples/Dict_Benchmark) for device numbers):

| Operation | 3.6 | 4.0 |
|---|---|---|
| insert | 0.35 us | 0.11 us |
| lookup | 0.21 us | 0.08 us |
| remove, oldest pair first | 1.1 us | 6.7 us |
| remove, newest pair first | 1.6 us | 0.06 us |

## Credits

- The value codec follows the static-codebook idea of **SMAZ** by Salvatore Sanfilippo
  ([here](https://github.com/antirez/smaz)); the table and code are new.
- Key hashing uses **FNV-1a** by Glenn Fowler, Landon Curt Noll and Kiem-Phong Vo (public domain).
- Earlier versions used a modified **QueueArray** by Efstathios Chatzikyriakidis and the
  **SHOCO** and **SMAZ** compression libraries.

## License

Distributed under the BSD 3-Clause License. See [`LICENSE.txt`](LICENSE.txt) for the full text.
