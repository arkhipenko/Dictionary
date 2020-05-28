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

#define NODEARRAY_OK    0
#define NODEARRAY_ERR   (-1)
#define NODEARRAY_MEM   (-2)

class node {
  public:

    void* operator new(size_t size) {

#if defined (ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
      if (psramFound()) {
        void* p = ps_malloc(size);
        return p;
      }
#endif
      void* p = malloc(size);
      return p;
    }

    void operator delete(void* p) {
      if ( p == NULL ) return;
      node* n = (node*)p;

      // Delete key/value strings
      if ( n->keystr ) { 
        free(n->keystr);
        n->keystr = NULL;
      }
      if ( n->valstr ) {
          free(n->valstr);
          n->valstr = NULL;
      }
      free(p);
#ifdef _LIBDEBUG_
      Serial.printf("NODE-DELETE: Freed memory block %u\n", (uint32_t)p);
#endif    
    }

    uintNN_t    key() {
        uintNN_t* k = (uintNN_t*) keystr;
        return *k;
    }
    
    int8_t      create(const char* aKeystr, const char* aValstr, node* aLeft, node* aRight);
    int8_t      updateValue(const char* aValstr);
    int8_t      updateKey(const char* aKeystr);

#ifdef _LIBDEBUG_
    void printNode();
#endif
    char*           keystr;
    _DICT_KEY_TYPE  ksize;
    char*           valstr;
    _DICT_VAL_TYPE  vsize;
    node*           left;
    node*           right;
};


int8_t node::create(const char* aKeystr, const char* aValstr, node* aLeft, node* aRight) {

  if ( (ksize = strnlen(aKeystr, _DICT_KEYLEN)) == 0 ) return NODEARRAY_ERR; // a key cannot be zero-length
  vsize = strnlen(aValstr, _DICT_VALLEN);

  ksize = ( ksize < _DICT_LEN ? _DICT_LEN : ksize );

  // Now we will ry to allocate memory to both char arrays
  keystr = NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    keystr = (char*)ps_malloc(ksize+1);
  }
#endif
  if (!keystr)
    keystr = (char*)malloc(ksize+1);

  if (!keystr) return NODEARRAY_MEM;

  valstr = NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    valstr = (char*)ps_malloc(vsize+1);
  }
#endif
  if (!valstr)
    valstr = (char*)malloc(vsize+1);

  if (!valstr) {
    free(keystr);
    return NODEARRAY_MEM;
  }

  // Success - we have space for both strings
  memset(keystr, 0, ksize);
  strcpy(keystr, aKeystr);
  strcpy(valstr, aValstr);

  left = aLeft;
  right = aRight;

#ifdef _LIBDEBUG_
  Serial.print("NODE-CREATE: created a node:\n");
  printNode();
#endif

  return NODEARRAY_OK;
}


int8_t node::updateValue(const char* aValstr) {
  size_t l = strnlen(aValstr, _DICT_VALLEN);

  if (l < vsize) { // new string fits into the old one - will just update
    strcpy(valstr, aValstr);

#ifdef _LIBDEBUG_
    Serial.printf("NODE-UPDATEVALUE: updated value for key = %d\n", (uint32_t)keystr);
    printNode();
#endif

    return NODEARRAY_OK;
  }

  char* temp = NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    temp = (char*)ps_malloc(l + 1);
  }
#endif
  if (!temp)
    temp = (char*)malloc(l + 1);

  if (!temp) { // no memory, will copy as much as we can, and return an error
    strcpy(valstr, aValstr);
    return NODEARRAY_MEM;
  }

  // ok - we have enough space for the new value, lets copy the string there and delete the old one.

  if ( valstr ) free(valstr);
  valstr = temp;

  vsize = l;
  strcpy(valstr, aValstr);
  
#ifdef _LIBDEBUG_
  Serial.printf("NODE-UPDATEVALUE: replaced value for key = %d\n", (uint32_t)keystr);
  printNode();
#endif

  return NODEARRAY_OK;
}


int8_t node::updateKey(const char* aKeystr) {
  size_t l = strnlen(aKeystr, _DICT_KEYLEN);

  if (l < ksize) { // new string fits into the old one - will just update
    memset(keystr, 0, ksize);
    strcpy(keystr, aKeystr);
    
#ifdef _LIBDEBUG_
    Serial.printf("NODE-UPDATEKEY: updated key = %d\n", (uint32_t)keystr);
    printNode();
#endif

    return NODEARRAY_OK;
  }

  l = ( l < _DICT_LEN ? _DICT_LEN : l );
  
  char* temp = NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(_DICT_USE_PSRAM)
  if (psramFound()) {
    temp = (char*)ps_malloc(l + 1);
  }
#endif
  if (!temp)
    temp = (char*)malloc(l + 1);

  if (!temp) { // no memory, will copy as much as we can, and return an error
    strcpy(keystr, aKeystr);

#ifdef _LIBDEBUG_
    Serial.printf("NODE-UPDATEKEY: NOMEMORY replace key = %d\n", (uint32_t)keystr);
    printNode();
#endif

    return NODEARRAY_MEM;
  }

  // ok - we have enough space for the new value, lets copy the string there and delete the old one.

  if ( keystr ) free(keystr);
  keystr = temp;

  ksize = l;
  memset(keystr, 0, ksize);
  strcpy(keystr, aKeystr);
  
#ifdef _LIBDEBUG_
  Serial.printf("NODE-UPDATEKEY: replaced key = %d\n", (uint32_t)keystr);
  printNode();
#endif

  return NODEARRAY_OK;
}


#ifdef _LIBDEBUG_
void node::printNode() {
  Serial.println("node:");
  Serial.printf("\tkeyNN   = %u\n", key());
  Serial.printf("\tkeyStr  = %s (%d) (%u)\n", keystr, ksize, (uint32_t)keystr);
  Serial.printf("\tValStr  = %s (%d) (%u)\n", valstr, vsize, (uint32_t)valstr);
  Serial.printf("\tLeft n  = %u\n", (uint32_t)left);
  Serial.printf("\tRight n = %u\n", (uint32_t)right);
}
#endif


class NodeArray {
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
    size_t index = -1;

    for (size_t j = 0; j < items; j++) {
      if (i == contents[j]) {
        index = j;
        break;
      }
    }

    if (index < 0) return;  // how?

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

#endif // _NODEARRAY_H

