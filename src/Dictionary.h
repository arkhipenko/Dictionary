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

  v1.3.0:
    2020-04-27 - feature: crc 16/32/64 support. 32 is default
    
  v2.0.0:
    2020-05-14 - feature: support PSRAM for ESP32, 
                 Switch to char* for key/values,
                 Error codes for memory-allocating methods
                 Key and Value max length constants

*/


#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

#ifndef _DICT_KEYLEN
#define _DICT_KEYLEN 64
#endif

#ifndef _DICT_VALLEN
#define _DICT_VALLEN 256
#endif

#define DICTIONARY_OK   0
#define DICTIONARY_ERR   (-1)
#define DICTIONARY_MEM   (-2)

#ifndef _DICT_CRC
#define _DICT_CRC  32
#endif

#if _DICT_CRC == 16
#define uintNN_t uint16_t
#endif

#if _DICT_CRC == 32
#define uintNN_t uint32_t
#endif

#if _DICT_CRC == 64
#define uintNN_t uint64_t
#endif

#include <Arduino.h>
#include "NodeArray.h"

#if _DICT_CRC == 64
static const uintNN_t DictionaryCRCTable[256] = {
  0x0ull, 0x42f0e1eba9ea3693ull, 0x85e1c3d753d46d26ull, 0xc711223cfa3e5bb5ull,
  0x493366450e42ecdfull, 0xbc387aea7a8da4cull, 0xccd2a5925d9681f9ull, 0x8e224479f47cb76aull,
  0x9266cc8a1c85d9beull, 0xd0962d61b56fef2dull, 0x17870f5d4f51b498ull, 0x5577eeb6e6bb820bull,
  0xdb55aacf12c73561ull, 0x99a54b24bb2d03f2ull, 0x5eb4691841135847ull, 0x1c4488f3e8f96ed4ull,
  0x663d78ff90e185efull, 0x24cd9914390bb37cull, 0xe3dcbb28c335e8c9ull, 0xa12c5ac36adfde5aull,
  0x2f0e1eba9ea36930ull, 0x6dfeff5137495fa3ull, 0xaaefdd6dcd770416ull, 0xe81f3c86649d3285ull,
  0xf45bb4758c645c51ull, 0xb6ab559e258e6ac2ull, 0x71ba77a2dfb03177ull, 0x334a9649765a07e4ull,
  0xbd68d2308226b08eull, 0xff9833db2bcc861dull, 0x388911e7d1f2dda8ull, 0x7a79f00c7818eb3bull,
  0xcc7af1ff21c30bdeull, 0x8e8a101488293d4dull, 0x499b3228721766f8ull, 0xb6bd3c3dbfd506bull,
  0x854997ba2f81e701ull, 0xc7b97651866bd192ull, 0xa8546d7c558a27ull, 0x4258b586d5bfbcb4ull,
  0x5e1c3d753d46d260ull, 0x1cecdc9e94ace4f3ull, 0xdbfdfea26e92bf46ull, 0x990d1f49c77889d5ull,
  0x172f5b3033043ebfull, 0x55dfbadb9aee082cull, 0x92ce98e760d05399ull, 0xd03e790cc93a650aull,
  0xaa478900b1228e31ull, 0xe8b768eb18c8b8a2ull, 0x2fa64ad7e2f6e317ull, 0x6d56ab3c4b1cd584ull,
  0xe374ef45bf6062eeull, 0xa1840eae168a547dull, 0x66952c92ecb40fc8ull, 0x2465cd79455e395bull,
  0x3821458aada7578full, 0x7ad1a461044d611cull, 0xbdc0865dfe733aa9ull, 0xff3067b657990c3aull,
  0x711223cfa3e5bb50ull, 0x33e2c2240a0f8dc3ull, 0xf4f3e018f031d676ull, 0xb60301f359dbe0e5ull,
  0xda050215ea6c212full, 0x98f5e3fe438617bcull, 0x5fe4c1c2b9b84c09ull, 0x1d14202910527a9aull,
  0x93366450e42ecdf0ull, 0xd1c685bb4dc4fb63ull, 0x16d7a787b7faa0d6ull, 0x5427466c1e109645ull,
  0x4863ce9ff6e9f891ull, 0xa932f745f03ce02ull, 0xcd820d48a53d95b7ull, 0x8f72eca30cd7a324ull,
  0x150a8daf8ab144eull, 0x43a04931514122ddull, 0x84b16b0dab7f7968ull, 0xc6418ae602954ffbull,
  0xbc387aea7a8da4c0ull, 0xfec89b01d3679253ull, 0x39d9b93d2959c9e6ull, 0x7b2958d680b3ff75ull,
  0xf50b1caf74cf481full, 0xb7fbfd44dd257e8cull, 0x70eadf78271b2539ull, 0x321a3e938ef113aaull,
  0x2e5eb66066087d7eull, 0x6cae578bcfe24bedull, 0xabbf75b735dc1058ull, 0xe94f945c9c3626cbull,
  0x676dd025684a91a1ull, 0x259d31cec1a0a732ull, 0xe28c13f23b9efc87ull, 0xa07cf2199274ca14ull,
  0x167ff3eacbaf2af1ull, 0x548f120162451c62ull, 0x939e303d987b47d7ull, 0xd16ed1d631917144ull,
  0x5f4c95afc5edc62eull, 0x1dbc74446c07f0bdull, 0xdaad56789639ab08ull, 0x985db7933fd39d9bull,
  0x84193f60d72af34full, 0xc6e9de8b7ec0c5dcull, 0x1f8fcb784fe9e69ull, 0x43081d5c2d14a8faull,
  0xcd2a5925d9681f90ull, 0x8fdab8ce70822903ull, 0x48cb9af28abc72b6ull, 0xa3b7b1923564425ull,
  0x70428b155b4eaf1eull, 0x32b26afef2a4998dull, 0xf5a348c2089ac238ull, 0xb753a929a170f4abull,
  0x3971ed50550c43c1ull, 0x7b810cbbfce67552ull, 0xbc902e8706d82ee7ull, 0xfe60cf6caf321874ull,
  0xe224479f47cb76a0ull, 0xa0d4a674ee214033ull, 0x67c58448141f1b86ull, 0x253565a3bdf52d15ull,
  0xab1721da49899a7full, 0xe9e7c031e063acecull, 0x2ef6e20d1a5df759ull, 0x6c0603e6b3b7c1caull,
  0xf6fae5c07d3274cdull, 0xb40a042bd4d8425eull, 0x731b26172ee619ebull, 0x31ebc7fc870c2f78ull,
  0xbfc9838573709812ull, 0xfd39626eda9aae81ull, 0x3a28405220a4f534ull, 0x78d8a1b9894ec3a7ull,
  0x649c294a61b7ad73ull, 0x266cc8a1c85d9be0ull, 0xe17dea9d3263c055ull, 0xa38d0b769b89f6c6ull,
  0x2daf4f0f6ff541acull, 0x6f5faee4c61f773full, 0xa84e8cd83c212c8aull, 0xeabe6d3395cb1a19ull,
  0x90c79d3fedd3f122ull, 0xd2377cd44439c7b1ull, 0x15265ee8be079c04ull, 0x57d6bf0317edaa97ull,
  0xd9f4fb7ae3911dfdull, 0x9b041a914a7b2b6eull, 0x5c1538adb04570dbull, 0x1ee5d94619af4648ull,
  0x2a151b5f156289cull, 0x4051b05e58bc1e0full, 0x87409262a28245baull, 0xc5b073890b687329ull,
  0x4b9237f0ff14c443ull, 0x962d61b56fef2d0ull, 0xce73f427acc0a965ull, 0x8c8315cc052a9ff6ull,
  0x3a80143f5cf17f13ull, 0x7870f5d4f51b4980ull, 0xbf61d7e80f251235ull, 0xfd913603a6cf24a6ull,
  0x73b3727a52b393ccull, 0x31439391fb59a55full, 0xf652b1ad0167feeaull, 0xb4a25046a88dc879ull,
  0xa8e6d8b54074a6adull, 0xea16395ee99e903eull, 0x2d071b6213a0cb8bull, 0x6ff7fa89ba4afd18ull,
  0xe1d5bef04e364a72ull, 0xa3255f1be7dc7ce1ull, 0x64347d271de22754ull, 0x26c49cccb40811c7ull,
  0x5cbd6cc0cc10fafcull, 0x1e4d8d2b65facc6full, 0xd95caf179fc497daull, 0x9bac4efc362ea149ull,
  0x158e0a85c2521623ull, 0x577eeb6e6bb820b0ull, 0x906fc95291867b05ull, 0xd29f28b9386c4d96ull,
  0xcedba04ad0952342ull, 0x8c2b41a1797f15d1ull, 0x4b3a639d83414e64ull, 0x9ca82762aab78f7ull,
  0x87e8c60fded7cf9dull, 0xc51827e4773df90eull, 0x20905d88d03a2bbull, 0x40f9e43324e99428ull,
  0x2cffe7d5975e55e2ull, 0x6e0f063e3eb46371ull, 0xa91e2402c48a38c4ull, 0xebeec5e96d600e57ull,
  0x65cc8190991cb93dull, 0x273c607b30f68faeull, 0xe02d4247cac8d41bull, 0xa2dda3ac6322e288ull,
  0xbe992b5f8bdb8c5cull, 0xfc69cab42231bacfull, 0x3b78e888d80fe17aull, 0x7988096371e5d7e9ull,
  0xf7aa4d1a85996083ull, 0xb55aacf12c735610ull, 0x724b8ecdd64d0da5ull, 0x30bb6f267fa73b36ull,
  0x4ac29f2a07bfd00dull, 0x8327ec1ae55e69eull, 0xcf235cfd546bbd2bull, 0x8dd3bd16fd818bb8ull,
  0x3f1f96f09fd3cd2ull, 0x41011884a0170a41ull, 0x86103ab85a2951f4ull, 0xc4e0db53f3c36767ull,
  0xd8a453a01b3a09b3ull, 0x9a54b24bb2d03f20ull, 0x5d45907748ee6495ull, 0x1fb5719ce1045206ull,
  0x919735e51578e56cull, 0xd367d40ebc92d3ffull, 0x1476f63246ac884aull, 0x568617d9ef46bed9ull,
  0xe085162ab69d5e3cull, 0xa275f7c11f7768afull, 0x6564d5fde549331aull, 0x279434164ca30589ull,
  0xa9b6706fb8dfb2e3ull, 0xeb46918411358470ull, 0x2c57b3b8eb0bdfc5ull, 0x6ea7525342e1e956ull,
  0x72e3daa0aa188782ull, 0x30133b4b03f2b111ull, 0xf7021977f9cceaa4ull, 0xb5f2f89c5026dc37ull,
  0x3bd0bce5a45a6b5dull, 0x79205d0e0db05dceull, 0xbe317f32f78e067bull, 0xfcc19ed95e6430e8ull,
  0x86b86ed5267cdbd3ull, 0xc4488f3e8f96ed40ull, 0x359ad0275a8b6f5ull, 0x41a94ce9dc428066ull,
  0xcf8b0890283e370cull, 0x8d7be97b81d4019full, 0x4a6acb477bea5a2aull, 0x89a2aacd2006cb9ull,
  0x14dea25f3af9026dull, 0x562e43b4931334feull, 0x913f6188692d6f4bull, 0xd3cf8063c0c759d8ull,
  0x5dedc41a34bbeeb2ull, 0x1f1d25f19d51d821ull, 0xd80c07cd676f8394ull, 0x9afce626ce85b507ull
};
#endif

#if _DICT_CRC == 32
static const uintNN_t DictionaryCRCTable[256] = {
  0x0ul, 0x77073096ul, 0xee0e612cul, 0x990951baul, 0x76dc419ul, 0x706af48ful, 0xe963a535ul, 0x9e6495a3ul,
  0xedb8832ul, 0x79dcb8a4ul, 0xe0d5e91eul, 0x97d2d988ul, 0x9b64c2bul, 0x7eb17cbdul, 0xe7b82d07ul, 0x90bf1d91ul,
  0x1db71064ul, 0x6ab020f2ul, 0xf3b97148ul, 0x84be41deul, 0x1adad47dul, 0x6ddde4ebul, 0xf4d4b551ul, 0x83d385c7ul,
  0x136c9856ul, 0x646ba8c0ul, 0xfd62f97aul, 0x8a65c9ecul, 0x14015c4ful, 0x63066cd9ul, 0xfa0f3d63ul, 0x8d080df5ul,
  0x3b6e20c8ul, 0x4c69105eul, 0xd56041e4ul, 0xa2677172ul, 0x3c03e4d1ul, 0x4b04d447ul, 0xd20d85fdul, 0xa50ab56bul,
  0x35b5a8faul, 0x42b2986cul, 0xdbbbc9d6ul, 0xacbcf940ul, 0x32d86ce3ul, 0x45df5c75ul, 0xdcd60dcful, 0xabd13d59ul,
  0x26d930acul, 0x51de003aul, 0xc8d75180ul, 0xbfd06116ul, 0x21b4f4b5ul, 0x56b3c423ul, 0xcfba9599ul, 0xb8bda50ful,
  0x2802b89eul, 0x5f058808ul, 0xc60cd9b2ul, 0xb10be924ul, 0x2f6f7c87ul, 0x58684c11ul, 0xc1611dabul, 0xb6662d3dul,
  0x76dc4190ul, 0x1db7106ul, 0x98d220bcul, 0xefd5102aul, 0x71b18589ul, 0x6b6b51ful, 0x9fbfe4a5ul, 0xe8b8d433ul,
  0x7807c9a2ul, 0xf00f934ul, 0x9609a88eul, 0xe10e9818ul, 0x7f6a0dbbul, 0x86d3d2dul, 0x91646c97ul, 0xe6635c01ul,
  0x6b6b51f4ul, 0x1c6c6162ul, 0x856530d8ul, 0xf262004eul, 0x6c0695edul, 0x1b01a57bul, 0x8208f4c1ul, 0xf50fc457ul,
  0x65b0d9c6ul, 0x12b7e950ul, 0x8bbeb8eaul, 0xfcb9887cul, 0x62dd1ddful, 0x15da2d49ul, 0x8cd37cf3ul, 0xfbd44c65ul,
  0x4db26158ul, 0x3ab551ceul, 0xa3bc0074ul, 0xd4bb30e2ul, 0x4adfa541ul, 0x3dd895d7ul, 0xa4d1c46dul, 0xd3d6f4fbul,
  0x4369e96aul, 0x346ed9fcul, 0xad678846ul, 0xda60b8d0ul, 0x44042d73ul, 0x33031de5ul, 0xaa0a4c5ful, 0xdd0d7cc9ul,
  0x5005713cul, 0x270241aaul, 0xbe0b1010ul, 0xc90c2086ul, 0x5768b525ul, 0x206f85b3ul, 0xb966d409ul, 0xce61e49ful,
  0x5edef90eul, 0x29d9c998ul, 0xb0d09822ul, 0xc7d7a8b4ul, 0x59b33d17ul, 0x2eb40d81ul, 0xb7bd5c3bul, 0xc0ba6cadul,
  0xedb88320ul, 0x9abfb3b6ul, 0x3b6e20cul, 0x74b1d29aul, 0xead54739ul, 0x9dd277aful, 0x4db2615ul, 0x73dc1683ul,
  0xe3630b12ul, 0x94643b84ul, 0xd6d6a3eul, 0x7a6a5aa8ul, 0xe40ecf0bul, 0x9309ff9dul, 0xa00ae27ul, 0x7d079eb1ul,
  0xf00f9344ul, 0x8708a3d2ul, 0x1e01f268ul, 0x6906c2feul, 0xf762575dul, 0x806567cbul, 0x196c3671ul, 0x6e6b06e7ul,
  0xfed41b76ul, 0x89d32be0ul, 0x10da7a5aul, 0x67dd4accul, 0xf9b9df6ful, 0x8ebeeff9ul, 0x17b7be43ul, 0x60b08ed5ul,
  0xd6d6a3e8ul, 0xa1d1937eul, 0x38d8c2c4ul, 0x4fdff252ul, 0xd1bb67f1ul, 0xa6bc5767ul, 0x3fb506ddul, 0x48b2364bul,
  0xd80d2bdaul, 0xaf0a1b4cul, 0x36034af6ul, 0x41047a60ul, 0xdf60efc3ul, 0xa867df55ul, 0x316e8eeful, 0x4669be79ul,
  0xcb61b38cul, 0xbc66831aul, 0x256fd2a0ul, 0x5268e236ul, 0xcc0c7795ul, 0xbb0b4703ul, 0x220216b9ul, 0x5505262ful,
  0xc5ba3bbeul, 0xb2bd0b28ul, 0x2bb45a92ul, 0x5cb36a04ul, 0xc2d7ffa7ul, 0xb5d0cf31ul, 0x2cd99e8bul, 0x5bdeae1dul,
  0x9b64c2b0ul, 0xec63f226ul, 0x756aa39cul, 0x26d930aul, 0x9c0906a9ul, 0xeb0e363ful, 0x72076785ul, 0x5005713ul,
  0x95bf4a82ul, 0xe2b87a14ul, 0x7bb12baeul, 0xcb61b38ul, 0x92d28e9bul, 0xe5d5be0dul, 0x7cdcefb7ul, 0xbdbdf21ul,
  0x86d3d2d4ul, 0xf1d4e242ul, 0x68ddb3f8ul, 0x1fda836eul, 0x81be16cdul, 0xf6b9265bul, 0x6fb077e1ul, 0x18b74777ul,
  0x88085ae6ul, 0xff0f6a70ul, 0x66063bcaul, 0x11010b5cul, 0x8f659efful, 0xf862ae69ul, 0x616bffd3ul, 0x166ccf45ul,
  0xa00ae278ul, 0xd70dd2eeul, 0x4e048354ul, 0x3903b3c2ul, 0xa7672661ul, 0xd06016f7ul, 0x4969474dul, 0x3e6e77dbul,
  0xaed16a4aul, 0xd9d65adcul, 0x40df0b66ul, 0x37d83bf0ul, 0xa9bcae53ul, 0xdebb9ec5ul, 0x47b2cf7ful, 0x30b5ffe9ul,
  0xbdbdf21cul, 0xcabac28aul, 0x53b39330ul, 0x24b4a3a6ul, 0xbad03605ul, 0xcdd70693ul, 0x54de5729ul, 0x23d967bful,
  0xb3667a2eul, 0xc4614ab8ul, 0x5d681b02ul, 0x2a6f2b94ul, 0xb40bbe37ul, 0xc30c8ea1ul, 0x5a05df1bul, 0x2d02ef8dul
};
#endif

#if _DICT_CRC == 16
static const uintNN_t DictionaryCRCTable[256] = {
  0x0u, 0xc0c1u, 0xc181u, 0x140u, 0xc301u, 0x3c0u, 0x280u, 0xc241u, 0xc601u, 0x6c0u, 0x780u, 0xc741u,
  0x500u, 0xc5c1u, 0xc481u, 0x440u, 0xcc01u, 0xcc0u, 0xd80u, 0xcd41u, 0xf00u, 0xcfc1u, 0xce81u, 0xe40u,
  0xa00u, 0xcac1u, 0xcb81u, 0xb40u, 0xc901u, 0x9c0u, 0x880u, 0xc841u, 0xd801u, 0x18c0u, 0x1980u, 0xd941u,
  0x1b00u, 0xdbc1u, 0xda81u, 0x1a40u, 0x1e00u, 0xdec1u, 0xdf81u, 0x1f40u, 0xdd01u, 0x1dc0u, 0x1c80u, 0xdc41u,
  0x1400u, 0xd4c1u, 0xd581u, 0x1540u, 0xd701u, 0x17c0u, 0x1680u, 0xd641u, 0xd201u, 0x12c0u, 0x1380u, 0xd341u,
  0x1100u, 0xd1c1u, 0xd081u, 0x1040u, 0xf001u, 0x30c0u, 0x3180u, 0xf141u, 0x3300u, 0xf3c1u, 0xf281u, 0x3240u,
  0x3600u, 0xf6c1u, 0xf781u, 0x3740u, 0xf501u, 0x35c0u, 0x3480u, 0xf441u, 0x3c00u, 0xfcc1u, 0xfd81u, 0x3d40u,
  0xff01u, 0x3fc0u, 0x3e80u, 0xfe41u, 0xfa01u, 0x3ac0u, 0x3b80u, 0xfb41u, 0x3900u, 0xf9c1u, 0xf881u, 0x3840u,
  0x2800u, 0xe8c1u, 0xe981u, 0x2940u, 0xeb01u, 0x2bc0u, 0x2a80u, 0xea41u, 0xee01u, 0x2ec0u, 0x2f80u, 0xef41u,
  0x2d00u, 0xedc1u, 0xec81u, 0x2c40u, 0xe401u, 0x24c0u, 0x2580u, 0xe541u, 0x2700u, 0xe7c1u, 0xe681u, 0x2640u,
  0x2200u, 0xe2c1u, 0xe381u, 0x2340u, 0xe101u, 0x21c0u, 0x2080u, 0xe041u, 0xa001u, 0x60c0u, 0x6180u, 0xa141u,
  0x6300u, 0xa3c1u, 0xa281u, 0x6240u, 0x6600u, 0xa6c1u, 0xa781u, 0x6740u, 0xa501u, 0x65c0u, 0x6480u, 0xa441u,
  0x6c00u, 0xacc1u, 0xad81u, 0x6d40u, 0xaf01u, 0x6fc0u, 0x6e80u, 0xae41u, 0xaa01u, 0x6ac0u, 0x6b80u, 0xab41u,
  0x6900u, 0xa9c1u, 0xa881u, 0x6840u, 0x7800u, 0xb8c1u, 0xb981u, 0x7940u, 0xbb01u, 0x7bc0u, 0x7a80u, 0xba41u,
  0xbe01u, 0x7ec0u, 0x7f80u, 0xbf41u, 0x7d00u, 0xbdc1u, 0xbc81u, 0x7c40u, 0xb401u, 0x74c0u, 0x7580u, 0xb541u,
  0x7700u, 0xb7c1u, 0xb681u, 0x7640u, 0x7200u, 0xb2c1u, 0xb381u, 0x7340u, 0xb101u, 0x71c0u, 0x7080u, 0xb041u,
  0x5000u, 0x90c1u, 0x9181u, 0x5140u, 0x9301u, 0x53c0u, 0x5280u, 0x9241u, 0x9601u, 0x56c0u, 0x5780u, 0x9741u,
  0x5500u, 0x95c1u, 0x9481u, 0x5440u, 0x9c01u, 0x5cc0u, 0x5d80u, 0x9d41u, 0x5f00u, 0x9fc1u, 0x9e81u, 0x5e40u,
  0x5a00u, 0x9ac1u, 0x9b81u, 0x5b40u, 0x9901u, 0x59c0u, 0x5880u, 0x9841u, 0x8801u, 0x48c0u, 0x4980u, 0x8941u,
  0x4b00u, 0x8bc1u, 0x8a81u, 0x4a40u, 0x4e00u, 0x8ec1u, 0x8f81u, 0x4f40u, 0x8d01u, 0x4dc0u, 0x4c80u, 0x8c41u,
  0x4400u, 0x84c1u, 0x8581u, 0x4540u, 0x8701u, 0x47c0u, 0x4680u, 0x8641u, 0x8201u, 0x42c0u, 0x4380u, 0x8341u,
  0x4100u, 0x81c1u, 0x8081u, 0x4040u
};
#endif

class Dictionary {
  public:
    Dictionary(size_t init_size = 10);
    ~Dictionary();

    int8_t insert(String keystr, String valstr);
    int8_t insert(const char* keystr, const char* valstr);
    String search(String keystr);
    String search(const char* keystr);
    void destroy();
    int8_t remove(String keystr);
    size_t size();
    
    String key(unsigned int i) {
      if (Q) {
        node* p = (*Q)[i];
        if (p) return String(p->keystr);
      }
      return String();
    }
    String value(unsigned int i) {
      if (Q) {
        node* p = (*Q)[i];
        if (p) return String(p->valstr);
      }
      return String();
    }
    String operator [] (String keystr) {
      return search(keystr);
    }
    String operator [] (unsigned int i) {
      return value(i);
    }
    int8_t operator () (String keystr, String valstr) {
      return insert(keystr, valstr);
    }

    bool operator () (String keystr);

    String operator () (unsigned int i) {
      return key(i);
    }
    bool operator == (Dictionary& b);
    inline bool operator != (Dictionary& b) {
      return (!(*this == b));
    }
    inline const size_t count() {

      return ( Q ? Q->count() : 0);
    }

#ifdef _LIBDEBUG_
    void printNode(node* root);
    void printDictionary(node* root);
    void printDictionary() {
      Serial.printf("\nDictionary::printDictionary:\n");
      printDictionary(iRoot);
      Serial.println();
    };
    void printArray() {
      Q->printArray();
    };
#endif
  private:
    void     destroy_tree(node* leaf);
    int8_t   insert(uintNN_t key, const char* keystr, const char* valstr, node* leaf);
    node*    search(uintNN_t key, node* leaf, const char* keystr);
    uintNN_t crc(const void* data, size_t n_bytes);

    node*    deleteNode(node* root, uintNN_t key, const char* keystr);
    node*    minValueNode(node* n);

    node*               iRoot;
    NodeArray*          Q;
    size_t              initSize;
};

Dictionary::Dictionary(size_t init_size) {
  iRoot = NULL;

  // This is unlikely to fail as practically no memory is allocated by the NodeArray
  // All memory allocation is delegated to the first append
  Q = new NodeArray(init_size);
  initSize = init_size;
}


Dictionary::~Dictionary() {
  destroy();
  delete Q;
}

void Dictionary::destroy_tree(node* leaf) {
  if (leaf != NULL) {
    destroy_tree(leaf->left);
    destroy_tree(leaf->right);
    delete leaf; // node destructor takes care of the key and value strings
  }
}

int8_t Dictionary::insert(uintNN_t key, const char* keystr, const char* valstr, node* leaf) {
  if (key < leaf->key) {
    if (leaf->left != NULL)
      return insert(key, keystr, valstr, leaf->left);
    else {
      int8_t rc;

      leaf->left = new node;
      if (!leaf->left) return DICTIONARY_MEM;
      rc = leaf->left->create(key, keystr, valstr, NULL, NULL);
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
  else if (key > leaf->key) {
    if (leaf->right != NULL)
      return insert(key, keystr, valstr, leaf->right);
    else {
      int8_t rc;

      leaf->right = new node;
      if (!leaf->right) return DICTIONARY_MEM;
      rc = leaf->right->create(key, keystr, valstr, NULL, NULL);
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
  else if (key == leaf->key) {
    if (strcmp(leaf->keystr, keystr) == 0) {
      if (leaf->updateValue(valstr) != NODEARRAY_OK) return DICTIONARY_MEM;
    }
    else {
      if (leaf->right != NULL)
        return insert(key, keystr, valstr, leaf->right);
      else {
        int8_t rc;

        leaf->right = new node;
        if (!leaf->right) return DICTIONARY_MEM;
        rc = leaf->right->create(key, keystr, valstr, NULL, NULL);
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
  return DICTIONARY_OK;
}

bool Dictionary::operator () (String keystr) {
  if ( keystr.length() > _DICT_KEYLEN ) return false;

  uintNN_t key = crc(keystr.c_str(), keystr.length());

  node* p = search(key, iRoot, keystr.c_str());
  if (p) return true;
  return false;
}


node* Dictionary::search(uintNN_t key, node* leaf, const char* keystr) {
  if (leaf != NULL) {
    if ( key == leaf->key && strcmp(leaf->keystr, keystr) == 0 )
      return leaf;
    if ( key < leaf->key )
      return search( key, leaf->left, keystr );
    else
      return search( key, leaf->right, keystr );
  }
  else return NULL;
}


int8_t Dictionary::insert(String keystr, String valstr) {
  return insert(keystr.c_str(), valstr.c_str());
}

int8_t Dictionary::insert(const char* keystr, const char* valstr) {
  // TODO: decide if to check for length here
  size_t l = strnlen(keystr, _DICT_KEYLEN + 1);

  if ( l > _DICT_KEYLEN ) return DICTIONARY_ERR;
  if ( strnlen(valstr, _DICT_VALLEN + 1) > _DICT_VALLEN ) return DICTIONARY_ERR;

  uintNN_t key = crc(keystr, l);

  if (iRoot != NULL)
    return insert(key, keystr, valstr, iRoot);
  else {
    int8_t rc;

    iRoot = new node;
    if (!iRoot) return DICTIONARY_MEM;
    rc = iRoot->create(key, keystr, valstr, NULL, NULL);

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


String Dictionary::search(String keystr) {
  return search(keystr.c_str());
}

String Dictionary::search(const char* keystr) {
  size_t l = strnlen(keystr, _DICT_KEYLEN + 1);

  if (l <= _DICT_KEYLEN) {
    uintNN_t key = crc(keystr, l);

    node* p = search(key, iRoot, keystr);
    if (p) return String(p->valstr);
  }
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
    sz += key(i).length() + 1;
    sz += value(i).length() + 1;
    sz += sizeof(node);  // to account for size of the node itself
  }
  return sz;
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


int8_t Dictionary::remove(String keystr) {
#ifdef _LIBDEBUG_
  Serial.printf("Dictionary::remove: %s\n", keystr.c_str());
#endif
  if (keystr.length() > _DICT_KEYLEN) return DICTIONARY_ERR;

  uintNN_t key = crc(keystr.c_str(), keystr.length());
  node* p = search(key, iRoot, keystr.c_str());

  if (p) {
#ifdef _LIBDEBUG_
    Serial.printf("Found key to delete int: %u\n", p->key);
    Serial.printf("Found key to delete ptr: %u\n", (uint32_t)p);
    Serial.printf("Found key to delete str: %s\n", keystr.c_str());
#endif
    iRoot = deleteNode(iRoot, p->key, keystr.c_str());
  }
  return DICTIONARY_OK;
}

node* Dictionary::deleteNode(node* root, uintNN_t key, const char* keystr) {
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
    if ( strcmp(root->keystr, keystr) == 0 ) {
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
      root->ksize = temp->ksize;
      root->vsize = temp->vsize;

      // Delete the inorder successor
      root->right = deleteNode(root->right, temp->key, temp->keystr);
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

void Dictionary::printNode(node* root) {
  if (root != NULL) {
    Serial.printf("%u: (%u:%s) [l:%u, r:%u]\n", (uint32_t)root, root->key, root->keystr, (uint32_t)root->left, (uint32_t)root->right);
  }
  else {
    Serial.println("NULL:");
  }
}
#endif

#if _DICT_CRC == 64
uint64_t Dictionary::crc(const void* data, size_t n_bytes) {
  uint64_t crc = 0x0000000000000000ull;
  uint8_t* p = (uint8_t*)data;

  for (int a = 0; a < n_bytes; a++, p++) {
    crc = (crc << 8) ^ DictionaryCRCTable[((crc >> 56) ^ (uint64_t) * p) & 0x00000000000000FFull];
  }
  return crc;
}
#endif

#if _DICT_CRC == 32
uint32_t Dictionary::crc(const void* data, size_t n_bytes) {
  uint32_t crc = 0xFFFFFFFFul;
  uint8_t* p = (uint8_t*)data;

  for (int a = 0; a < n_bytes; a++, p++) {
    crc = (crc >> 8) ^ DictionaryCRCTable[(crc ^ (uint32_t) * p) & 0x000000FFul];
  }
  return (crc ^ 0xFFFFFFFFul);
}
#endif

#if _DICT_CRC == 16
uint16_t Dictionary::crc(const void* data, size_t n_bytes) {
  uint16_t crc = 0;
  uint8_t* p = (uint8_t*)data;

  for (size_t a = 0; a < n_bytes; a++, p++) {
    crc = (crc >> 8) ^ DictionaryCRCTable[(crc ^ (uint16_t) * p) & 0x00FF];
  }
  return crc;
}
#endif

#endif // #define _DICTIONARY_H_






