/*
 *  NodeArray.h
 * 
 *  Adapted from QueueArray: 
 *  Library implementing a generic, dynamic queue (array version).
 *
 *  ---
 *
 *  Copyright (C) 2010  Efstathios Chatzikyriakidis (contact@efxa.org)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

// header defining the interface of the source.
#ifndef _NODEARRAY_H
#define _NODEARRAY_H

#include <Arduino.h>

#ifdef _DICT_CRC64_
#define uintNN_t uint64_t
#else
#define uintNN_t uint32_t
#endif

struct node
{
  uintNN_t key;
  String keystr;
  String valstr;
  node *left;
  node *right;
};


class NodeArray {
  public:
    // init the queue (constructor).
    NodeArray (unsigned int init_size = 10);

    // clear the queue (destructor).
    ~NodeArray ();

    // add an item to the queue.
    void append (const node* i);
    
    // remove an item from the queue.
    void remove (const node* i);

    // check if the queue is empty.
    bool isEmpty () const;

//    // get the number of items in the queue.
    int count () const;

    // check if the queue is full.
    bool isFull () const;

   
    node* operator [] (const unsigned int i) {
      if ( i >= items ) {
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
    void resize (const int s);

    // exit report method in case of error.
//    void exit (const char * m) const;

    // the initial size of the queue.
    unsigned int initialSize;

    node**        contents;    // the array of the queue.

    int size;        // the size of the queue.
    int items;       // the number of items of the queue.
    int tail;        // the tail of the queue.
};

// init the queue (constructor).
NodeArray::NodeArray (unsigned int init_size) {
  size = 0;       // set the size of queue to zero.
  items = 0;      // set the number of items of queue to zero.
  tail = 0;       // set the tail of the queue to zero.
  
  initialSize = init_size;

  // allocate enough memory for the array.
  contents = (node**) malloc (sizeof (node*) * initialSize);

  // if there is a memory allocation error.
  if (contents == NULL) return;
//    exit ("QUEUE: insufficient memory to initialize queue.");

  // set the initial size of the queue.
  size = initialSize;
}

// clear the queue (destructor).
NodeArray::~NodeArray () {
  free (contents); // deallocate the array of the queue.

  contents = NULL; // set queue's array pointer to nowhere.

  size = 0;        // set the size of queue to zero.
  items = 0;       // set the number of items of queue to zero.
  tail = 0;        // set the tail of the queue to zero.
}

// resize the size of the queue.
void NodeArray::resize (const int s) {
  // defensive issue.
  if (s <= 0) return;
//    exit ("QUEUE: error due to undesirable size for queue size.");

  // allocate enough memory for the temporary array.
  node** temp = (node**) malloc (sizeof (node*) * s);

  // if there is a memory allocation error.
  if (temp == NULL) return;
//    exit ("QUEUE: insufficient memory to initialize temporary queue.");
  
  // copy the items from the old queue to the new one.
  for (int i = 0; i < items; i++)
    temp[i] = contents[i];

  // deallocate the old array of the queue.
  free (contents);

  // copy the pointer of the new queue.
  contents = temp;

  // set the head and tail of the new queue.
  tail = items;

  // set the new size of the queue.
  size = s;
}

// add an item to the queue.
void NodeArray::append (const node* i) {
  // check if the queue is full.
  if (isFull ())
    resize (size + initialSize);

  // store the item to the array.
  contents[tail++] = (node*)i;
  
  // increase the items.
  items++;
}

// remove an item from the queue.
void NodeArray::remove (const node* i) {
  // check if the queue is empty.

#ifdef _LIBDEBUG_  
    Serial.printf("NodeArray: request remove: %u\n", (uint32_t) i);
#endif

  if (isEmpty ()) return;
//    exit ("QUEUE: can't pop item from queue: queue is empty.");

  if (items > 1) {
    int index = -1;
    
    for (int j=0; j<items; j++) {
      if (i == contents[j]) {
        index = j;
        break;
      }
    }
    
    if (index < 0) return;  // how?
    
#ifdef _LIBDEBUG_  
    Serial.printf("NodeArray: found index: %d\n", index);
#endif
    
    for (int j=index; j<items-1; j++) {
        contents[j] = contents[j+1];
    }
  }
  tail--;
  items--;
}


#ifdef _LIBDEBUG_
void NodeArray::printArray () {
  Serial.printf("\nNodeArray::printArray:\n");
  for (int i=0; i<items; i++) {
    Serial.printf("%d: %u\n", i, (uint32_t) contents[i]);
  }
  Serial.println();
}
#endif

// check if the queue is empty.
bool NodeArray::isEmpty () const {
  return items == 0;
}

// check if the queue is full.
bool NodeArray::isFull () const {
  return items == size;
}

// get the number of items in the queue.
int NodeArray::count () const {
  return items;
}

#endif // _NODEARRAY_H
