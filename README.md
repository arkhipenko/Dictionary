# Dictionary

### Dictionary data type

#### Background

I was looking for a small, but flexible class implementing **Dictionary** or **Hash** data type to use on ESP8266 or ESP32 microcontrollers.  In the end just decided to write my own. 

I needed this to work with JSON files and configuration parameters like

`"ssid" = "your_wifi"``

``"ota_url" = "http://some.url"`

This dictionary only works with `String` objects. 

Under the hood is a binary-tree structure based on the CRC32 (or CRC64 if you want) hash of the key strings to make lookups fast.  Key collisions are taken care of, so **plumless** and **backeroo** will properly create separate entries... :)

There is no reason to use CRC64 since key collisions are resolved explicitly. The storage and performance overhead is not worth it. 

#### Usage:

##### Creation:

`Dictionary *dict = new Dictionary();` **or**

`Dictionary *dict = new Dictionary(N);`  - where N is the initial size of the dictionary (10 is a default), **or**

`Dictionary &d = *(new Dictionary(6));`



##### Populate key-value pairs:

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



##### Lookup values:

`d["port"]`  will return "80"

`d["mqtt_url"]` will return an empty String as the key does not exist yet.   **or**

`dict->search("port")`  will return "80"

`d[0]` will return "devices"

`d[3]` will return "80"

`dict->value(4)` will return "buckeroo"



##### Lookup keys:

`d(0)` will return "ssid"

`d(1)` will return "pwd"

`d(10)` will return an empty string as this is out of current bounds.   **or**

`dict->key(0)` will return "ssid";

`d.count()` returns a number of key-value pairs in the dictionary 

**NOTE**: Indexes are assigned in the order the kay-values are inserted. You **cannot** assign `d(0, "test")`



##### Deleting kay-value pairs

There is no deletion implemented as it would require lengthy updates to the underlying binary tree structure and in my opinion is not worth the performance and code overhead. *Sorry*. 



#### Examples:

##### Creating a simple JSON config file:

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



#### Credits and References: 

- **QueueArray** (modified) - by Efstathios Chatzikyriakidis ([here](https://playground.arduino.cc/Code/QueueArray/))
- **Binary Trees in C++** - by Alex Allain ([here](https://www.cprogramming.com/tutorial/lesson18.html))
- **CRC32 code** (modified) - by Björn Samuelsson ([here](http://home.thep.lu.se/~bjorn/crc/))
- **CRC64 code** (modified) - from Linus Torvalds Linux kernel source tree ([here](https://github.com/torvalds/linux/blob/master/lib/crc64.c))
- **Probability of CRC collisions** discussion ([here](https://stackoverflow.com/questions/14210298/probability-of-collision-when-using-a-32-bit-hash))

That should be everyone. Apologies if I missed anyone - will update as soon as I remember. 



#### Stress test:

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



#### Benchmarking:

###### ESP8266 (Wemos R1 running at 80 MHz)

400 random keys (4-15 characters long), 1000 lookups

- CRC32: ~32.5 microseconds/lookup
- CRC64: ~68.5 microseconds/lookup

###### ESP32 (ESP32 WRoom Dev Board  running at 240 MHz)

2000 random keys (4-25 characters long), 20000 lookups

- CRC32: ~9.1 microseconds/lookup
- CRC64: ~9.3 microseconds/lookup



## Enjoy!