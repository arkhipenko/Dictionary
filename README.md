# Dictionary

## Dictionary data type

[![arduino-library-badge](https://www.ardu-badge.com/badge/Dictionary.svg?)](https://www.ardu-badge.com/Dictionary)

### Background

I was looking for a small, but flexible class implementing **Dictionary** or **Hash** data type to use on ESP8266 or ESP32 microcontrollers.  In the end just decided to write my own. 

I needed this to work with JSON files and configuration parameters like

```
"ssid" = "your_wifi"

"ota_url" = "http://some.url"
```
This dictionary only works with `String` objects. 

Under the hood is a binary-tree structure based on the CRC32 (CRC16 or CRC64 if you want) hash of the key strings to make lookups fast.  Key collisions are taken care of, so **plumless** and **backeroo** will properly create separate entries... :)

There is no reason to use CRC64 since key collisions are resolved explicitly. The storage and performance overhead is not worth it. CRC16 produces too much of the collision probability, but maybe more suitable for devices with less memory.

To switch to alternative CRC calculation engine use the following statements:

```c++
#define _DICT_CRC_ 16
#define _DICT_CRC_ 32   --> this is the default
#define _DICT_CRC_ 64
```

 

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

`d("buckeroo", "buckeroonew");`

will replace the old value of **buckeroo** for the same key with a new value of **buckeroonew**



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

`d.size()` returns combined lengths (in bytes) of all key and value strings (including trailing zeros). If you need to exclude space required for trailing zeros subtract `2*count()` from the number returned by `size()`. 



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
- **CRC32 code** (modified) - by Björn Samuelsson ([here](http://home.thep.lu.se/~bjorn/crc/))
- **CRC64 code** (modified) - from Linus Torvalds Linux kernel source tree ([here](https://github.com/torvalds/linux/blob/master/lib/crc64.c))
- **Probability of CRC collisions** discussion ([here](https://stackoverflow.com/questions/14210298/probability-of-collision-when-using-a-32-bit-hash))
- **Examples of CRC32 collisions** article ([here](https://preshing.com/20110504/hash-collision-probabilities/))
- **Deletion of b-tree entries** article ([here](https://www.geeksforgeeks.org/binary-search-tree-set-2-delete/))

That should be everyone. Apologies if I missed anyone - will update as soon as I remember. 



### Stress test:

I was able to create ~400 entries and print them in a simple JSON format before starting to run into memory issues. Not a bad result for a small microcontroller:

```json
399: free heap = 17104

{
	"key0" : "This is value number 0",
	"key1" : "This is value number 1",
	"key2" : "This is value number 2",
	"key3" : "This is value number 3",
	"key4" : "This is value number 4",
```



### Benchmarking:

##### ESP8266 (Wemos R1 running at 160 MHz)

400 random keys (4-15 characters long), 1000 lookups

- CRC32: ~32.5 microseconds/lookup
- CRC64: ~68.5 microseconds/lookup

##### ESP32 (ESP32 WRoom Dev Board  running at 240 MHz)

2000 random keys (4-25 characters long), 20000 lookups

- CRC32: ~9.1 microseconds/lookup
- CRC64: ~9.3 microseconds/lookup



## Enjoy!
