/*
 *  QueueArray.h
 *
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
#ifndef _DICTIONARYARRAY_H
#define _DICTIONARYARRAY_H

// include Arduino basic header.
#include <Arduino.h>

// the definition of the queue class.
template<typename T>
class DictionaryArray {
  public:
    // init the queue (constructor).
    DictionaryArray (unsigned int init_size = 10);

    // clear the queue (destructor).
    ~DictionaryArray ();

    // add an item to the queue.
    void append (const T i);
    
    // remove an item from the queue.
    void remove (const T i);

    // check if the queue is empty.
    bool isEmpty () const;

//    // get the number of items in the queue.
    int count () const;

    // check if the queue is full.
    bool isFull () const;

   
    T operator [] (const unsigned int i) {
      if ( i >= items ) {
//        exit ("QUEUE: Out of bounds");
        return NULL;
      }
      T item = contents[i];
      return item;
    }

  private:
    // resize the size of the queue.
    void resize (const int s);

    // exit report method in case of error.
//    void exit (const char * m) const;

    // led blinking method in case of error.
    // void blink () const;

    // the initial size of the queue.
    unsigned int initialSize;

    // the pin number of the on-board led.
    // static const int ledPin = 13;

//    Print * printer; // the printer of the queue.
    T * contents;    // the array of the queue.

    int size;        // the size of the queue.
    int items;       // the number of items of the queue.

//    int head;        // the head of the queue.
    int tail;        // the tail of the queue.
};

// init the queue (constructor).
template<typename T>
DictionaryArray<T>::DictionaryArray (unsigned int init_size) {
  size = 0;       // set the size of queue to zero.
  items = 0;      // set the number of items of queue to zero.

//  head = 0;       // set the head of the queue to zero.
  tail = 0;       // set the tail of the queue to zero.
  
  initialSize = init_size;

//  printer = NULL; // set the printer of queue to point nowhere.

  // allocate enough memory for the array.
  contents = (T *) malloc (sizeof (T) * initialSize);

  // if there is a memory allocation error.
  if (contents == NULL) return;
//    exit ("QUEUE: insufficient memory to initialize queue.");

  // set the initial size of the queue.
  size = initialSize;
}

// clear the queue (destructor).
template<typename T>
DictionaryArray<T>::~DictionaryArray () {
  free (contents); // deallocate the array of the queue.

  contents = NULL; // set queue's array pointer to nowhere.
//  printer = NULL;  // set the printer of queue to point nowhere.

  size = 0;        // set the size of queue to zero.
  items = 0;       // set the number of items of queue to zero.

//  head = 0;        // set the head of the queue to zero.
  tail = 0;        // set the tail of the queue to zero.
}

// resize the size of the queue.
template<typename T>
void DictionaryArray<T>::resize (const int s) {
  // defensive issue.
  if (s <= 0) return;
//    exit ("QUEUE: error due to undesirable size for queue size.");

  // allocate enough memory for the temporary array.
  T * temp = (T *) malloc (sizeof (T) * s);

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
//  head = 0; 
  tail = items;

  // set the new size of the queue.
  size = s;
}

// add an item to the queue.
template<typename T>
void DictionaryArray<T>::append (const T i) {
  // check if the queue is full.
  if (isFull ())
    resize (size + initialSize);

  // store the item to the array.
  contents[tail++] = i;
  
  // increase the items.
  items++;
}

// remove an item from the queue.
template<typename T>
void DictionaryArray<T>::remove (T i) {
  // check if the queue is empty.
  if (isEmpty ()) return;
//    exit ("QUEUE: can't pop item from queue: queue is empty.");

  int index = -1;
  
  for (int j=0; j<items; j++) {
    if (i == contents[j]) {
      index = j;
      break;
    }
  }
  
  if (index < 0) return;  // how?
  
  for (int j=index; j<items-1; j++) {
      contents[j] = contents[j+1];
  }
  tail--;
  items--;
}


/*
// get the front of the queue.
template<typename T>
T DictionaryArray<T>::front () const {
  // check if the queue is empty.
  if (isEmpty ())
    exit ("QUEUE: can't get the front item of queue: queue is empty.");
    
  // get the item from the array.
  return contents[head];
}
*/

// check if the queue is empty.
template<typename T>
bool DictionaryArray<T>::isEmpty () const {
  return items == 0;
}

// check if the queue is full.
template<typename T>
bool DictionaryArray<T>::isFull () const {
  return items == size;
}

// get the number of items in the queue.
template<typename T>
int DictionaryArray<T>::count () const {
  return items;
}

#endif // _DICTIONARYARRAY_H
