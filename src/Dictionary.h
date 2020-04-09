/*
  Implementation of the Dictionary data type
  for String key-value pairs, based on 
  CRC32/64 has keys and binary tree search

  ---
  
  Copyright (C) Anatoli Arkhipenko, 2020
  All rights reserved.
  
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

  ---

  2020-04-09:
    v1.0.0 - Initial release


*/


#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

#include "QueueArray.h"


//#define _DICT_CRC64_


#ifdef _DICT_CRC64_
#define uintNN_t uint64_t
#else
#define uintNN_t uint32_t
#endif

typedef struct node
{
  uintNN_t key;
  String keystr;
  String valstr;
  node *left;
  node *right;
};

class Dictionary {
  public:
    Dictionary(size_t init_size = 10);
    ~Dictionary();

    void insert(String keystr, String valstr);
    String search(String keystr);
    void destroy_tree();
    String key(unsigned int i) {
      node* p = (*Q)[i];
      if (p) return p->keystr;
      else return String();
    }
    String operator [] (String keystr) {
      return search(keystr);
    }
    String operator [] (unsigned int i) {
      return key(i);
    }

    void operator () (String keystr, String valstr) {
      insert (keystr, valstr);
    }

    inline const size_t count() {
      return Q->count();
    }

  private:
    void destroy_tree(node *leaf);
    void insert(uintNN_t key, String* keystr, String* valstr, node *leaf);
    node *search(uintNN_t key, node *leaf, String* keystr);
    uintNN_t crc(const void *data, size_t n_bytes);

    node                *root;
    uintNN_t            table[0x100];
    QueueArray<node *>* Q;
};


Dictionary::Dictionary(size_t init_size) {
  root = NULL;

#ifdef _DICT_CRC64_
  const uintNN_t m_poly = 0xC96C5795D7870F42ull;
  for (int i = 0; i < 256; ++i) {
    uintNN_t crc = i;
    for (int j = 0; j < 8; ++j) {
      if (crc & 1) { // is current coefficient set?
        crc >>= 1;   // yes, then assume it gets zero'd (by implied x^64 coefficient of dividend)
        crc ^= m_poly; // and add rest of the divisor
      }
      else { // no? then move to next coefficient
        crc >>= 1;
      }
    }
    table[i] = crc;
  }
#else
  for (size_t i = 0; i < 0x100; ++i) {
    uintNN_t r = i;
    for (int j = 0; j < 8; ++j) {
      r = (r & 1 ? 0 : (uintNN_t)0xEDB88320L) ^ r >> 1;
    }
    table[i] = ( r ^ (uintNN_t)0xFF000000L );
  }
#endif

  Q = new QueueArray<node *>(init_size);
}


Dictionary::~Dictionary() {
  destroy_tree();
  delete Q;
}

void Dictionary::destroy_tree(node *leaf) {
  if (leaf != NULL) {
    destroy_tree(leaf->left);
    destroy_tree(leaf->right);
    delete leaf;
  }
}

void Dictionary::insert(uintNN_t key, String *keystr, String *valstr, node *leaf) {
  if (key < leaf->key) {
    if (leaf->left != NULL)
      insert(key, keystr, valstr, leaf->left);
    else {
      leaf->left = new node;
      leaf->left->key = key;
      leaf->left->keystr = *keystr;
      leaf->left->valstr = *valstr;
      leaf->left->left = NULL;  //Sets the left child of the child node to null
      leaf->left->right = NULL; //Sets the right child of the child node to null
      Q->enqueue(leaf->left);
    }
  }
  else if (key > leaf->key) {
    if (leaf->right != NULL)
      insert(key, keystr, valstr, leaf->right);
    else {
      leaf->right = new node;
      leaf->right->key = key;
      leaf->right->keystr = *keystr;
      leaf->right->valstr = *valstr;
      leaf->right->left = NULL; //Sets the left child of the child node to null
      leaf->right->right = NULL; //Sets the right child of the child node to null
      Q->enqueue(leaf->right);
    }
  }
  else if (key == leaf->key) {
    if ( leaf->keystr == *keystr ) {
      leaf->valstr = *valstr;
    }
    else {
      if (leaf->right != NULL)
        insert(key, keystr, valstr, leaf->right);
      else {
        leaf->right = new node;
        leaf->right->key = key;
        leaf->right->keystr = *keystr;
        leaf->right->valstr = *valstr;
        leaf->right->left = NULL; //Sets the left child of the child node to null
        leaf->right->right = NULL; //Sets the right child of the child node to null
        Q->enqueue(leaf->right);
      }
    }
  }
}

node *Dictionary::search(uintNN_t key, node *leaf, String* keystr) {
  if (leaf != NULL) {
    if (key == leaf->key && leaf->keystr == *keystr)
      return leaf;
    if (key < leaf->key)
      return search(key, leaf->left, keystr);
    else
      return search(key, leaf->right, keystr);
  }
  else return NULL;
}


void Dictionary::insert(String keystr, String valstr) {
  uintNN_t key = crc( keystr.c_str(), keystr.length() );

  if (root != NULL)
    insert(key, &keystr, &valstr, root);
  else {
    root = new node;
    root->key = key;
    root->keystr = keystr;
    root->valstr = valstr;
    root->left = NULL;
    root->right = NULL;
    Q->enqueue(root);
  }
}


String Dictionary::search(String keystr) {
  uintNN_t key = crc(keystr.c_str(), keystr.length());

  node *p = search(key, root, &keystr);
  if (p) return p->valstr;
  return String("");
}


void Dictionary::destroy_tree() {
  destroy_tree(root);
}

#ifdef _DICT_CRC64_
uintNN_t Dictionary::crc(const void *p, size_t len) {
  size_t i, t;
  uintNN_t crc = 0;
  uint8_t *_p = (uint8_t*)p;

  for (i = 0; i < len; i++) {
    t = ((crc >> 56) ^ (*_p++)) & 0xFF;
    crc = table[t] ^ (crc << 8);
  }
  return crc;
}
#else
uintNN_t Dictionary::crc(const void *data, size_t n_bytes) {
  uintNN_t crc = 0;
  for (size_t i = 0; i < n_bytes; ++i) {
    crc = table[(uint8_t)crc ^ ((uint8_t*)data)[i]] ^ crc >> 8;
  }
  return crc;
}
#endif

#endif // #define _DICTIONARY_H_

