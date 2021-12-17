#include "DictionaryDeclarations.h"

int8_t node::create(const char* aKey, _DICT_KEY_TYPE aKeySize, const char* aVal, _DICT_VAL_TYPE aValSize, node* aLeft, node* aRight) {

  size_t  vsize_final;
  
  if ( aKeySize == 0 ) return NODEARRAY_ERR; // a key cannot be zero-length
  vsize = aValSize;
  vsize_final = vsize + _DICT_EXTRA == 0 ? 1 : vsize + _DICT_EXTRA;
  

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
    valbuf = (char*)ps_malloc(vsize_final);
  }
#endif
  if (!valbuf)
    valbuf = (char*)malloc(vsize_final);

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




// ==== CONSTRUCTOR / DESTRUCTOR ==================================
Dictionary::Dictionary(size_t init_size) {
  iRoot = NULL;

  // This is unlikely to fail as practically no memory is allocated by the NodeArray
  // All memory allocation is delegated to the first append
  Q = new NodeArray(init_size);
  initSize = init_size;

#ifdef _DICT_COMPRESS
  // This however is a problem - need to think about a safer way
  iKeyTemp = (char*) malloc(sizeof(char) * (_DICT_KEYLEN + 1));
  iValTemp = (char*) malloc(sizeof(char) * (_DICT_VALLEN + 1));
#else
  iKeyTemp = NULL;
  iValTemp = NULL;
#endif
}

Dictionary::~Dictionary() {
  destroy();
  delete Q;
#ifdef _DICT_COMPRESS
  free(iKeyTemp); iKeyTemp = NULL;
  free(iValTemp); iValTemp = NULL;
#endif
}



// ==== PUBLIC METHODS ===============================================

// ===== INSERTS =====================================================

int8_t Dictionary::insert(const char* keystr, const char* valstr) {
  // TODO: decide if to check for length here
  iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);
#ifdef _DICT_COMPRESS
  int8_t rc;
#endif

  if ( iKeyLen == 0 || iKeyLen > _DICT_KEYLEN ) return DICTIONARY_ERR;
  if ( (iValLen = strnlen(valstr, _DICT_VALLEN + 1)) > _DICT_VALLEN ) return DICTIONARY_ERR;

#ifdef _DICT_COMPRESS
  if ( (rc = compressKey(keystr)) ) return rc;
  if ( (rc = compressValue(valstr)) ) return rc;
#else
  iKeyTemp = (char*) keystr;
  iValTemp = (char*) valstr;
#endif

  uintNN_t key = crc(iKeyTemp, iKeyLen);

  if (iRoot != NULL)
    return insert(key, iKeyTemp, iKeyLen, iValTemp, iValLen, iRoot);
  else {
    int8_t rc;

    iRoot = new node;
    if (!iRoot) return DICTIONARY_MEM;
    rc = iRoot->create(iKeyTemp, iKeyLen, iValTemp, iValLen, NULL, NULL);

#ifdef _LIBDEBUG_
    Serial.printf("DICT-insert: creating root entry. rc = %d\n", rc);
#endif

    if (rc) {
      delete iRoot;
      return rc;
    }
    rc = Q->append(iRoot);
    if (rc) {
      delete iRoot;
      return rc;
    }
  }
  return DICTIONARY_OK;
}


// ==== SEARCHES AND LOOKUPS ===============================================
String Dictionary::search(const char* keystr) {
    iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);
#ifdef _DICT_COMPRESS
    int8_t rc;
#endif

    if (iKeyLen != 0 && iKeyLen <= _DICT_KEYLEN) {
#ifdef _DICT_COMPRESS
        if ( (rc = compressKey(keystr)) ) return String("");
#else
        iKeyTemp = (char*) keystr;
#endif
        uintNN_t key = crc(iKeyTemp, iKeyLen);

        node* p = search(key, iRoot, iKeyTemp, iKeyLen);
        if (p) {
#ifdef _DICT_COMPRESS
            decompressValue(p->valbuf, p->vsize);
#else
            iValTemp = p->valbuf;
            iValTemp[p->vsize] = 0;
#endif
            return String(iValTemp);
        }
    }
    return String("");
}

String Dictionary::key(size_t i) {
  if (Q) {
    node* p = (*Q)[i];
    if (p) {
#ifdef _DICT_COMPRESS
        decompressKey(p->keybuf, p->ksize);
#else
        iKeyTemp = p->keybuf;
        iKeyTemp[p->ksize] = 0;
#endif
        return String(iKeyTemp);
    }
  }
  return String();
}

String Dictionary::value(size_t i) {
    if (Q) {
        node* p = (*Q)[i];
        if (p) {
#ifdef _LIBDEBUG_
    Serial.printf("Dictionary::value:\n");
    Serial.printf("\tFound ptr = %u (%u:%d)\n", (uint32_t)p, (uint32_t)p->valbuf, p->vsize);
#endif
#ifdef _DICT_COMPRESS
            decompressValue(p->valbuf, p->vsize);
#else
            iValTemp = p->valbuf;
            iValTemp[p->vsize] = 0;
#endif
            return String(iValTemp);
        }
    }
    return String();
}


// ==== DELETES =====================================================
void Dictionary::destroy() {
    destroy_tree(iRoot);
    iRoot = NULL;
    delete Q;
    Q = new NodeArray(initSize);
}

int8_t Dictionary::remove(const String& keystr) {
    return remove(keystr.c_str());
}

int8_t Dictionary::remove(const char* keystr) {
#ifdef _LIBDEBUG_
    Serial.printf("Dictionary::remove: %s\n", keystr);
#endif
#ifdef _DICT_COMPRESS
    int8_t rc;
#endif
    iKeyLen = strnlen(keystr, _DICT_KEYLEN + 1);
    if (iKeyLen > _DICT_KEYLEN) return DICTIONARY_ERR;

#ifdef _DICT_COMPRESS
    if ( (rc = compressKey(keystr)) ) return rc;
#else
    iKeyTemp = (char*) keystr;
#endif

    uintNN_t key = crc(iKeyTemp, iKeyLen);
    node* p = search(key, iRoot, iKeyTemp, iKeyLen);

    if (p) {
#ifdef _LIBDEBUG_
        Serial.printf("Found key to delete int: %u\n", p->key());
        Serial.printf("Found key to delete ptr: %u\n", (uint32_t)p);
//        Serial.printf("Found key to delete str: %s\n", keystr);
#endif
        iRoot = deleteNode(iRoot, p->key(), iKeyTemp, iKeyLen);
    }
    return DICTIONARY_OK;
}


// ==== SIZES ============================================================================
// This is the size of the Dictionary in memory (just data, not object)
size_t Dictionary::size() {
    size_t ct = count();
    size_t sz = 0;
    for (size_t i = 0; i < ct; i++) {
        sz += (*Q)[i]->ksize;
        sz += (*Q)[i]->vsize;
        sz += sizeof(node);  // to account for size of the node itself
    }
    return sz;
}

// This is size of JSON file to be created out of this dictionary
size_t Dictionary::jsize() {
    size_t ct = count();
    // {"key":"value","key":"value"}\0:
    // 3 (2 brackets and 1 zero terminator) + 4 quotes, a comma and a semicolon = 6 chars)
    // minus one last comma
    size_t sz = 2 + ct * 6;
    for (size_t i = 0; i < ct; i++) {
        sz += key(i).length();
        sz += value(i).length();
    }
    return sz;
}

// This is size method for storing in EEPROM
size_t Dictionary::esize() {
    size_t ct = count();
    size_t sz = 0;

    for (size_t i = 0; i < ct; i++) {
        sz += key(i).length() + 1;
        sz += value(i).length() + 1;
    }
    return sz;
}


// ==== JSON RELATED ================================================
String Dictionary::json() {
    String s;

    s.reserve(jsize());
    s = '{';

    size_t ct = count();
    for (size_t i = 0; i < ct; i++) {
        String vv = value(i);
        vv.replace("\"","\\\"");
        s += '"' + key(i) + "\":\"" + vv + '"';
        if (i < ct - 1) s += ',';
    }
    s += '}';

    return s;
}

int8_t Dictionary::jload(const String& json, int aNum) {
  ReadBufferStream stream( (uint8_t*)json.c_str(), json.length() );
  return jload(stream, aNum);
}

int8_t Dictionary::jload(Stream& json, int aNum) {
    bool insideQoute = false;
    bool nextVerbatim = false;
    bool isValue = false;
    bool isComment = false;
    int p = 0;
    int8_t rc;
    String currentKey;
    String currentValue;

    while ( json.peek() >= 0 ) {
        char c = json.read();

        if ( isComment ) {
          if ( c == '\n' ) {
            isComment = false;
            isValue = false;
          }
          continue;
        }
        if (nextVerbatim) {
          nextVerbatim = false;
        }
        
        //  not a comment and not a verbatim char
        else {
          // process all special cases: '\', '"', ':', and ','
          if (c == '\\' ) {
            nextVerbatim = true;
            continue;
          }

          if ( c == '\"' ) {
            if (!insideQoute) {
              if ( isValue ) {
                if ( currentValue.length() > 0 ) return DICTIONARY_FMT;
              }
              else {
                if ( currentKey.length() > 0 ) return DICTIONARY_FMT;
              }
              insideQoute = true;
              continue;
            }
            else {
              insideQoute = false;
              continue;
            }
          }
          
          if (c == '\n') {
            if ( insideQoute ) {
              return DICTIONARY_QUOTE;
            }
            if ( nextVerbatim ) {
              return DICTIONARY_BCKSL;
            }
          }
          
#ifdef _DICT_ASCII_ONLY
          if ( c > 127 ) continue;  //  ignore non-ascii characters
#endif
          
          if (!insideQoute) {
            if ( c == '#' ) {
              isComment = true;
              continue;
            }

            if (c == ':') {
              if ( isValue ) {
                return DICTIONARY_COMMA; //missing comma probably
              }
              isValue = true;
              continue;
            }

            if ( c == '{' || c == ' ' || c == '\t'  || c == '\r' ) continue;
            
            if ( c == ',' || c == '\n' || c == '}') {
              if ( isValue ) {
                if ( currentValue.length() == 0 ) return DICTIONARY_FMT;
                isValue = false;
                rc = insert( currentKey, currentValue );
                if (rc) return DICTIONARY_MEM;  // if error - exit with an error code
                currentValue = String();
                currentKey = String();
                p++;
                if (aNum > 0 && p >= aNum) break;
              }
              else {
                if ( c == ',' ) return DICTIONARY_FMT;
              }
              continue;
            }
          }
        }
        if (isValue) currentValue.concat(c);
        else currentKey.concat(c);
      }
      if (insideQoute || nextVerbatim || (aNum > 0 && p < aNum )) return DICTIONARY_EOF;

      return DICTIONARY_OK;
}


int8_t Dictionary::merge(Dictionary& dict) {
    size_t ct = dict.count();
    int8_t rc;

    for (size_t i = 0; i < ct; i++) {
        rc = insert(dict(i), dict[i]);
        if (rc) return rc;
    }
    return DICTIONARY_OK;
}

// ==== OPERATORS ====================================

bool Dictionary::operator () (const String& keystr) {
    iKeyLen = keystr.length();
    if (iKeyLen > _DICT_KEYLEN) return false;

#ifdef _DICT_COMPRESS
    int8_t rc;
    if ( (rc = compressKey(keystr.c_str())) ) return false;
#else
    iKeyTemp = (char*) keystr.c_str();
#endif

    uintNN_t key = crc(iKeyTemp, iKeyLen);

    node* p = search(key, iRoot, iKeyTemp, iKeyLen);
    if (p) return true;
    return false;
}


bool Dictionary::operator == (Dictionary& b) {
    if (b.size() != size()) return false;
    if (b.count() != count()) return false;
    size_t ct = count();
    for (size_t i = 0; i < ct; i++) {
        if (value(i) != b[key(i)]) return false;
    }
    return true;
}




// ==== PRIVATE METHODS ====================================================
// ==== INSERTS ============================================================
int8_t Dictionary::insert(uintNN_t key, const char* keystr, _DICT_KEY_TYPE keylen, const char* valstr, _DICT_VAL_TYPE vallen, node* leaf) {
    if (key < leaf->key()) {
        if (leaf->left != NULL)
            return insert(key, keystr, keylen, valstr, vallen, leaf->left);
        else {
            int8_t rc;

            leaf->left = new node;
            if (!leaf->left) return DICTIONARY_MEM;
            rc = leaf->left->create(keystr, keylen, valstr, vallen, NULL, NULL);
            if (rc) {
                delete leaf->left;
                return rc;
            }
            rc = Q->append(leaf->left);
            if (rc) {
                delete leaf->left;
                return rc;
            }
        }
    }
    else if (key > leaf->key()) {
        if (leaf->right != NULL)
            return insert(key, keystr, keylen, valstr, vallen, leaf->right);
        else {
            int8_t rc;

            leaf->right = new node;
            if (!leaf->right) return DICTIONARY_MEM;
            rc = leaf->right->create(keystr, keylen, valstr, vallen, NULL, NULL);
            if (rc) {
                delete leaf->right;
                return rc;
            }
            rc = Q->append(leaf->right);
            if (rc) {
                delete leaf->right;
                return rc;
            }
        }
    }
    else if (key == leaf->key()) {
        int cmpres = keylen != leaf->ksize ? keylen - leaf->ksize : memcmp(leaf->keybuf, keystr, keylen);
        if (cmpres == 0) {
            if (leaf->updateValue(valstr, vallen) != NODEARRAY_OK) return DICTIONARY_MEM;
        }
        else {

            if (cmpres < 0) {
                if (leaf->left != NULL)
                    return insert(key, keystr, keylen, valstr, vallen, leaf->left);
                else {
                    int8_t rc;

                    leaf->left = new node;
                    if (!leaf->left) return DICTIONARY_MEM;
                    rc = leaf->left->create(keystr, keylen, valstr, vallen, NULL, NULL);
                    if (rc) {
                        delete leaf->left;
                        return rc;
                    }
                    rc = Q->append(leaf->left);
                    if (rc) {
                        delete leaf->left;
                        return rc;
                    }
                }
            }
            else if (cmpres > 0) {
                if (leaf->right != NULL)
                    return insert(key, keystr, keylen, valstr, vallen, leaf->right);
                else {
                    int8_t rc;

                    leaf->right = new node;
                    if (!leaf->right) return DICTIONARY_MEM;
                    rc = leaf->right->create(keystr, keylen, valstr, vallen, NULL, NULL);
                    if (rc) {
                        delete leaf->right;
                        return rc;
                    }
                    rc = Q->append(leaf->right);
                    if (rc) {
                        delete leaf->right;
                        return rc;
                    }
                }
            }
        }
    }
    return DICTIONARY_OK;
}


// ==== SEARCH ===========================================================================
node* Dictionary::search(uintNN_t key, node* leaf, const char* keystr, _DICT_KEY_TYPE keylen) {
    if (leaf != NULL) {
        if ( key == leaf->key() ) {
            int cmpres = keylen != leaf->ksize ? keylen - leaf->ksize : memcmp(leaf->keybuf, keystr, keylen);

            if (cmpres == 0) {
                return leaf;
            }
            else {
                if ( cmpres < 0 )
                    return search(key, leaf->left, keystr, keylen);
                else
                    return search(key, leaf->right, keystr, keylen);
            }
        }
        else {
            if ( key < leaf->key() )
                return search(key, leaf->left, keystr, keylen);
            else
                return search(key, leaf->right, keystr, keylen);
        }
    }
    else return NULL;
}


// ==== DELETES ==========================================================================
void Dictionary::destroy_tree(node* leaf) {
  if (leaf != NULL) {
    destroy_tree(leaf->left);
    destroy_tree(leaf->right);
    delete leaf; // node destructor takes care of the key and value strings
    leaf = NULL;
  }
}


node* Dictionary::deleteNode(node* root, uintNN_t key, const char* keystr, _DICT_KEY_TYPE keylen) {
  if (root == NULL) return root;

  if (key < root->key() ) {
    root->left = deleteNode(root->left, key, keystr, keylen);
  }
  // If the key to be deleted is greater than the root's key,
  // then it lies in right subtree
  else if (key > root->key() ) {
    root->right = deleteNode(root->right, key, keystr, keylen);
  }
  // if key is same as root's key, then This is the node
  // to be deleted
  else {
    int cmpres = (keylen != root->ksize) ? keylen - root->ksize : memcmp(root->keybuf, keystr, keylen);
    if (cmpres == 0 ) {
      // node with only one child or no child
      if (root->left == NULL) {
        node* temp = root->right;
        Q->remove(root);
        delete root;
        root = NULL;
        return temp;
      }
      else if (root->right == NULL) {
        node* temp = root->left;
        Q->remove(root);
        delete root;
        root = NULL;
        return temp;
      }

      // node with two children: Get the in-order successor (smallest
      // in the right subtree)
      node* temp = minValueNode(root->right);

      // Copy the in-order successor's content to this node
      root->updateKey(temp->keybuf, temp->ksize);
      root->updateValue(temp->valbuf, temp->vsize);

      // Delete the in-order successor
      root->right = deleteNode(root->right, temp->key(), temp->keybuf, temp->ksize);
    }
    else {
        if ( cmpres < 0 ) {
            root->left = deleteNode(root->left, key, keystr, keylen);
        }
        // If the key to be deleted is greater than the root's key,
        // then it lies in right subtree
        else if ( cmpres > 0 ) {
            root->right = deleteNode(root->right, key, keystr, keylen);
        }
    }
  }
  return root;
}


node* Dictionary::minValueNode(node* n) {
  node* current = n;

  /* loop down to find the leftmost leaf */
  while (current && current->left) current = current->left;

  return current;
}


// ==== KEY/CRC METHODS ===============================================

uintNN_t Dictionary::crc(const void* data, size_t n_bytes) {
    uintNN_t    a = 0;

    memcpy((void*)&a, data, n_bytes < sizeof(uintNN_t) ? n_bytes : sizeof(uintNN_t));
    return a;
}


#ifdef _DICT_COMPRESS

// ==== COMPRESS METHODS =============================================
int8_t Dictionary::compressKey(const char* aStr) {
    memset(iKeyTemp, 0, sizeof(uintNN_t));

#if defined (_DICT_COMPRESS_SHOCO)
    iKeyLen = shoco_compress(aStr, 0, iKeyTemp, _DICT_KEYLEN + 1);

#elif defined (_DICT_COMPRESS_SMAZ)
    iKeyLen = smaz_compress((char*) aStr, strlen(aStr), iKeyTemp, _DICT_KEYLEN + 1);

// #else
//     iKeyLen = strlen(aStr);
//     memcpy(iKeyTemp, aStr, iKeyLen);

#endif

    if (iKeyLen > _DICT_KEYLEN + 1) return DICTIONARY_OOB;

    return DICTIONARY_OK;
}

int8_t Dictionary::compressValue(const char* aStr) {

#if defined (_DICT_COMPRESS_SHOCO) 
    iValLen = shoco_compress(aStr, 0, iValTemp, _DICT_VALLEN + 1);

#elif defined (_DICT_COMPRESS_SMAZ)
    iValLen = smaz_compress((char*) aStr, strlen(aStr), iValTemp, _DICT_VALLEN + 1);

#endif

    if (iValLen > _DICT_VALLEN + 1) return DICTIONARY_OOB;

    return DICTIONARY_OK;
}

void Dictionary::decompressKey(const char* aBuf, _DICT_KEY_TYPE aLen) {

#if defined (_DICT_COMPRESS_SHOCO)
    iKeyLen = shoco_decompress(aBuf, aLen, iKeyTemp, _DICT_KEYLEN + 1);
    iKeyTemp[iKeyLen] = 0;

#elif defined (_DICT_COMPRESS_SMAZ)
    iKeyLen = smaz_decompress((char*) aBuf, (int) aLen, iKeyTemp, (int) (_DICT_KEYLEN + 1));
    iKeyTemp[iKeyLen] = 0;

#endif
}

void Dictionary::decompressValue(const char* aBuf, _DICT_VAL_TYPE aLen) {
    
#if defined (_DICT_COMPRESS_SHOCO)
    iValLen = shoco_decompress(aBuf, aLen, iValTemp, _DICT_VALLEN + 1);
    iValTemp[iValLen] = 0;

#elif defined (_DICT_COMPRESS_SMAZ)
    iValLen = smaz_decompress((char*) aBuf, (int) aLen, iValTemp, (int) (_DICT_VALLEN + 1) );
    iValTemp[iValLen] = 0;

#endif
}

#endif // _DICT_COMPRESS


// ==== DEBUG METHODS ===================================================
#ifdef _LIBDEBUG_
void Dictionary::printDictionary(node* root) {
  if (root != NULL)
  {
    printDictionary(root->left);
    printNode(root);
    printDictionary(root->right);
  }
}

void Dictionary::printNode(node* root) {
  if (root != NULL) {
//    Serial.printf("%u: (%u:%s,%s %u:%u) [l:%u, r:%u]\n", (uint32_t)root, root->key(), root->keybuf, root->valbuf, (uint32_t)root->keybuf, (uint32_t)root->valbuf, (uint32_t)root->left, (uint32_t)root->right);
    Serial.printf("%u: (%u:%u:%u) [l:%u, r:%u]\n", (uint32_t)root, root->key(), (uint32_t)root->keybuf, (uint32_t)root->valbuf, (uint32_t)root->left, (uint32_t)root->right);
  }
  else {
    Serial.println("NULL:");
  }
}
#endif  //  _LIBDEBUG_







