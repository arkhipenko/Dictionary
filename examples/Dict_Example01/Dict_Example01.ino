/*

  Dictionary Example 01
  Copyright (c) Anatoli Arkhipenko, 2020
  All Rights Reserved

  This example tests Dictionary functionality
  
  Compile and run on ESP32 boards only

*/

//#define _DICT_CRC_ 16
//#define _DICT_CRC_ 32
//#define _DICT_CRC_ 64

#define _DICT_PACK_STRUCTURES
//#define _DICT_COMPRESS_SHOCO
//#define _DICT_COMPRESS_SMAZ
//#define _DICT_USE_PSRAM  // for ESP32 only

//#define _DICT_KEYLEN 64
//#define _DICT_VALLEN 256

#define _DEBUG_
//#define _TEST_

#ifdef _DEBUG_
#define _PP(a) Serial.print(a);
#define _PL(a) Serial.println(a);
#else
#define _PP(a)
#define _PL(a)
#endif


#include <Dictionary.h>


// ======================================================================
void setup() {

#ifdef _DEBUG_
  Serial.begin(115200);
  delay(500);
  {
    _PL("Dictionary test"); _PL();
  }
#endif

  _PP("Free heap (before allocation): "); _PL(ESP.getFreeHeap());
  Dictionary &d = *(new Dictionary(6));
  _PP("Free heap (after init allocation): "); _PL(ESP.getFreeHeap());

  d("ssid", "devices");
  d("pwd", "********");
  d("url", "http://ota.home.net");
  d("port", "80");
  d("plumless", "plumless value");
  d("buckeroo", "buckeroo value");
  _PP("Free heap (created 6 entries): "); _PL(ESP.getFreeHeap());

  _PL(); _PL("Testing access:");
  _PP("d[\"port\"]="); _PL(d["port"]);
  _PP("d[\"plumless\"]="); _PL(d["plumless"]);
  _PP("d[\"buckeroo\"]="); _PL(d["buckeroo"]);
  _PP("d[\"mqtt_url\"]="); _PL(d["mqtt_url"]);
  _PL();

  _PL("{");
  for (int i = 0; i < d.count(); i++) {
    _PP("\t\""); _PP(d(i)); _PP("\" : \""); _PP(d[i]); _PL("\",");
  }
  _PL("}");

  _PP("Free heap (json 1): "); _PL(ESP.getFreeHeap());

  d("plumless", "plumless new");

  _PL("{");
  for (int i = 0; i < d.count(); i++) {
    _PP("\t\""); _PP(d(i)); _PP("\" : \""); _PP(d[i]); _PL("\",");
  }
  _PL("}");
  _PP("Free heap (json 2): "); _PL(ESP.getFreeHeap());

  d("buckeroo", "buckeroo new");

  _PL("{");
  for (int i = 0; i < d.count(); i++) {
    _PP("\t\""); _PP(d(i)); _PP("\" : \""); _PP(d[i]); _PL("\",");
  }
  _PL("}");
  _PP("Free heap (json 3): "); _PL(ESP.getFreeHeap());

  _PL("{");
  for (int i = 0; i < d.count(); i++) {
    _PP("\t\""); _PP(d(i)); _PP("\" : \""); _PP(d[i]); _PL("\",");
  }
  _PL("}");
  _PP("Free heap (json 3 again): "); _PL(ESP.getFreeHeap());


  _PP("Reading out of bounds = "); _PL(d[10]);

  Dictionary &a = *(new Dictionary(6)); 
  Dictionary &b = *(new Dictionary(6));

// Merging one dictionary with another:
  a("one", "already here");
  a("ssid", "empty");
  a.merge(d);
  _PL(a.json()); // print JSON representation of the result

// Copy dictionary d into dictionary a 
  a = d;
  _PL(a.json()); // print JSON representation of the result

// Load dictionary b from the JSON representation of dictionary d
  b.jload( d.json() );
  _PL(b.json());


  delete (&a);
  delete (&b);
  delete (&d);
  
  _PP("Free heap (after delete): "); _PL(ESP.getFreeHeap());


  _PL(); _PL("Stress test:");

  Dictionary &t = *(new Dictionary(6));
  for (int i = 0; i < 200; i++)  {
    String k = String("key") + String(i);
    String v = String("This is value number ") + String(i);
    t(k, v);
    _PP(i); _PP(": free heap = "); _PL(ESP.getFreeHeap());
  }
  _PL("{");
  for (int i = 0; i < t.count(); i++) {
    _PP("\t\""); _PP(t(i)); _PP("\" : \""); _PP(t[i]); _PL("\",");
  }
  _PL("}");
  
  delete (&t);
  _PP("Free heap (end of test): "); _PL(ESP.getFreeHeap());
}

void loop() {

}
