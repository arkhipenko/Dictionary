# Dictionary

## Dictionary data type

[![arduino-library-badge](https://www.ardu-badge.com/badge/Dictionary.svg?)](https://www.ardu-badge.com/Dictionary)

### Background

I was looking for a small, but flexible class implementing **Dictionary** or **Hash** data type to use on ESP8266 or ESP32 microcontrollers.  In the end just decided to write my own. 

I needed this to work with JSON files and configuration key-value parameters like

```
"ssid" = "your_wifi"
"ota_url" = "http://some.url"
```
This dictionary only works with `String` objects and character strings (char arrays)

**As of v3.3.0 you can load JSON strings with values without quotation marks, but they still would be converted to Strings internally.**

Under the hood is a binary-tree structure based on the reinterpretation of the first 2 (CRC16), 4 (CRC32) or 8 (CRC64) bytes of the key string (padded with 0's). As of version 3.0.0  I dropped use of actual CRC calculation as an unnecessary overhead in both space and calculation time. 

To choose how many bytes represent a "key", define one of the following constants: (note, CRC 32 is a default if you do not define anything).

```c++
#define _DICT_CRC 16
#define _DICT_CRC 32   --> this is the default
#define _DICT_CRC 64
```

Version 3 also allocates as little memory to each node as possible given specific key and value maximum lengths. For instance a max key length of 64 characters and max value length of 254 characters (which are the defaults) only requires 1 byte for keeping track of the respective lengths. 

You can control the maximum allowable length of both keys and values vis the following parameters:

```c++
#define _DICT_KEYLEN 64
#define _DICT_VALLEN 254
```

**NOTE:** the actual buffer length is one byte longer to accommodate the terminating '\0' for the string. 



### Usage:

#### Creation:

`Dictionary *dict = new Dictionary();` **or**

`Dictionary *dict = new Dictionary(N);`  - where N is the initial size of the dictionary (10 is a default), **or**

`Dictionary &d = *(new Dictionary(6));`



#### Populate key-value pairs:

```c++
d("ssid", "devices");
d("pwd", "********");
d("url", "http://ota.home.lan")
d("port", "80");
d("plumless", "plumless");
d("buckeroo", "buckeroo");  // OR:
dict->insert("buckeroo", "buckeroo");
```

If a key does not exist, a new key-value pair is created. If a key exists, the value is updated. So

`d("buckeroo", "buckeroonew");` will replace the old value of **buckeroo** for the same key with a new value of **buckeroonew**

`d.merge(a)` will merge key-value pairs from dictionary a into dictionary d. This could be used as copy operator, just need to make sure that d is empty beforehand. s

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



#### Lookup values:

`d["port"]`  will return "80"

`d["mqtt_url"]` will return an empty String as the key does not exist yet.   **or**

`dict->search("port")`  will return "80"

`d[0]` will return "devices"

`d[3]` will return "80"

`dict->value(4)` will return "buckeroo"



#### Lookup keys:

`d(0)` will return "ssid"

`d(1)` will return "pwd"

`d(10)` will return an empty string as this is out of current bounds.   **or**

`dict->key(0)` will return "ssid";

**NOTE**: Indexes are assigned in the order the kay-values are inserted. You **cannot** assign `d(0, "test")`



#### Information and compare:

`if (d == a)` will return true if dictionaries are identical

`if (d != a)` will return true if dictionaries are not identical

`d("ssid")` will return **true** indicating that the key "ssid" exists in the dictionary

`d("something-else")` will return **false** indicating that the key "something-else" does not exist in the dictionary

`d.count()` returns a number of key-value pairs in the dictionary 

`d.size()` returns combined lengths (in bytes) of all key and value strings (including trailing zeros) and other dictionary node elements. Basically this is how much space the dictionary date (not the C++ object, only the data elements) take up. 

`d.jsize()` returns size of a JSON representation of the dictionary, as will be returned by the next method, so you can pre-allocate space if necessary. Zero terminator included.

`d.json()` returns a String with a JSON representation of the dictionary. 

```c++
Example:

d.json(): {"ssid":"devices","pwd":"********","url":"http://ota.home.net","port":"80","plumless":"plumless value","buckeroo":"buckeroo value"}
d.jsize(): 132
```

**NOTE**: currently `json()` and `jsize()` methods are not taking potential quotation marks inside keys or values into account, so it is better not to use such values with JSON  functionality. 

For instance a value `"the answer is \"no\"."` will probably break JSON functionality.



#### Deleting kay-value pairs

You can nuke the entire dictionary with a `d.destroy()` method, but I recommend deleting and recreating the object instead due to memory fragmentation issues.

If you absolutely have to delete nodes:

`d.remove("url")` will remove the key "url" and value "http://ota.home.lan" from the dictionary.

**A very important NOTE:** the key indexes are assigned as entries are added the dictionary. However, due to deletion algorithm, once you delete a single key, the order of the keys is changed and no longer corresponds to the original order. In other words, once you start using deletion, the index of the keys is arbitrary. **Also**, as you delete the entries, the count and indexes of individual entries change. Be very careful how you deal with that in a loop. Once you delete entry *i*, the entry *i+1* may change and be not what you expected. For instance, the right way to delete all entries in a loop is:

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



### Platform.io Support

As of version 3.5.0 support for platform.io system has been simplified:

In your programs, use `DictionaryDeclarations.h` include instead of the `Dictionary.h` one.

In your `src` folder add this file: 

File: `Dictionary.cpp`:

```c++
#include <Dictionary.h>
```

that's it. 



### Memory management:

#### Footprint

Each `Dictionary` object requires 28 bytes (22 for packed structures) for itself + 20 bytes for underlying `NodeArray` obiect.

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

By default Dictionary allocates `NodeArray` space for 10 nodes. Each key/value is allocated upon insertion. 

#### DRAM vs. PSRAM

Dictionary allocates all its objects on the Heap. For ESP32 microcontrollers specifically there is an option to use PSRAM (if present) as a storage:

```c++
#define _DICT_USE_PSRAM
```

will make Dictionary try to allocate all its objects in the PSRAM.

#### Structure packing

By default ESP microcontrollers allocate memory aligned with 4 byte boundaries for faster memory operations. The overhead could become significant if you create many key-value pairs.  Compiling the library with `#define _DICT_PACK_STRUCTURES` compile option will use packed structures as the very slight performance expense. 

For instance:

Creating and accessing and deleting 1000 key-value pairs on ESP32 running at 240MHz:

```
Without packing:	memory: 35964 bytes, lookup: ~76 micros/lookup
With packing:		memory: 34844 bytes, lookup: ~94 micros/lookup
```

#### Compression

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



#### Out of memory Error Handling

All methods that allocate memory are enabled to return error codes in case memory allocation fails.  Typically a success code is '0', so a simple comparison like this would be sufficient:

```c++
if ( d("key", "value") ) {
	// this is an error situation on insert
}
// memory allocation was successful
```

The following methods return error codes in case of out-of-memory situation: `insert, remove, jload, merge, operator ()`

### Error Codes:

```C++
#define DICTIONARY_OK    	0	   // operation successful
#define DICTIONARY_ERR   	(-1)   // genaral error
#define DICTIONARY_MEM   	(-2)   // failed memory allocation
#define DICTIONARY_OOB		(-3)   // compressed string does not fit into buffer
#define DICTIONARY_COMMA    (-20)  // json conversion error - expected a comma
#define DICTIONARY_COLON    (-21)  // json conversion error - expected a colon
#define DICTIONARY_QUOTE    (-22)  // json conversion error - expected a quote
#define DICTIONARY_BCKSL    (-23)  // json conversion error - expected a back slash
#define DICTIONARY_EOF      (-99)  // json conversion error - unexpected end of string
```



### Examples:

#### Creating a simple JSON config file:

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



### Credits and References: 

- **QueueArray** (modified) - by Efstathios Chatzikyriakidis ([here](https://playground.arduino.cc/Code/QueueArray/))
- **Binary Trees in C++** - by Alex Allain ([here](https://www.cprogramming.com/tutorial/lesson18.html))
- **Deletion of b-tree entries** article ([here](https://www.geeksforgeeks.org/binary-search-tree-set-2-delete/))

That should be everyone. Apologies if I missed anyone - will update as soon as I remember. 



### Stress test:

On **ESP8266** I was able to create ~400 entries and print them in a simple JSON format before starting to run into memory issues. Not a bad result for a small microcontroller.

On **ESP32** around 2000 key/value pairs fit in the DRAM, and about 30000 in 4Mb of PSRAM. 

**NOTE:**  not all DRAM is available to allocation on ESP devices: 

*Due to a technical limitation, the maximum statically allocated DRAM usage is 160KB. The remaining 160KB (for a total of 320KB of DRAM) can only be allocated at runtime as heap.*  Reference is [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/mem_alloc.html). 



### Benchmarking:

Using random configuration-*like* key/value pairs (ex: `suggestion : ftp://toothbrush.health.org/elastic`, around. 40 characters long) generated from a set of 1000 random words (timings are in *microseconds*)

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



## Enjoy!
