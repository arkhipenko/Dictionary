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

  v1.0.0:
    2020-04-09 - Initial release

  v1.0.1:
    2020-04-10 - feature: operator (), examples, benchmarks
    
  v1.0.2:
    2020-04-10 - feature: operators == and !=
                 bug: memory leak after destroy method call. 
                 
  v1.1.0:
    2020-04-12 - feature: delete a node method.
                 feature: Dictionary Array optimization
                 
  v1.1.1:
    2020-04-13 - feature: check if key exists via d("key")
    
  v1.2.0:
    2020-04-25 - bug: incorrect node handling during deletion
                 performance improvements
                 
  v1.2.1:
    2020-04-26 - feature: switched to static crc tables

*/


#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

#include "NodeArray.h"


#ifdef _DICT_CRC64_
static const uintNN_t DictionaryCRCTable[256] = {
  0x0ULL, 0xb32e4cbe03a75f6fULL, 0xf4843657a840a05bULL, 0x47aa7ae9abe7ff34ULL, 
  0x7bd0c384ff8f5e33ULL, 0xc8fe8f3afc28015cULL, 0x8f54f5d357cffe68ULL, 0x3c7ab96d5468a107ULL, 
  0xf7a18709ff1ebc66ULL, 0x448fcbb7fcb9e309ULL, 0x325b15e575e1c3dULL, 0xb00bfde054f94352ULL, 
  0x8c71448d0091e255ULL, 0x3f5f08330336bd3aULL, 0x78f572daa8d1420eULL, 0xcbdb3e64ab761d61ULL, 
  0x7d9ba13851336649ULL, 0xceb5ed8652943926ULL, 0x891f976ff973c612ULL, 0x3a31dbd1fad4997dULL, 
  0x64b62bcaebc387aULL, 0xb5652e02ad1b6715ULL, 0xf2cf54eb06fc9821ULL, 0x41e11855055bc74eULL, 
  0x8a3a2631ae2dda2fULL, 0x39146a8fad8a8540ULL, 0x7ebe1066066d7a74ULL, 0xcd905cd805ca251bULL, 
  0xf1eae5b551a2841cULL, 0x42c4a90b5205db73ULL, 0x56ed3e2f9e22447ULL, 0xb6409f5cfa457b28ULL, 
  0xfb374270a266cc92ULL, 0x48190ecea1c193fdULL, 0xfb374270a266cc9ULL, 0xbc9d3899098133a6ULL, 
  0x80e781f45de992a1ULL, 0x33c9cd4a5e4ecdceULL, 0x7463b7a3f5a932faULL, 0xc74dfb1df60e6d95ULL, 
  0xc96c5795d7870f4ULL, 0xbfb889c75edf2f9bULL, 0xf812f32ef538d0afULL, 0x4b3cbf90f69f8fc0ULL, 
  0x774606fda2f72ec7ULL, 0xc4684a43a15071a8ULL, 0x83c230aa0ab78e9cULL, 0x30ec7c140910d1f3ULL, 
  0x86ace348f355aadbULL, 0x3582aff6f0f2f5b4ULL, 0x7228d51f5b150a80ULL, 0xc10699a158b255efULL, 
  0xfd7c20cc0cdaf4e8ULL, 0x4e526c720f7dab87ULL, 0x9f8169ba49a54b3ULL, 0xbad65a25a73d0bdcULL, 
  0x710d64410c4b16bdULL, 0xc22328ff0fec49d2ULL, 0x85895216a40bb6e6ULL, 0x36a71ea8a7ace989ULL, 
  0xadda7c5f3c4488eULL, 0xb9f3eb7bf06317e1ULL, 0xfe5991925b84e8d5ULL, 0x4d77dd2c5823b7baULL, 
  0x64b62bcaebc387a1ULL, 0xd7986774e864d8ceULL, 0x90321d9d438327faULL, 0x231c512340247895ULL, 
  0x1f66e84e144cd992ULL, 0xac48a4f017eb86fdULL, 0xebe2de19bc0c79c9ULL, 0x58cc92a7bfab26a6ULL, 
  0x9317acc314dd3bc7ULL, 0x2039e07d177a64a8ULL, 0x67939a94bc9d9b9cULL, 0xd4bdd62abf3ac4f3ULL, 
  0xe8c76f47eb5265f4ULL, 0x5be923f9e8f53a9bULL, 0x1c4359104312c5afULL, 0xaf6d15ae40b59ac0ULL, 
  0x192d8af2baf0e1e8ULL, 0xaa03c64cb957be87ULL, 0xeda9bca512b041b3ULL, 0x5e87f01b11171edcULL, 
  0x62fd4976457fbfdbULL, 0xd1d305c846d8e0b4ULL, 0x96797f21ed3f1f80ULL, 0x2557339fee9840efULL, 
  0xee8c0dfb45ee5d8eULL, 0x5da24145464902e1ULL, 0x1a083bacedaefdd5ULL, 0xa9267712ee09a2baULL, 
  0x955cce7fba6103bdULL, 0x267282c1b9c65cd2ULL, 0x61d8f8281221a3e6ULL, 0xd2f6b4961186fc89ULL, 
  0x9f8169ba49a54b33ULL, 0x2caf25044a02145cULL, 0x6b055fede1e5eb68ULL, 0xd82b1353e242b407ULL, 
  0xe451aa3eb62a1500ULL, 0x577fe680b58d4a6fULL, 0x10d59c691e6ab55bULL, 0xa3fbd0d71dcdea34ULL, 
  0x6820eeb3b6bbf755ULL, 0xdb0ea20db51ca83aULL, 0x9ca4d8e41efb570eULL, 0x2f8a945a1d5c0861ULL, 
  0x13f02d374934a966ULL, 0xa0de61894a93f609ULL, 0xe7741b60e174093dULL, 0x545a57dee2d35652ULL, 
  0xe21ac88218962d7aULL, 0x5134843c1b317215ULL, 0x169efed5b0d68d21ULL, 0xa5b0b26bb371d24eULL, 
  0x99ca0b06e7197349ULL, 0x2ae447b8e4be2c26ULL, 0x6d4e3d514f59d312ULL, 0xde6071ef4cfe8c7dULL, 
  0x15bb4f8be788911cULL, 0xa6950335e42fce73ULL, 0xe13f79dc4fc83147ULL, 0x521135624c6f6e28ULL, 
  0x6e6b8c0f1807cf2fULL, 0xdd45c0b11ba09040ULL, 0x9aefba58b0476f74ULL, 0x29c1f6e6b3e0301bULL, 
  0xc96c5795d7870f42ULL, 0x7a421b2bd420502dULL, 0x3de861c27fc7af19ULL, 0x8ec62d7c7c60f076ULL, 
  0xb2bc941128085171ULL, 0x192d8af2baf0e1eULL, 0x4638a2468048f12aULL, 0xf516eef883efae45ULL, 
  0x3ecdd09c2899b324ULL, 0x8de39c222b3eec4bULL, 0xca49e6cb80d9137fULL, 0x7967aa75837e4c10ULL, 
  0x451d1318d716ed17ULL, 0xf6335fa6d4b1b278ULL, 0xb199254f7f564d4cULL, 0x2b769f17cf11223ULL, 
  0xb4f7f6ad86b4690bULL, 0x7d9ba1385133664ULL, 0x4073c0fa2ef4c950ULL, 0xf35d8c442d53963fULL, 
  0xcf273529793b3738ULL, 0x7c0979977a9c6857ULL, 0x3ba3037ed17b9763ULL, 0x888d4fc0d2dcc80cULL, 
  0x435671a479aad56dULL, 0xf0783d1a7a0d8a02ULL, 0xb7d247f3d1ea7536ULL, 0x4fc0b4dd24d2a59ULL, 
  0x3886b22086258b5eULL, 0x8ba8fe9e8582d431ULL, 0xcc0284772e652b05ULL, 0x7f2cc8c92dc2746aULL, 
  0x325b15e575e1c3d0ULL, 0x8175595b76469cbfULL, 0xc6df23b2dda1638bULL, 0x75f16f0cde063ce4ULL, 
  0x498bd6618a6e9de3ULL, 0xfaa59adf89c9c28cULL, 0xbd0fe036222e3db8ULL, 0xe21ac88218962d7ULL, 
  0xc5fa92ec8aff7fb6ULL, 0x76d4de52895820d9ULL, 0x317ea4bb22bfdfedULL, 0x8250e80521188082ULL, 
  0xbe2a516875702185ULL, 0xd041dd676d77eeaULL, 0x4aae673fdd3081deULL, 0xf9802b81de97deb1ULL, 
  0x4fc0b4dd24d2a599ULL, 0xfceef8632775faf6ULL, 0xbb44828a8c9205c2ULL, 0x86ace348f355aadULL, 
  0x34107759db5dfbaaULL, 0x873e3be7d8faa4c5ULL, 0xc094410e731d5bf1ULL, 0x73ba0db070ba049eULL, 
  0xb86133d4dbcc19ffULL, 0xb4f7f6ad86b4690ULL, 0x4ce50583738cb9a4ULL, 0xffcb493d702be6cbULL, 
  0xc3b1f050244347ccULL, 0x709fbcee27e418a3ULL, 0x3735c6078c03e797ULL, 0x841b8ab98fa4b8f8ULL, 
  0xadda7c5f3c4488e3ULL, 0x1ef430e13fe3d78cULL, 0x595e4a08940428b8ULL, 0xea7006b697a377d7ULL, 
  0xd60abfdbc3cbd6d0ULL, 0x6524f365c06c89bfULL, 0x228e898c6b8b768bULL, 0x91a0c532682c29e4ULL, 
  0x5a7bfb56c35a3485ULL, 0xe955b7e8c0fd6beaULL, 0xaeffcd016b1a94deULL, 0x1dd181bf68bdcbb1ULL, 
  0x21ab38d23cd56ab6ULL, 0x9285746c3f7235d9ULL, 0xd52f0e859495caedULL, 0x6601423b97329582ULL, 
  0xd041dd676d77eeaaULL, 0x636f91d96ed0b1c5ULL, 0x24c5eb30c5374ef1ULL, 0x97eba78ec690119eULL, 
  0xab911ee392f8b099ULL, 0x18bf525d915feff6ULL, 0x5f1528b43ab810c2ULL, 0xec3b640a391f4fadULL, 
  0x27e05a6e926952ccULL, 0x94ce16d091ce0da3ULL, 0xd3646c393a29f297ULL, 0x604a2087398eadf8ULL, 
  0x5c3099ea6de60cffULL, 0xef1ed5546e415390ULL, 0xa8b4afbdc5a6aca4ULL, 0x1b9ae303c601f3cbULL, 
  0x56ed3e2f9e224471ULL, 0xe5c372919d851b1eULL, 0xa26908783662e42aULL, 0x114744c635c5bb45ULL, 
  0x2d3dfdab61ad1a42ULL, 0x9e13b115620a452dULL, 0xd9b9cbfcc9edba19ULL, 0x6a978742ca4ae576ULL, 
  0xa14cb926613cf817ULL, 0x1262f598629ba778ULL, 0x55c88f71c97c584cULL, 0xe6e6c3cfcadb0723ULL, 
  0xda9c7aa29eb3a624ULL, 0x69b2361c9d14f94bULL, 0x2e184cf536f3067fULL, 0x9d36004b35545910ULL, 
  0x2b769f17cf112238ULL, 0x9858d3a9ccb67d57ULL, 0xdff2a94067518263ULL, 0x6cdce5fe64f6dd0cULL, 
  0x50a65c93309e7c0bULL, 0xe388102d33392364ULL, 0xa4226ac498dedc50ULL, 0x170c267a9b79833fULL, 
  0xdcd7181e300f9e5eULL, 0x6ff954a033a8c131ULL, 0x28532e49984f3e05ULL, 0x9b7d62f79be8616aULL, 
  0xa707db9acf80c06dULL, 0x14299724cc279f02ULL, 0x5383edcd67c06036ULL, 0xe0ada17364673f59ULL 
};
#else
static const uintNN_t DictionaryCRCTable[256] = {
  0xd202ef8dUL, 0xa505df1bUL, 0x3c0c8ea1UL, 0x4b0bbe37UL, 0xd56f2b94UL, 0xa2681b02UL, 0x3b614ab8UL, 0x4c667a2eUL, 
  0xdcd967bfUL, 0xabde5729UL, 0x32d70693UL, 0x45d03605UL, 0xdbb4a3a6UL, 0xacb39330UL, 0x35bac28aUL, 0x42bdf21cUL, 
  0xcfb5ffe9UL, 0xb8b2cf7fUL, 0x21bb9ec5UL, 0x56bcae53UL, 0xc8d83bf0UL, 0xbfdf0b66UL, 0x26d65adcUL, 0x51d16a4aUL, 
  0xc16e77dbUL, 0xb669474dUL, 0x2f6016f7UL, 0x58672661UL, 0xc603b3c2UL, 0xb1048354UL, 0x280dd2eeUL, 0x5f0ae278UL, 
  0xe96ccf45UL, 0x9e6bffd3UL, 0x762ae69UL, 0x70659effUL, 0xee010b5cUL, 0x99063bcaUL, 0xf6a70UL, 0x77085ae6UL, 
  0xe7b74777UL, 0x90b077e1UL, 0x9b9265bUL, 0x7ebe16cdUL, 0xe0da836eUL, 0x97ddb3f8UL, 0xed4e242UL, 0x79d3d2d4UL, 
  0xf4dbdf21UL, 0x83dcefb7UL, 0x1ad5be0dUL, 0x6dd28e9bUL, 0xf3b61b38UL, 0x84b12baeUL, 0x1db87a14UL, 0x6abf4a82UL, 
  0xfa005713UL, 0x8d076785UL, 0x140e363fUL, 0x630906a9UL, 0xfd6d930aUL, 0x8a6aa39cUL, 0x1363f226UL, 0x6464c2b0UL, 
  0xa4deae1dUL, 0xd3d99e8bUL, 0x4ad0cf31UL, 0x3dd7ffa7UL, 0xa3b36a04UL, 0xd4b45a92UL, 0x4dbd0b28UL, 0x3aba3bbeUL, 
  0xaa05262fUL, 0xdd0216b9UL, 0x440b4703UL, 0x330c7795UL, 0xad68e236UL, 0xda6fd2a0UL, 0x4366831aUL, 0x3461b38cUL, 
  0xb969be79UL, 0xce6e8eefUL, 0x5767df55UL, 0x2060efc3UL, 0xbe047a60UL, 0xc9034af6UL, 0x500a1b4cUL, 0x270d2bdaUL, 
  0xb7b2364bUL, 0xc0b506ddUL, 0x59bc5767UL, 0x2ebb67f1UL, 0xb0dff252UL, 0xc7d8c2c4UL, 0x5ed1937eUL, 0x29d6a3e8UL, 
  0x9fb08ed5UL, 0xe8b7be43UL, 0x71beeff9UL, 0x6b9df6fUL, 0x98dd4accUL, 0xefda7a5aUL, 0x76d32be0UL, 0x1d41b76UL, 
  0x916b06e7UL, 0xe66c3671UL, 0x7f6567cbUL, 0x862575dUL, 0x9606c2feUL, 0xe101f268UL, 0x7808a3d2UL, 0xf0f9344UL, 
  0x82079eb1UL, 0xf500ae27UL, 0x6c09ff9dUL, 0x1b0ecf0bUL, 0x856a5aa8UL, 0xf26d6a3eUL, 0x6b643b84UL, 0x1c630b12UL, 
  0x8cdc1683UL, 0xfbdb2615UL, 0x62d277afUL, 0x15d54739UL, 0x8bb1d29aUL, 0xfcb6e20cUL, 0x65bfb3b6UL, 0x12b88320UL, 
  0x3fba6cadUL, 0x48bd5c3bUL, 0xd1b40d81UL, 0xa6b33d17UL, 0x38d7a8b4UL, 0x4fd09822UL, 0xd6d9c998UL, 0xa1def90eUL, 
  0x3161e49fUL, 0x4666d409UL, 0xdf6f85b3UL, 0xa868b525UL, 0x360c2086UL, 0x410b1010UL, 0xd80241aaUL, 0xaf05713cUL, 
  0x220d7cc9UL, 0x550a4c5fUL, 0xcc031de5UL, 0xbb042d73UL, 0x2560b8d0UL, 0x52678846UL, 0xcb6ed9fcUL, 0xbc69e96aUL, 
  0x2cd6f4fbUL, 0x5bd1c46dUL, 0xc2d895d7UL, 0xb5dfa541UL, 0x2bbb30e2UL, 0x5cbc0074UL, 0xc5b551ceUL, 0xb2b26158UL, 
  0x4d44c65UL, 0x73d37cf3UL, 0xeada2d49UL, 0x9ddd1ddfUL, 0x3b9887cUL, 0x74beb8eaUL, 0xedb7e950UL, 0x9ab0d9c6UL, 
  0xa0fc457UL, 0x7d08f4c1UL, 0xe401a57bUL, 0x930695edUL, 0xd62004eUL, 0x7a6530d8UL, 0xe36c6162UL, 0x946b51f4UL, 
  0x19635c01UL, 0x6e646c97UL, 0xf76d3d2dUL, 0x806a0dbbUL, 0x1e0e9818UL, 0x6909a88eUL, 0xf000f934UL, 0x8707c9a2UL, 
  0x17b8d433UL, 0x60bfe4a5UL, 0xf9b6b51fUL, 0x8eb18589UL, 0x10d5102aUL, 0x67d220bcUL, 0xfedb7106UL, 0x89dc4190UL, 
  0x49662d3dUL, 0x3e611dabUL, 0xa7684c11UL, 0xd06f7c87UL, 0x4e0be924UL, 0x390cd9b2UL, 0xa0058808UL, 0xd702b89eUL, 
  0x47bda50fUL, 0x30ba9599UL, 0xa9b3c423UL, 0xdeb4f4b5UL, 0x40d06116UL, 0x37d75180UL, 0xaede003aUL, 0xd9d930acUL, 
  0x54d13d59UL, 0x23d60dcfUL, 0xbadf5c75UL, 0xcdd86ce3UL, 0x53bcf940UL, 0x24bbc9d6UL, 0xbdb2986cUL, 0xcab5a8faUL, 
  0x5a0ab56bUL, 0x2d0d85fdUL, 0xb404d447UL, 0xc303e4d1UL, 0x5d677172UL, 0x2a6041e4UL, 0xb369105eUL, 0xc46e20c8UL, 
  0x72080df5UL, 0x50f3d63UL, 0x9c066cd9UL, 0xeb015c4fUL, 0x7565c9ecUL, 0x262f97aUL, 0x9b6ba8c0UL, 0xec6c9856UL, 
  0x7cd385c7UL, 0xbd4b551UL, 0x92dde4ebUL, 0xe5dad47dUL, 0x7bbe41deUL, 0xcb97148UL, 0x95b020f2UL, 0xe2b71064UL, 
  0x6fbf1d91UL, 0x18b82d07UL, 0x81b17cbdUL, 0xf6b64c2bUL, 0x68d2d988UL, 0x1fd5e91eUL, 0x86dcb8a4UL, 0xf1db8832UL, 
  0x616495a3UL, 0x1663a535UL, 0x8f6af48fUL, 0xf86dc419UL, 0x660951baUL, 0x110e612cUL, 0x88073096UL, 0xff000000UL 
};
#endif



class Dictionary {
  public:
    Dictionary(size_t init_size = 10);
    ~Dictionary();

    void insert(String keystr, String valstr);
    String search(String keystr);
    void destroy();
    void remove(String keystr);

    String key(unsigned int i) {
      node* p = (*Q)[i];
      if (p) return p->keystr;
      else return String();
    }
    String value(unsigned int i) {
      node* p = (*Q)[i];
      if (p) return p->valstr;
      else return String();
    }
    String operator [] (String keystr) {
      return search(keystr);
    }
    String operator [] (unsigned int i) {
      return value(i);
    }
    void operator () (String keystr, String valstr) {
      insert (keystr, valstr);
    }
    
    bool operator () (String keystr);
    
    String operator () (unsigned int i) {
      return key(i);
    }
    bool operator == (Dictionary& b);
    inline bool operator != (Dictionary& b) {
      return ( !(*this == b));
    }
    inline const size_t count() {
      return Q->count();
    }

    size_t size();

#ifdef _LIBDEBUG_
    void printNode(node* root);
    void printDictionary(node* root);
    void printDictionary() { 
      Serial.printf("\nDictionary::printDictionary:\n");
      printDictionary(iRoot); 
      Serial.println();
    };
    void printArray() { Q->printArray(); };
#endif
  private:
    void     destroy_tree(node* leaf);
    void     insert(uintNN_t key, String* keystr, String* valstr, node* leaf);
    node*    search(uintNN_t key, node* leaf, String* keystr);
    uintNN_t crc(const void* data, size_t n_bytes);

    node*    deleteNode(node* root, uintNN_t key, String* keystr);
    node*    minValueNode(node* n);

    node*               iRoot;
    NodeArray*          Q;
    size_t              initSize;
};

Dictionary::Dictionary(size_t init_size) {
  iRoot = NULL;
  Q = new NodeArray(init_size);
  initSize = init_size;
}


Dictionary::~Dictionary() {
  destroy();
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
      Q->append(leaf->left);
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
      Q->append(leaf->right);
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
        Q->append(leaf->right);
      }
    }
  }
}

bool Dictionary::operator () (String keystr) {
  uintNN_t key = crc(keystr.c_str(), keystr.length());

  node *p = search(key, iRoot, &keystr);
  if (p) return true;
  return false;
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

  if (iRoot != NULL)
    insert(key, &keystr, &valstr, iRoot);
  else {
    iRoot = new node;
    iRoot->key = key;
    iRoot->keystr = keystr;
    iRoot->valstr = valstr;
    iRoot->left = NULL;
    iRoot->right = NULL;
    Q->append(iRoot);
  }
}


String Dictionary::search(String keystr) {
  uintNN_t key = crc(keystr.c_str(), keystr.length());

  node *p = search(key, iRoot, &keystr);
  if (p) return p->valstr;
  return String("");
}


void Dictionary::destroy() {
  destroy_tree(iRoot);
  delete Q;
  Q = new NodeArray(initSize);
}


size_t Dictionary::size() {
  size_t ct = count();
  size_t sz = 0;
  for (size_t i = 0; i < ct; i++) {
    sz += key(i).length();
    sz += value(i).length();
    sz += 2;  // to account for the 2 trailing zeros
  }
  return sz;
}

bool Dictionary::operator == (Dictionary& b) {
  if (b.size() != size() ) return false;
  if (b.count() != count() ) return false;
  size_t ct = count();
  for (size_t i = 0; i < ct; i++) {
    if ( value(i) != b[key(i)] ) return false;
  }
  return true;
}


void Dictionary::remove(String keystr) {
#ifdef _LIBDEBUG_
  Serial.printf( "Dictionary::remove: %s\n", keystr.c_str() );
#endif
  
  uintNN_t key = crc(keystr.c_str(), keystr.length());
  node *p = search(key, iRoot, &keystr);

  if (p) { 
#ifdef _LIBDEBUG_
  Serial.printf( "Found key to delete int: %u\n", p->key );
  Serial.printf( "Found key to delete ptr: %u\n", (uint32_t)p );
  Serial.printf( "Found key to delete str: %s\n", keystr.c_str() );
#endif
    iRoot = deleteNode(iRoot, p->key, &keystr);
  }
}

node* Dictionary::deleteNode(node* root, uintNN_t key, String* keystr) {
  if (root == NULL) return root;

  if (key < root->key)
    root->left = deleteNode(root->left, key, keystr);

  // If the key to be deleted is greater than the root's key,
  // then it lies in right subtree
  else if (key > root->key)
    root->right = deleteNode(root->right, key, keystr);

  // if key is same as root's key, then This is the node
  // to be deleted
  else {
    if (root->keystr == *keystr) {
      // node with only one child or no child
      if (root->left == NULL) {
#ifdef _LIBDEBUG_
  Serial.println("Replacing RIGHT node");
  printNode(root);
  printNode(root->right);
#endif        
        node* temp = root->right;
        Q->remove(root);
        delete root;
        root = NULL;
        return temp;
      } 
      else if (root->right == NULL) {
#ifdef _LIBDEBUG_
  Serial.println("Replacing LEFT node");
  printNode(root);
  printNode(root->left);
#endif 
        node* temp = root->left;
        Q->remove(root);
        delete root;
        root = NULL;
        return temp;
      }

      // node with two children: Get the inorder successor (smallest
      // in the right subtree)
      node* temp = minValueNode(root->right);
#ifdef _LIBDEBUG_
  Serial.println("Replacing minValueNode node");
  printNode(root);
  printNode(temp);
#endif 

      // Copy the inorder successor's content to this node
      root->key = temp->key;
      root->keystr = temp->keystr;
      root->valstr = temp->valstr;

      // Delete the inorder successor
      root->right = deleteNode(root->right, temp->key, &temp->keystr);
    }
    else {
      root->right = deleteNode(root->right, key, keystr);
    }
  }
  return root;
}


node* Dictionary::minValueNode(node* n) {
  node* current = n;

  /* loop down to find the leftmost leaf */
  while (current && current->left != NULL)
    current = current->left;

  return current;
}


#ifdef _LIBDEBUG_
void Dictionary::printDictionary(node* root) { 
    if (root != NULL) 
    { 
        printDictionary(root->left); 
        printNode(root);
        printDictionary(root->right); 
    } 
} 

void Dictionary::printNode(node* root)  { 
    if (root != NULL) { 
        Serial.printf("%u: (%u:%s) [l:%u, r:%u]\n", (uint32_t)root, root->key, (root->keystr).c_str(), (uint32_t)root->left, (uint32_t)root->right);
    } 
    else {
        Serial.println("NULL:");
    }
}
#endif

#ifdef _DICT_CRC64_
uintNN_t Dictionary::crc(const void *p, size_t len) {
  size_t i, t;
  uintNN_t crc = 0;
  uint8_t *_p = (uint8_t*)p;

  for (i = 0; i < len; i++) {
    t = ((crc >> 56) ^ (*_p++)) & 0xFF;
    crc = DictionaryCRCTable[t] ^ (crc << 8);
  }
  return crc;
}
#else
uintNN_t Dictionary::crc(const void *data, size_t n_bytes) {
  uintNN_t crc = 0;
  for (size_t i = 0; i < n_bytes; ++i) {
    crc = DictionaryCRCTable[(uint8_t)crc ^ ((uint8_t*)data)[i]] ^ crc >> 8;
  }
  return crc;
}
#endif

#endif // #define _DICTIONARY_H_