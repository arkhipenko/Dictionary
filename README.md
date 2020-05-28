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

`d.jload(s)` will populate dictionary d from the JSON string s



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





### Memory management:

Dictionary allocates all its objects on the Heap. For ESP32 microcontrollers specifically there is an option to use PSRAM if present) as a storage option:

```c++
#define _DICT_USE_PSRAM
```

will make Dictionary try to allocate all its objects in the PSRAM.

All methods that allocate memory are enabled to return error codes in case memory allocation fails.  Typically a success code is '0', so a simple comparison like this would be sufficient:

```c++
if ( d("key", "value") ) {
	// this is an error situation on insert
}
// memory allocation was successful
```



### Error Codes:

```C++
#define DICTIONARY_OK    	0	   // operation successful
#define DICTIONARY_ERR   	(-1)   // genaral error
#define DICTIONARY_MEM   	(-2)   // failed memory allocation
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

Using random key/value pairs (ex: `rose-suggestion : toothbrush health elastic expansion`, around. 50 characters long) generated from a set of 1000 random words

##### ESP8266 (Wemos R1 running at 160 MHz)

300 random key/value pairs, 10000 lookups, times are averaged

- CRC16: ~72 microseconds/lookup, ~61 microseconds/delete
- CRC32: ~59 microseconds/lookup, ~54 microseconds/delete
- CRC64: ~55 microseconds/lookup, ~90 microseconds/delete

##### ESP32 (ESP32 WRoom Dev Board  running at 240 MHz)

###### Without using PSRAM:

2000 random key/value pairs, 10000 lookups, times are averaged

- CRC16: ~61 microseconds/lookup, ~83 microseconds/delete
- CRC32: ~43 microseconds/lookup, ~81 microseconds/delete
- CRC64: ~64 microseconds/lookup, ~59 microseconds/delete

###### Using 4Mb PSRAM:

2000 random key/value pairs, 10000 lookups, times are averaged

- CRC16: ~82 microseconds/lookup, ~107 microseconds/delete
- CRC32: ~60 microseconds/lookup, ~103 microseconds/delete
- CRC64: ~60 microseconds/lookup, ~112 microseconds/delete

###### Using 4Mb PSRAM:

20000 random key/value pairs, 20000 lookups, times are averaged

- CRC32: ~83 microseconds/lookup, ~2513 microseconds/delete

30000 random key/value pairs, 30000 lookups, times are averaged

- CRC32: ~77 microseconds/lookup

Further increasing the number of keys: ran out of PSRAM memory at key # 30682.



## Enjoy!