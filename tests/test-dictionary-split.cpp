// test-dictionary-split.cpp - PlatformIO split-header mode.
//
// This translation unit includes ONLY DictionaryDeclarations.h, as user code does
// with -D_DICT_HEADER_AND_CPP; the implementation is linked from the bundled
// src/Dictionary.cpp (see tests/CMakeLists.txt). Calling every public method here
// turns a method that is declared but never emitted into a link error (v3.6.0 had
// one: remove(const String&) was declared inline).
#include <gtest/gtest.h>
#include "Arduino.h"
#include "DictionaryDeclarations.h"
#include "TestStream.h"

#include <string>
#include <utility>

#ifndef _DICT_HEADER_AND_CPP
#error "This suite must be built with -D_DICT_HEADER_AND_CPP"
#endif

TEST(DictionarySplit, EveryPublicMethodLinks) {
    Dictionary d(4);
    String ks("s"), vs("v");

    // setters
    EXPECT_EQ(d.set("a", "1"), DICTIONARY_OK);
    EXPECT_EQ(d.set(ks, vs), DICTIONARY_OK);
    EXPECT_EQ(d.set("fl", F("flash")), DICTIONARY_OK);
    EXPECT_EQ(d.set("b", true), DICTIONARY_OK);
    EXPECT_EQ(d.set("i", 5), DICTIONARY_OK);
    EXPECT_EQ(d.set("u", 5u), DICTIONARY_OK);
    EXPECT_EQ(d.set("ll", 9000000000LL), DICTIONARY_OK);
    EXPECT_EQ(d.set("ull", 5ULL), DICTIONARY_OK);
    EXPECT_EQ(d.set("f", 1.5f), DICTIONARY_OK);
    EXPECT_EQ(d.set("d", 2.5), DICTIONARY_OK);
    EXPECT_EQ(d.setNull("z"), DICTIONARY_OK);
    EXPECT_EQ(d.reserve(32), DICTIONARY_OK);

    // getters
    EXPECT_TRUE(d.has("a"));
    EXPECT_EQ(d.type("i"), DICT_INT);
    EXPECT_EQ(d.getInt("i"), 5);
    EXPECT_FLOAT_EQ(d.getFloat("f"), 1.5f);
    EXPECT_TRUE(d.getBool("b"));
    EXPECT_STREQ(d.getString(ks).c_str(), "v");
    char buf[8];
    EXPECT_EQ(d.getString("a", buf, sizeof(buf)), 1u);
    EXPECT_STREQ(d.peek("a"), "1");
    EXPECT_STREQ(d.keyAt(0), "a");
    EXPECT_EQ(d.typeAt(0), DICT_STR);
    EXPECT_STREQ(d.key(0).c_str(), "a");
    EXPECT_STREQ(d.value(0).c_str(), "1");
    EXPECT_STREQ(d["a"].c_str(), "1");
    EXPECT_GT(d.count(), 0u);
    EXPECT_GT(d.size(), 0u);
    EXPECT_GT(d.esize(), 0u);

    // JSON out and in
    String js = d.json();
    EXPECT_EQ(d.jsize(), js.length() + 1);
    TestPrint p;
    EXPECT_EQ(d.json(p), (size_t)js.length());
    Dictionary e;
    EXPECT_EQ(e.jload(js), DICTIONARY_OK);
    EXPECT_EQ(e.jload(js.c_str()), DICTIONARY_OK);
    TestStream stream("{\"x\":\"1\"}");
    Dictionary s;
    EXPECT_EQ(s.jload(stream, 1), DICTIONARY_OK);

    // copy, move, compare, merge
    Dictionary f;
    EXPECT_EQ(f.merge(d), DICTIONARY_OK);
    Dictionary g;
    g = d;
    Dictionary h(std::move(g));
    g = std::move(h);
    EXPECT_TRUE(g == d);
    EXPECT_FALSE(g != d);

    // 3.x wrappers (deprecated)
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    EXPECT_EQ(d.insert("w1", "x"), DICTIONARY_OK);
    EXPECT_EQ(d.insert(String("w2"), String("x")), DICTIONARY_OK);
    EXPECT_EQ(d.insert(String("w3"), (int32_t)3), DICTIONARY_OK);
    EXPECT_EQ(d.insert(String("w4"), 1.5f), DICTIONARY_OK);
    EXPECT_EQ(d.insert(String("w5"), 2.5), DICTIONARY_OK);
    EXPECT_EQ(d("w6", "x"), DICTIONARY_OK);
    EXPECT_EQ(d(String("w7"), String("x")), DICTIONARY_OK);
    EXPECT_EQ(d(String("w8"), (int32_t)8), DICTIONARY_OK);
    EXPECT_EQ(d(String("w9"), 1.5f), DICTIONARY_OK);
    EXPECT_EQ(d(String("w10"), 2.5), DICTIONARY_OK);
    EXPECT_STREQ(d.search("w1").c_str(), "x");
    EXPECT_TRUE(d("w1"));
    EXPECT_STREQ(d((size_t)0).c_str(), "a");
    EXPECT_STREQ(d[(size_t)0].c_str(), "1");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    // removal and destroy
    EXPECT_EQ(d.remove(ks), DICTIONARY_OK);
    EXPECT_EQ(d.remove("a"), DICTIONARY_OK);
    d.destroy();
    EXPECT_EQ(d.count(), 0u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
