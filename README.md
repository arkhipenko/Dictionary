# Dictionary

## Dictionary data type for Arduino / ESP8266 / ESP32

[![arduino-library-badge](https://www.ardu-badge.com/badge/Dictionary.svg?)](https://www.ardu-badge.com/Dictionary)
[![Unit Tests](https://github.com/arkhipenko/Dictionary/actions/workflows/test.yml/badge.svg)](https://github.com/arkhipenko/Dictionary/actions/workflows/test.yml)
[![Examples Build](https://github.com/arkhipenko/Dictionary/actions/workflows/main.yml/badge.svg)](https://github.com/arkhipenko/Dictionary/actions/workflows/main.yml)
[![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](LICENSE.txt)

A small, flexible **Dictionary** (hash / key-value) data type for microcontrollers.
Keys and values are `String` objects / character strings, backed by a fast
binary-tree search. Built for storing JSON and configuration key-value pairs on
ESP8266 and ESP32, but architecture-independent.

## Table of Contents

- [Background](#background)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick start](#quick-start)
- [API reference](#api-reference)
- [Usage](#usage)
  - [Creation](#creation)
  - [Populate key-value pairs](#populate-key-value-pairs)
  - [Loading from JSON](#loading-from-json)
  - [Lookup values](#lookup-values)
  - [Lookup keys](#lookup-keys)
  - [Information and compare](#information-and-compare)
  - [Deleting key-value pairs](#deleting-key-value-pairs)
- [PlatformIO support](#platformio-support)
- [Configuration reference](#configuration-reference)
- [Memory management](#memory-management)
- [Error codes](#error-codes)
- [Limitations](#limitations)
- [Testing](#testing)
- [Examples](#examples)
- [Benchmarking](#benchmarking)
- [Stress test](#stress-test)
- [Credits and references](#credits-and-references)
- [License](#license)

## Background

I was looking for a small, but flexible class implementing **Dictionary** or **Hash** data type to use on ESP8266 or ESP32 microcontrollers.  In the end just decided to write my own.

I needed this to work with JSON files and configuration key-value parameters like

```
"ssid" = "your_wifi"
"ota_url" = "http://some.url"
```
This dictionary only works with `String` objects and character strings (char arrays).

**As of v3.3.0 you can load JSON strings with values without quotation marks, but they still would be converted to Strings internally.**

Under the hood is a binary-tree structure based on the reinterpretation of the first 2 (CRC16), 4 (CRC32) or 8 (CRC64) bytes of the key string (padded with 0's). As of version 3.0.0  I dropped use of actual CRC calculation as an unnecessary overhead in both space and calculation time.

To choose how many bytes represent a "key", define one of the following constants: (note, CRC 32 is a default if you do not define anything).

```c++
#define _DICT_CRC 16
#define _DICT_CRC 32   // this is the default
#define _DICT_CRC 64
```

Version 3 also allocates as little memory to each node as possible given specific key and value maximum lengths. For instance a max key length of 64 characters and max value length of 254 characters (which are the defaults) only requires 1 byte for keeping track of the respective lengths.

You can control the maximum allowable length of both keys and values via the following parameters:

```c++
#define _DICT_KEYLEN 64
#define _DICT_VALLEN 254
```

**NOTE:** the actual buffer length is one byte longer to accommodate the terminating '\0' for the string.

## Requirements

- An Arduino-compatible toolchain (Arduino IDE or PlatformIO).
- Primarily targeted at **ESP8266** and **ESP32**, but `architectures=*` / `platforms=*` - it will build anywhere the Arduino `String` class is available.
- Because keys and values are stored as `String`/char arrays on the heap, RAM-tight cores (e.g. AVR Uno with 2 KB SRAM) can only hold a handful of entries. The ESP cores are the intended home.

## Installation

**Arduino IDE / Library Manager:** open *Tools -> Manage Libraries...*, search for **Dictionary** by Anatoli Arkhipenko, and click Install. Or use [ardu-badge](https://www.ardu-badge.com/Dictionary).

**PlatformIO:** add it to `platformio.ini`:

```ini
lib_deps =
    arkhipenko/Dictionary
```

**Manual:** download this repository as a ZIP and use *Sketch -> Include Library -> Add .ZIP Library...* in the Arduino IDE.

## Quick start

```c++
#include <Dictionary.h>

Dictionary d;                    // or: Dictionary *d = new Dictionary();

void setup() {
  Serial.begin(115200);

  d("ssid", "my_wifi");          // insert (operator () form)
  d.insert("port", "80");        // insert (method form)

  Serial.println(d["ssid"]);     // -> my_wifi
  Serial.println(d.count());     // -> 2
  Serial.println(d.json());      // -> {"ssid":"my_wifi","port":"80"}

  if (d("ssid")) Serial.println("ssid exists");

  d.remove("port");
}

void loop() {}
```

## API reference

`d` is a `Dictionary` instance; `dict` is a `Dictionary*` pointer.

| Call | Returns | Description |
|------|---------|-------------|
| `d(key, val)` / `d.insert(key, val)` | `int8_t` | Insert a new pair or update an existing key. `0` = OK, negative = error. |
| `d[key]` / `d.search(key)` | `String` | Value for `key`, or `""` if the key is absent. |
| `d(key)` | `bool` | `true` if `key` exists. |
| `d(i)` / `d.key(i)` | `String` | The i-th key (insertion order; see note under Deleting). |
| `d[i]` / `d.value(i)` | `String` | The i-th value (insertion order). |
| `d.count()` | `size_t` | Number of key-value pairs. |
| `d.remove(key)` | `int8_t` | Delete a key-value pair. |
| `d.destroy()` | `void` | Remove every pair (see fragmentation note). |
| `d.merge(other)` | `int8_t` | Copy all pairs from `other` into `d`. |
| `d == other` / `d != other` | `bool` | Content comparison. |
| `d = other` | | Replace contents of `d` with those of `other`. |
| `d.json()` | `String` | JSON representation of the dictionary. |
| `d.jload(src [, n])` | `int8_t` | Parse JSON from a `String` or `Stream` (optionally only the first `n` pairs). |
| `d.jsize()` | `size_t` | Lower-bound estimate of `json()` length (for pre-allocation). |
| `d.size()` | `size_t` | Bytes of stored key/value data. |
| `d.esize()` | `size_t` | Bytes needed to serialize keys+values (e.g. for EEPROM). |

Memory-allocating calls (`insert`, `remove`, `jload`, `merge`, `operator()`) return `int8_t` status codes - see [Error codes](#error-codes).

## Usage

### Creation:

`Dictionary *dict = new Dictionary();` **or**

`Dictionary *dict = new Dictionary(N);`  - where N is the initial size of the dictionary (10 is a default), **or**

`Dictionary &d = *(new Dictionary(6));`

### Populate key-value pairs:

```c++
d("ssid", "devices");
d("pwd", "********");
d("url", "http://ota.home.lan");
d("port", "80");
d("plumless", "plumless");
d("buckeroo", "buckeroo");  // OR:
dict->insert("buckeroo", "buckeroo");
```

If a key does not exist, a new key-value pair is created. If a key exists, the value is updated. So

`d("buckeroo", "buckeroonew");` will replace the old value of **buckeroo** for the same key with a new value of **buckeroonew**

`d.merge(a)` will merge key-value pairs from dictionary a into dictionary d. This could be used as a copy operation, just need to make sure that d is empty beforehand.

Assignment does the copy for you: `d = a;` replaces the entire contents of `d` with those of `a` (internally it calls `destroy()` then `merge()`).

### Loading from JSON

`String s = "{\"ssid\":\"devices\",\"pwd\":\"********\"}";`

`d.jload(s)` will populate dictionary `d` from the JSON string `s`.

`d.jload(s, 2)` will load only first 2 key/value pairs.

`d.jload(file)` will populate dictionary from an opened `FILE Stream`, or any `Stream` for that matter.

**NOTE**: as of version 3.2.0 JSON strings can contain comments (lines starting with a `#` symbol).

E.g., :

```json
# This JSON file contains comments
{
    "key" : "value", # line comments are supported as well
}
```

**NOTE:** if compiled with the option `#define _DICT_ASCII_ONLY` jload will ignore all non-ASCII characters in the incoming stream.

This is a valid JSON load as well:

```
{
    "value" : 3,
	anothervalue : "23",
	and_this : is_ok_too,
}
```

All of the above will result in a dictionary:

`{"value":"3","anothervalue":"23","and_this":"is_ok_too"}`

Since `jload` allocates memory, check its return code when parsing untrusted input:

```c++
int8_t rc = d.jload(s);
if (rc) {
  // rc is a negative error code - malformed JSON or out of memory
}
```

### Lookup values:

`d["port"]`  will return "80"

`d["mqtt_url"]` will return an empty String as the key does not exist yet.   **or**

`dict->search("port")`  will return "80"

`d[0]` will return "devices"

`d[3]` will return "80"

`dict->value(4)` will return "buckeroo"

### Lookup keys:

`d(0)` will return "ssid"

`d(1)` will return "pwd"

`d(10)` will return an empty string as this is out of current bounds.   **or**

`dict->key(0)` will return "ssid";

**NOTE**: Indexes are assigned in the order the key-values are inserted. You **cannot** assign `d(0, "test")`

### Information and compare:

`if (d == a)` will return true if dictionaries are identical

`if (d != a)` will return true if dictionaries are not identical

`d("ssid")` will return **true** indicating that the key "ssid" exists in the dictionary

`d("something-else")` will return **false** indicating that the key "something-else" does not exist in the dictionary

`d.count()` returns a number of key-value pairs in the dictionary

`d.size()` returns combined lengths (in bytes) of all key and value strings (including trailing zeros) and other dictionary node elements. Basically this is how much space the dictionary data (not the C++ object, only the data elements) take up.

`d.jsize()` returns size of a JSON representation of the dictionary, as will be returned by the next method, so you can pre-allocate space if necessary. Zero terminator included.

`d.json()` returns a String with a JSON representation of the dictionary.

```c++
Example:

d.json(): {"ssid":"devices","pwd":"********","url":"http://ota.home.lan","port":"80","plumless":"plumless value","buckeroo":"buckeroo value"}
d.jsize(): 132
```

**NOTE**: as of version 3.6.0 `json()` escapes both double-quotes (`"`) and backslashes (`\`) inside keys and values, so a value like `the answer is "no".` produces valid JSON. `jsize()` returns a *lower-bound* estimate and does not count the extra escape characters, so treat its result as a reservation hint rather than an exact length when your data contains `"` or `\`.

### Deleting key-value pairs

You can nuke the entire dictionary with a `d.destroy()` method, but I recommend deleting and recreating the object instead due to memory fragmentation issues.

If you absolutely have to delete nodes:

`d.remove("url")` will remove the key "url" and value "http://ota.home.lan" from the dictionary.

**A very important NOTE:** the key indexes are assigned as entries are added to the dictionary. However, due to deletion algorithm, once you delete a single key, the order of the keys is changed and no longer corresponds to the original order. In other words, once you start using deletion, the index of the keys is arbitrary. **Also**, as you delete the entries, the count and indexes of individual entries change. Be very careful how you deal with that in a loop. Once you delete entry *i*, the entry *i+1* may change and be not what you expected. For instance, the right way to delete all entries in a loop is:

```c++
int cnt = d.count();
for (int i=0; i < cnt; i++) {
  d.remove(d(0));
}
```

Note how you always delete *index 0*, and not *index i*, since once the entry at index 0 is removed, some other entry becomes entry with index 0, and removing entry "i" will lead to skipping entries and not deleting the entire dictionary.

An even better way to delete every entry would be:

```c++
while ( d.count() ) d.remove(d(0));
```

## PlatformIO support

As of version 3.6.0 platform.io (and any non-Arduino-IDE build) is supported the same way as the TaskScheduler library, and you no longer need to hand-create a `Dictionary.cpp` file.

Two steps:

1. In your code, include `DictionaryDeclarations.h` instead of `Dictionary.h`.

2. Define `_DICT_HEADER_AND_CPP` as a **build flag** (not a `#define` in the sketch). For example, in `platformio.ini`:

```ini
build_flags =
    -D _DICT_HEADER_AND_CPP
```

The `Dictionary.cpp` bundled with the library then compiles the implementation exactly once. Because the bundled `Dictionary.cpp` is a separate translation unit, any other configuration options (`_DICT_CRC`, `_DICT_KEYLEN`, `_DICT_COMPRESS_SHOCO`, etc.) must also be passed as build flags so they are visible both to your code and to the bundled implementation.

For the Arduino IDE, nothing changes: leave `_DICT_HEADER_AND_CPP` undefined and simply `#include <Dictionary.h>` in your sketch.

## Configuration reference

All options are `#define`s that must appear **before** including the header (or be passed as build flags - required in the PlatformIO split-header mode above).

| Define | Default | Effect |
|--------|---------|--------|
| `_DICT_CRC` | `32` | Key-prefix width: `16`, `32`, or `64` bits. |
| `_DICT_KEYLEN` | `64` | Maximum key length (bytes). |
| `_DICT_VALLEN` | `254` | Maximum value length (bytes). |
| `_DICT_USE_PSRAM` | off | Allocate objects in ESP32 PSRAM when present. |
| `_DICT_PACK_STRUCTURES` | off | Pack structs to save RAM at a small speed cost. |
| `_DICT_COMPRESS_SHOCO` | off | Enable SHOCO key/value compression. |
| `_DICT_COMPRESS_SMAZ` | off | Enable SMAZ key/value compression. |
| `_DICT_ASCII_ONLY` | off | `jload` ignores non-ASCII input bytes. |
| `_DICT_HEADER_AND_CPP` | off | PlatformIO / non-Arduino-IDE split-header build. |
| `_LIBDEBUG_` | off | Enable `Serial`-based debug printing. |

`_DICT_COMPRESS_SHOCO` and `_DICT_COMPRESS_SMAZ` are mutually exclusive.

## Memory management:

### Footprint

Each `Dictionary` object requires 28 bytes (22 for packed structures) for itself + 20 bytes for the underlying `NodeArray` object.

If you use compression, the Dictionary needs to allocate space for compressing / decompressing strings equal to your `_DICT_KEYLEN` and `_DICT_VALLEN` settings.

For every key/value pair a new `node` object is created (24 bytes (18 bytes for compressed structures)) and space for key and value strings is allocated equal to the length of key and value strings + a few bytes for storing length (the amount of bytes depends on the `_DICT_KEYLEN` and `_DICT_VALLEN` settings - the Dictionary allocates 1, 2 or 4 bytes as necessary. The minimal number of bytes for the key string depends on the `_DICT_CRC` setting and will be 2, 4, or 8 bytes respectively).

**Example**:

Unpacked structures, CRC32, no compression, default `_DICT_KEYLEN` and `_DICT_VALLEN` settings (64 and 254 bytes respectively)

key/value = "this is a key"  : "This is a Value"

memory footprint:

- Dictionary object = 28 bytes
- NodeArray object = 20 bytes
- 1 x node object = 24 bytes
- 1 x key string = (13 + 1) bytes
- 1 x value string = (15 + 1) bytes
- TOTAL: **102 bytes**

By default Dictionary allocates `NodeArray` space for 10 nodes and grows it geometrically as needed. Each key/value is allocated upon insertion.

### DRAM vs. PSRAM

Dictionary allocates all its objects on the Heap. For ESP32 microcontrollers specifically there is an option to use PSRAM (if present) as a storage:

```c++
#define _DICT_USE_PSRAM
```

will make Dictionary try to allocate all its objects in the PSRAM.

### Structure packing

By default ESP microcontrollers allocate memory aligned with 4 byte boundaries for faster memory operations. The overhead could become significant if you create many key-value pairs.  Compiling the library with `#define _DICT_PACK_STRUCTURES` compile option will use packed structures at the very slight performance expense.

For instance:

Creating and accessing and deleting 1000 key-value pairs on ESP32 running at 240MHz:

```
Without packing:	memory: 35964 bytes, lookup: ~76 micros/lookup
With packing:		memory: 34844 bytes, lookup: ~94 micros/lookup
```

### Compression

As of version 3.1.0 Dictionary supports small string compression. Two algorithms are supported:

SHOCO (**SHO**rt string **CO**mpression) - based on the compression library [here](https://github.com/Ed-von-Schleck/shoco). And

SMAZ (**SMA**ll **Z**ip) - based on  the compression library [here](https://github.com/antirez/smaz).

While SMAZ's dictionary is static, SHOCO allows creation of custom dictionaries based on the type of strings you plan to use in the dictionary. Dictionary is shipped with a model based on typical configuration files. My benchmarks show that you can achieve 7% to 16% compression ratio depending on the data, but a more thorough analysis is required.

**NOTE**: when enabled, compression requires additional memory allocation to store compressed and uncompressed keys. The combined memory overhead is (Max Key Length + Max Value Length + 2) bytes, or 64 + 254 + 2 = 320 bytes for default settings.

The following compile directives will enable SHOCO and SMAZ respectively at the compile time:

```c++
#define _DICT_COMPRESS_SHOCO
#define _DICT_COMPRESS_SMAZ
```

### Out of memory error handling

All methods that allocate memory are enabled to return error codes in case memory allocation fails.  Typically a success code is '0', so a simple comparison like this would be sufficient:

```c++
if ( d("key", "value") ) {
	// this is an error situation on insert
}
// memory allocation was successful
```

The following methods return error codes in case of out-of-memory situation: `insert, remove, jload, merge, operator ()`

As of version 3.6.0 every allocating path is exercised by `malloc` fault-injection tests (run under AddressSanitizer): on an allocation failure the operation returns a negative error code and the dictionary is left uncorrupted - it never crashes or leaks, and existing entries stay intact.

## Error codes

```C++
#define DICTIONARY_OK    	0	   // operation successful
#define DICTIONARY_ERR   	(-1)   // general error
#define DICTIONARY_MEM   	(-2)   // failed memory allocation
#define DICTIONARY_OOB		(-3)   // compressed string does not fit into buffer
#define DICTIONARY_COMMA    (-20)  // json conversion error - expected a comma
#define DICTIONARY_COLON    (-21)  // json conversion error - expected a colon
#define DICTIONARY_QUOTE    (-22)  // json conversion error - expected a quote
#define DICTIONARY_BCKSL    (-23)  // json conversion error - expected a back slash
#define DICTIONARY_FMT      (-25)  // json conversion error - malformed format
#define DICTIONARY_EOF      (-99)  // json conversion error - unexpected end of string
```

## Limitations

- **Not thread-safe.** Read operations (`search`, `key`, `value`, `json`) share internal temporary buffers, so concurrent access from multiple FreeRTOS tasks or from an ISR on ESP32 must be guarded by your own mutex.
- **Performance is that of an unbalanced binary search tree.** Lookups, inserts, and deletes average ~O(log n), but degrade toward O(n) when keys are inserted in sorted key-prefix order (e.g. `key0`, `key1`, `key2`, ...). As of version 3.6.0 traversal is iterative, so a deep/degenerate tree no longer risks a stack overflow, but the time cost remains.
- **No quoting inside keys via positional JSON building.** `json()` handles escaping for you; if you build JSON by hand from `key(i)`/`value(i)`, remember to escape it yourself.
- **After any `remove()`, positional order is arbitrary** - see the [Deleting](#deleting-key-value-pairs) note.

## Testing

Native (host-side) unit tests live in [`tests/`](tests/) and run under
[Google Test](https://github.com/google/googletest). A mock `Arduino.h` lets the
header-only library be compiled and exercised off-device, so logic is validated
on every push - see [`tests/README.md`](tests/README.md) for the full test plan.

```bash
cd tests
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Add `-DDICT_SANITIZE=ON` at configure time for an AddressSanitizer + UBSan build.
GitHub Actions runs the suite (plain and sanitized) on every push, and separately
compiles the example sketches on ESP32/ESP8266 plus the PlatformIO split-header
path.

## Examples

### Creating a simple JSON config file:

``` c++
Serial.println("{");
for (int i = 0; i < d.count(); i++) {
   Serial.print("\t\"");
   Serial.print(d(i));
   Serial.print("\" : \"");
   Serial.print(d[i]);
   if (i < d.count() - 1) Serial.println("\",");
   else Serial.println();
}
Serial.println("}");
```

will produce this:

```json
{
	"ssid" : "devices",
	"pwd" : "********",
	"url" : "http://ota.home.lan",
	"port" : "80",
	"plumless" : "plumlessnew",
	"buckeroo" : "buckeroonew"
}
```

The repository ships two complete sketches under [`examples/`](examples/):
`Dict_Example01` (general functionality) and `Dict_Example02_ESP32_PSRAM` (PSRAM usage on ESP32).

## Benchmarking:

Using random configuration-*like* key/value pairs (ex: `suggestion : ftp://toothbrush.health.org/elastic`, around. 40 characters long) generated from a set of 1000 random words (timings are in *microseconds*).

**NOTE:** these figures were measured on the v3.1.x code base. In v3.6.0 the lookup/insert/delete paths became iterative; the numbers below are indicative rather than exact for the current release.

```
Scenario:									        lookup  delete   size
=========================================================================================================
Esp8266, no compression, DRAM, 300 keys, default key length				76.09	83.31	 8364
Esp8266, shoco compression (default model), DRAM, 300 keys, default key length		108.03	109.22	 7274
Esp8266, shoco compression (config model), DRAM, 300 keys, default key length		108.18	109.18	 7137
Esp8266, shoco compression (dedicated model), DRAM, 300 keys, default key length	114.01	115.02	 6960
Esp8266, smaz compression, DRAM, 300 keys, default key length				128.75	125.76	 7049

ESP32, no compression, DRAM, 1000 keys, default key length, packed			94.67	17.48	34844
ESP32, no compression, DRAM, 1000 keys, default key length				76.23	15.49	35964
ESP32, shoco compression (default model), DRAM, 1000 keys, default key length		73.33	19.59	32216
ESP32, shoco compression (config model), DRAM, 1000 keys, default key length		73.11	19.26	32084
ESP32, smaz compression, DRAM, 1000 keys, default key length				73.87	20.84	32140
```

## Stress test:

On **ESP8266** I was able to create ~400 entries and print them in a simple JSON format before starting to run into memory issues. Not a bad result for a small microcontroller.

On **ESP32** around 2000 key/value pairs fit in the DRAM, and about 30000 in 4Mb of PSRAM.

**NOTE:**  not all DRAM is available to allocation on ESP devices:

*Due to a technical limitation, the maximum statically allocated DRAM usage is 160KB. The remaining 160KB (for a total of 320KB of DRAM) can only be allocated at runtime as heap.*  Reference is [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/mem_alloc.html).

## Credits and references:

- **QueueArray** (modified) - by Efstathios Chatzikyriakidis ([here](https://playground.arduino.cc/Code/QueueArray/))
- **Binary Trees in C++** - by Alex Allain ([here](https://www.cprogramming.com/tutorial/lesson18.html))
- **Deletion of b-tree entries** article ([here](https://www.geeksforgeeks.org/binary-search-tree-set-2-delete/))

That should be everyone. Apologies if I missed anyone - will update as soon as I remember.

## License

Distributed under the BSD 3-Clause License. See [`LICENSE.txt`](LICENSE.txt) for the full text.

## Enjoy!
