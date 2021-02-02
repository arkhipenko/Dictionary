/*
    NodeArray.h

    Adapted from QueueArray:
    Library implementing a generic, dynamic queue (array version).

    ---

    Copyright (C) 2010  Efstathios Chatzikyriakidis (contact@efxa.org)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.

*/

// header defining the interface of the source.
#ifndef _NODEARRAY_H
#define _NODEARRAY_H

namespace NodeArray {

#define NODEARRAY_OK    0
#define NODEARRAY_ERR   (-1)
#define NODEARRAY_MEM   (-2)

#ifdef _DICT_PACK_STRUCTURES
class __attribute((__packed__)) node {
#else
class node {
#endif
  public:

    void* operator new(size_t size) {

      void* p = NULL;
      if ( size ) {
#if defined (ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
        if (psramFound()) {
          p = ps_malloc(size);
        }
#endif
        if (!p) p = malloc(size);
#ifdef _LIBDEBUG_
        Serial.printf("NODE-NEW: size=%d (%d) k/v sizes=%d, %d, ptr=%u\n", size, sizeof(node), sizeof(_DICT_KEY_TYPE), sizeof(_DICT_VAL_TYPE), (uint32_t)p);
#endif    
      }
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


int8_t node::create(const char* aKey, _DICT_KEY_TYPE aKeySize, const char* aVal, _DICT_VAL_TYPE aValSize, node* aLeft, node* aRight) {

  if ( aKeySize == 0 ) return NODEARRAY_ERR; // a key cannot be zero-length
  vsize = aValSize;

  uintNN_t ks = ( aKeySize < sizeof(uintNN_t) ? sizeof(uintNN_t) : aKeySize );
  ksize = aKeySize;

  // Now we will try to allocate memory to both char arrays
  keybuf = NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    keybuf = (char*)ps_malloc(ks + _DICT_EXTRA);
  }
#endif
  if (!keybuf)
    keybuf = (char*)malloc(ks + _DICT_EXTRA);

  if (!keybuf) return NODEARRAY_MEM;

  valbuf = NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    valbuf = (char*)ps_malloc(vsize + _DICT_EXTRA);
  }
#endif
  if (!valbuf)
    valbuf = (char*)malloc(vsize + _DICT_EXTRA);

  if (!valbuf) {
    free(keybuf);
    return NODEARRAY_MEM;
  }

  // Success - we have space for both strings
  memset(keybuf, 0, ks);
  memcpy(keybuf, aKey, aKeySize);
  memcpy(valbuf, aVal, aValSize);

  left = aLeft;
  right = aRight;

#ifdef _LIBDEBUG_
  Serial.print("NODE-CREATE: created a node:\n");
  printNode();
#endif

  return NODEARRAY_OK;
}


int8_t node::updateValue(const char* aVal, _DICT_VAL_TYPE aValSize) {
  if ( aValSize > _DICT_VALLEN ) return NODEARRAY_ERR;
  
  if (aValSize <= vsize) { // new string fits into the old one - will just update
    memcpy(valbuf, aVal, aValSize);
    vsize = aValSize;

#ifdef _LIBDEBUG_
    Serial.printf("NODE-UPDATEVALUE: updated value for key = %d\n", (uint32_t)keybuf);
    printNode();
#endif

    return NODEARRAY_OK;
  }

  char* temp = NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    temp = (char*)ps_malloc(aValSize + _DICT_EXTRA);
  }
#endif
  if (!temp)
    temp = (char*)malloc(aValSize + _DICT_EXTRA);

  if (!temp) { // no memory
    return NODEARRAY_MEM;
  }

  // ok - we have enough space for the new value, lets copy the string there and delete the old one.

  if ( valbuf ) free(valbuf);
  valbuf = temp;

  vsize = aValSize;
  memcpy(valbuf, aVal, vsize);
  
#ifdef _LIBDEBUG_
  Serial.printf("NODE-UPDATEVALUE: replaced value for key = %d\n", (uint32_t)key());
  printNode();
#endif

  return NODEARRAY_OK;
}


int8_t node::updateKey(const char* aKey, _DICT_KEY_TYPE aKeySize) {
  if (aKeySize > _DICT_KEYLEN) return NODEARRAY_ERR;;

  _DICT_KEY_TYPE ks = aKeySize < sizeof(uintNN_t) ? sizeof(uintNN_t) : aKeySize;

  if (ks < ksize) { // new string fits into the old one - will just update
    memcpy(keybuf, aKey, aKeySize);
    ksize = aKeySize;
    
#ifdef _LIBDEBUG_
    Serial.printf("NODE-UPDATEKEY: updated key = %d\n", (uint32_t)keybuf);
    printNode();
#endif

    return NODEARRAY_OK;
  }
  
  char* temp = NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    temp = (char*)ps_malloc(ks + _DICT_EXTRA);
  }
#endif
  if (!temp)
    temp = (char*)malloc(ks + _DICT_EXTRA);

  if (!temp) { // no memory, will copy as much as we can, and return an error

#ifdef _LIBDEBUG_
    Serial.printf("NODE-UPDATEKEY: NOMEMORY replace keybuf = %d\n", (uint32_t)keybuf);
    printNode();
#endif

    return NODEARRAY_MEM;
  }

  // ok - we have enough space for the new value, lets copy the string there and delete the old one.

  if ( keybuf ) free(keybuf);
  keybuf = temp;

  ksize = aKeySize;
  memcpy(keybuf, aKey, ksize);
  
#ifdef _LIBDEBUG_
  Serial.printf("NODE-UPDATEKEY: replaced keybuf = %d\n", (uint32_t)keybuf);
  printNode();
#endif

  return NODEARRAY_OK;
}


#ifdef _LIBDEBUG_
void node::printNode() {
  Serial.println("node:");
  Serial.printf("\tkeyNN   = %u\n", key());
  Serial.printf("\tkey  = ");
  for (int i=0; i<ksize; i++) Serial.printf("%02x", keybuf[i]); 
  Serial.printf(" (%d) (%u)\n", ksize, (uint32_t)keybuf);
  Serial.printf("\tval  = ");
  for (int i=0; i<vsize; i++) Serial.printf("%02x", valbuf[i]); 
  Serial.printf(" (%d) (%u)\n", vsize, (uint32_t)valbuf);
  Serial.printf("\tLeft n  = %u\n", (uint32_t)left);
  Serial.printf("\tRight n = %u\n", (uint32_t)right);
}
#endif

#ifdef _DICT_PACK_STRUCTURES
class __attribute((__packed__)) NodeArray {
#else
class NodeArray {
#endif   
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

// init the queue (constructor).
NodeArray::NodeArray(size_t init_size) {
  size = 0;       // set the size of queue to zero.
  items = 0;      // set the number of items of queue to zero.
  tail = 0;       // set the tail of the queue to zero.

  initialSize = init_size;
  contents = NULL;

  // Let's not allocate memory in the constructor and delegate it to the
  // resize method, that could return something.

}

// clear the queue (destructor).
NodeArray::~NodeArray() {
  free(contents); // deallocate the array of the queue.

  contents = NULL; // set queue's array pointer to nowhere.

  size = 0;        // set the size of queue to zero.
  items = 0;       // set the number of items of queue to zero.
  tail = 0;        // set the tail of the queue to zero.
}

// resize the size of the queue.
int8_t NodeArray::resize(const size_t s) {
  // defensive issue.
  if (s <= 0) return NODEARRAY_ERR;
  //    exit ("QUEUE: error due to undesirable size for queue size.");

  // allocate enough memory for the temporary array.
  node** temp = NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    temp = (node**)ps_malloc(sizeof(node*) * s);
  }
#endif
  if (!temp)
    temp = (node**)malloc(sizeof(node*) * s);

  // if there is a memory allocation error.
  if (temp == NULL) return NODEARRAY_MEM;
  //    exit ("QUEUE: insufficient memory to initialize temporary queue.");

  // copy the items from the old queue to the new one.
  for (size_t i = 0; i < items; i++)
    temp[i] = contents[i];

  // deallocate the old array of the queue.
  free(contents);

  // copy the pointer of the new queue.
  contents = temp;

  // set the head and tail of the new queue.
  tail = items;

  // set the new size of the queue.
  size = s;
  return NODEARRAY_OK;
}

// add an item to the queue.
int8_t NodeArray::append(const node* i) {
  // check if the queue is full.
  if (isFull()) {
    int8_t rc = resize(size + initialSize);
    if (rc) return rc;
  }

  // store the item to the array.
  contents[tail++] = (node*)i;

  // increase the items.
  items++;

#ifdef _LIBDEBUG_
  Serial.printf("NODEARRAY-APPEND: successfully added a node %x. Cur size: %d\n", (uint32_t) i, items);
#endif
  return NODEARRAY_OK;
}


// remove an item from the queue.
void NodeArray::remove(const node* i) {
  // check if the queue is empty.

#ifdef _LIBDEBUG_
  Serial.printf("NODEARRAY-REMOVE: request remove: %u\n", (uint32_t)i);
#endif

  if (isEmpty()) return;
  //    exit ("QUEUE: can't pop item from queue: queue is empty.");

  if (items > 1) {
    int8_t found = 0;
    size_t index = 0;

    for (size_t j = 0; j < items; j++) {
      if (i == contents[j]) {
        index = j;
        found = 1;
        break;
      }
    }

    if ( !found ) return;  // how?

#ifdef _LIBDEBUG_
    Serial.printf("NODEARRAY-REMOVE: found index: %d\n", index);
#endif

    for (size_t j = index; j < items - 1; j++) {
      contents[j] = contents[j + 1];
    }
  }
  tail--;
  items--;
#ifdef _LIBDEBUG_
//    for (size_t j = 0; j < items; j++) {
//        Serial.printf("%d : %u\n", j, (uint32_t)contents[j]);
//    }
    Serial.printf("NODEARRAY-REMOVE: removal complete\n");
    Serial.printf("NODEARRAY-REMOVE: current count: %d\n", items);
#endif
}


#ifdef _LIBDEBUG_
void NodeArray::printArray() {
  Serial.printf("\nNodeArray::printArray:\n");
  for (size_t i = 0; i < items; i++) {
    Serial.printf("%d: %u\n", i, (uint32_t)contents[i]);
  }
  Serial.println();
}
#endif

// check if the queue is empty.
bool NodeArray::isEmpty() const {
  return items == 0;
}

// check if the queue is full.
bool NodeArray::isFull() const {
  return items == size;
}

// get the number of items in the queue.
size_t NodeArray::count() const {
  return items;
}
} // namespace NodeArray
#endif // _NODEARRAY_H

