#pragma once
#include "DictionaryDeclarations.h"

#define NODEARRAY_OK    0
#define NODEARRAY_ERR   (-1)
#define NODEARRAY_MEM   (-2)

#ifdef _DICT_PACK_STRUCTURES
class __attribute((__packed__)) node
#else
class node
#endif
{
  public:

    void* operator new(size_t size) {

        void* p = NULL;
#if defined (ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
      if (psramFound()) {
        p = ps_malloc(size);
      }
#endif
      if (!p) p = malloc(size);
#ifdef _LIBDEBUG_
      Serial.printf("NODE-NEW: size=%d (%d) k/v sizes=%d, %d, ptr=%u\n", size, sizeof(node), sizeof(_DICT_KEY_TYPE), sizeof(_DICT_VAL_TYPE), (uint32_t)p);
#endif    
    return p;
    }

    void operator delete(void* p) {
      if ( p == NULL ) return;
      node* n = (node*)p;

      // Delete key/value strings
      if ( n->keybuf ) { 
        free(n->keybuf);
        n->keybuf = NULL;
      }
      if ( n->valbuf ) {
          free(n->valbuf);
          n->valbuf = NULL;
      }
      free(p);
#ifdef _LIBDEBUG_
      Serial.printf("NODE-DELETE: Freed memory block %u\n", (uint32_t)p);
#endif    
    }

    uintNN_t    key() {
        uintNN_t k = 0;
        
        memcpy((void*)&k, keybuf, ksize < sizeof(uintNN_t) ? ksize : sizeof(uintNN_t));
        return k;
    }
    
    int8_t      create(const char* aKey, _DICT_KEY_TYPE aKeySize, const char* aVal, _DICT_VAL_TYPE aValSize, node* aLeft, node* aRight);
    int8_t      updateValue(const char* aVal, _DICT_VAL_TYPE aValSize);
    int8_t      updateKey(const char* aKey, _DICT_KEY_TYPE aKeySize);

#ifdef _LIBDEBUG_
    void printNode();
#endif
    char*           keybuf;
    _DICT_KEY_TYPE  ksize;
    char*           valbuf;
    _DICT_VAL_TYPE  vsize;
    node*           left;
    node*           right;
};


#ifdef _DICT_PACK_STRUCTURES
class __attribute((__packed__)) NodeArray
#else
class NodeArray
#endif   
{
  public:
    // init the queue (constructor).
    NodeArray(size_t init_size = 10);

    // clear the queue (destructor).
    ~NodeArray();

    // add an item to the queue.
    int8_t append(const node* i);

    // remove an item from the queue.
    void remove(const node* i);

    // check if the queue is empty.
    bool isEmpty() const;

    //    // get the number of items in the queue.
    size_t count() const;

    // check if the queue is full.
    bool isFull() const;


    node* operator [] (const size_t i) {
      if (i >= items) {
        //        exit ("QUEUE: Out of bounds");
        return NULL;
      }
      return contents[i];
    }

#ifdef _LIBDEBUG_
    void printArray();
#endif

  private:
    // resize the size of the queue.
    int8_t resize(const size_t s);

    // exit report method in case of error.
    //    void exit (const char * m) const;

    // the initial size of the queue.
    size_t initialSize;

    node** contents;    // the array of the queue.

    size_t size;        // the size of the queue.
    size_t items;       // the number of items of the queue.
    size_t tail;        // the tail of the queue.
};