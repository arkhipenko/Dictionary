// Implementation of the Dictionary data type
// Copyright (c) 2020-2026 Anatoli Arkhipenko
// Distributed under the BSD 3-Clause License. See LICENSE.txt.
//
// This translation unit compiles the Dictionary implementation for
// PlatformIO / non-Arduino-IDE builds. Define _DICT_HEADER_AND_CPP as a
// build flag and include <DictionaryDeclarations.h> in your code; the
// implementation is compiled here exactly once. Without the define
// (Arduino IDE style) this file is empty and you just #include <Dictionary.h>.

#ifdef _DICT_HEADER_AND_CPP
#include <Dictionary.h>
#endif
