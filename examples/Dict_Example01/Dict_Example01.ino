/*
  Dictionary Example 01
  Copyright (c) Anatoli Arkhipenko, 2020
  All Rights Reserved

  General functionality of the 4.0 API: typed values, lookups, positional
  access, updates, JSON output and loading, merge, copy, move and removal.

  Compile and run on ESP32 boards (uses ESP.getFreeHeap()).
*/

//#define _DICT_COMPRESS         // built-in value compression
//#define _DICT_USE_PSRAM        // ESP32 only: store data in PSRAM when present
//#define _DICT_TYPED_JSON       // jload() stores numbers and true/false typed
//#define _DICT_KEYLEN 64
//#define _DICT_VALLEN 254

#define _DEBUG_

#ifdef _DEBUG_
#define _PP(a) Serial.print(a);
#define _PL(a) Serial.println(a);
#else
#define _PP(a)
#define _PL(a)
#endif

#include <Dictionary.h>

void printAll(const Dictionary& d) {
  _PL("{");
  for (size_t i = 0; i < d.count(); i++) {
    _PP("\t\""); _PP(d.keyAt(i)); _PP("\" : \""); _PP(d.value(i)); _PL("\",");
  }
  _PL("}");
}

// Returning a Dictionary by value moves it (copy construction is not available).
Dictionary makeDefaults() {
  Dictionary d;
  d.set("mqtt_port", 1883);
  d.set("retries", 3);
  return d;
}

// ======================================================================
void setup() {
#ifdef _DEBUG_
  Serial.begin(115200);
  delay(500);
  _PL("Dictionary test"); _PL();
#endif
  _PP("Free heap (before allocation): "); _PL(ESP.getFreeHeap());

  Dictionary& d = *(new Dictionary(6));
  _PP("Free heap (after init allocation): "); _PL(ESP.getFreeHeap());

  // Typed values: text, numbers, booleans
  d.set("ssid", "devices");
  d.set("pwd", "********");
  d.set("url", "http://ota.home.net");
  d.set("port", 80);
  d.set("ratio", 0.75f);
  d.set("debug", true);
  _PP("Free heap (created 6 entries): "); _PL(ESP.getFreeHeap());

  _PL(); _PL("Testing access:");
  _PP("d[\"url\"]="); _PL(d["url"]);
  _PP("getInt(\"port\")="); _PL(d.getInt("port"));
  _PP("getFloat(\"ratio\")="); _PL(d.getFloat("ratio"));
  _PP("getBool(\"debug\")="); _PL(d.getBool("debug"));
  _PP("getInt(\"mqtt_port\", 1883)="); _PL(d.getInt("mqtt_port", 1883));   // missing: default
  _PP("has(\"ssid\")="); _PL(d.has("ssid"));
  _PP("type(\"port\") is DICT_INT: "); _PL(d.type("port") == DICT_INT);
  _PL();
  printAll(d);

  // Update in place: the position does not change
  d.set("url", "https://ota.home.net/firmware.bin");
  d.set("port", 8080);
  printAll(d);

  _PP("Reading out of bounds = "); _PL(d.value(10));
  _PP("JSON: "); _PL(d.json());
  _PP("jsize: "); _PL(d.jsize());

  // Merge, copy and load
  Dictionary& a = *(new Dictionary(6));
  Dictionary& b = *(new Dictionary(6));
  a.set("one", "already here");
  a.set("ssid", "empty");
  a.merge(d);                      // d's values win for shared keys
  _PL(a.json());
  a = d;                           // replace a with a copy of d
  _PL(a.json());
  b.jload(d.json());               // load b from d's JSON
  _PL(b.json());
  _PP("a == d: "); _PL(a == d);

  Dictionary defaults = makeDefaults();
  _PL(defaults.json());

  // Removal keeps the order of the remaining pairs
  d.remove("pwd");
  printAll(d);

  delete (&a);
  delete (&b);
  delete (&d);
  _PP("Free heap (after delete): "); _PL(ESP.getFreeHeap());

  _PL(); _PL("Stress test:");
  Dictionary& t = *(new Dictionary(6));
  t.reserve(200);                  // one allocation for the index up front
  for (int i = 0; i < 200; i++) {
    String k = String("key") + String(i);
    String v = String("This is value number ") + String(i);
    t.set(k, v);
    _PP(i); _PP(": free heap = "); _PL(ESP.getFreeHeap());
  }
  printAll(t);
  delete (&t);
  _PP("Free heap (end of test): "); _PL(ESP.getFreeHeap());
}

void loop() {
}
