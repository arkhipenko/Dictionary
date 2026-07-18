// test-dictionary-basic.cpp - core CRUD, positional access, operators, sizes.
// Compiled against the DEFAULT configuration (CRC32, KEYLEN 64, VALLEN 254,
// no compression, unpacked). Config variants reuse this same source with
// different -D defines (see tests/CMakeLists.txt).
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"

#include <string>
#include <vector>

class DictionaryBasic : public ::testing::Test {};

// ---- creation / empty state -------------------------------------------------
TEST_F(DictionaryBasic, CreatesEmpty) {
    Dictionary d;
    EXPECT_EQ(d.count(), 0u);
    EXPECT_EQ(d.size(), 0u);
}

TEST_F(DictionaryBasic, MissingKeyReturnsEmptyString) {
    Dictionary d;
    EXPECT_STREQ(d.search("nope").c_str(), "");
    EXPECT_STREQ(d["nope"].c_str(), "");
}

TEST_F(DictionaryBasic, ExistenceOperator) {
    Dictionary d;
    EXPECT_FALSE(d("k"));
    d.insert("k", "v");
    EXPECT_TRUE(d("k"));
    EXPECT_FALSE(d("other"));
}

// ---- insert / search --------------------------------------------------------
TEST_F(DictionaryBasic, InsertAndSearchSingle) {
    Dictionary d;
    ASSERT_EQ(d.insert("ssid", "devices"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 1u);
    EXPECT_STREQ(d.search("ssid").c_str(), "devices");
    EXPECT_STREQ(d["ssid"].c_str(), "devices");
}

TEST_F(DictionaryBasic, InsertMultipleAndLookup) {
    Dictionary d;
    d("ssid", "devices");
    d("pwd", "secret");
    d("port", "80");
    EXPECT_EQ(d.count(), 3u);
    EXPECT_STREQ(d["ssid"].c_str(), "devices");
    EXPECT_STREQ(d["pwd"].c_str(),  "secret");
    EXPECT_STREQ(d["port"].c_str(), "80");
}

TEST_F(DictionaryBasic, UpdateExistingValueKeepsCount) {
    Dictionary d;
    d("k", "old");
    EXPECT_STREQ(d["k"].c_str(), "old");
    d("k", "new");
    EXPECT_EQ(d.count(), 1u);
    EXPECT_STREQ(d["k"].c_str(), "new");
}

TEST_F(DictionaryBasic, UpdateGrowAndShrinkValue) {
    Dictionary d;
    d("k", "short");
    d("k", "a much longer value than before");   // forces realloc path
    EXPECT_STREQ(d["k"].c_str(), "a much longer value than before");
    d("k", "x");                                  // shrink, reuse buffer
    EXPECT_STREQ(d["k"].c_str(), "x");
    EXPECT_EQ(d.count(), 1u);
}

// ---- positional access (insertion order, before any remove) -----------------
TEST_F(DictionaryBasic, PositionalAccessInInsertionOrder) {
    Dictionary d;
    d("a", "1"); d("b", "2"); d("c", "3");
    EXPECT_STREQ(d(0).c_str(), "a");   // key by index
    EXPECT_STREQ(d(1).c_str(), "b");
    EXPECT_STREQ(d(2).c_str(), "c");
    EXPECT_STREQ(d[0].c_str(), "1");   // value by index
    EXPECT_STREQ(d[1].c_str(), "2");
    EXPECT_STREQ(d[2].c_str(), "3");
    EXPECT_STREQ(d.key(1).c_str(), "b");
    EXPECT_STREQ(d.value(2).c_str(), "3");
}

TEST_F(DictionaryBasic, PositionalOutOfBoundsReturnsEmpty) {
    Dictionary d;
    d("a", "1");
    EXPECT_STREQ(d(10).c_str(), "");
    EXPECT_STREQ(d[10].c_str(), "");
}

// ---- length / validation ----------------------------------------------------
TEST_F(DictionaryBasic, ZeroLengthKeyRejected) {
    Dictionary d;
    EXPECT_NE(d.insert("", "v"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 0u);
}

TEST_F(DictionaryBasic, OverlongKeyRejected) {
    Dictionary d;
    std::string longkey(_DICT_KEYLEN + 5, 'k');
    EXPECT_NE(d.insert(longkey.c_str(), "v"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 0u);
}

TEST_F(DictionaryBasic, MaxLengthKeyAndValueAccepted) {
    Dictionary d;
    std::string k(_DICT_KEYLEN, 'k');
    std::string v(_DICT_VALLEN, 'v');
    ASSERT_EQ(d.insert(k.c_str(), v.c_str()), DICTIONARY_OK);
    EXPECT_EQ(d.search(k.c_str()).length(), (unsigned)_DICT_VALLEN);
}

// ---- prefix-collision keys (same first CRC bytes, differ later) -------------
TEST_F(DictionaryBasic, PrefixCollisionKeysAreDistinct) {
    Dictionary d;
    d("prefix_A", "1");
    d("prefix_B", "2");
    d("prefix_C", "3");
    EXPECT_EQ(d.count(), 3u);
    EXPECT_STREQ(d["prefix_A"].c_str(), "1");
    EXPECT_STREQ(d["prefix_B"].c_str(), "2");
    EXPECT_STREQ(d["prefix_C"].c_str(), "3");
}

// ---- operators ==, !=, = ----------------------------------------------------
TEST_F(DictionaryBasic, EqualityOperators) {
    Dictionary a, b;
    a("x", "1"); a("y", "2");
    b("x", "1"); b("y", "2");
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    b("y", "different");
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST_F(DictionaryBasic, AssignmentCopiesContents) {
    Dictionary a;
    a("x", "1"); a("y", "2");
    Dictionary b;
    b = a;
    EXPECT_EQ(b.count(), 2u);
    EXPECT_STREQ(b["x"].c_str(), "1");
    EXPECT_STREQ(b["y"].c_str(), "2");
    EXPECT_TRUE(a == b);
}

TEST_F(DictionaryBasic, MergeCombinesDictionaries) {
    Dictionary a, b;
    a("x", "1");
    b("y", "2");
    ASSERT_EQ(a.merge(b), DICTIONARY_OK);
    EXPECT_EQ(a.count(), 2u);
    EXPECT_STREQ(a["x"].c_str(), "1");
    EXPECT_STREQ(a["y"].c_str(), "2");
}

// ---- sizes ------------------------------------------------------------------
TEST_F(DictionaryBasic, SizeAccountsForData) {
    Dictionary d;
    d("ab", "cde");   // 2 + 3 bytes of string data + node overhead
    EXPECT_GT(d.size(), 5u);
}

TEST_F(DictionaryBasic, EsizeCountsStringsPlusTerminators) {
    Dictionary d;
    d("ab", "cde");
    d("f", "gh");
    // (2+1)+(3+1) + (1+1)+(2+1) = 7 + 5 = 12
    EXPECT_EQ(d.esize(), 12u);
}

// ---- scale: sequential inserts (degenerate/unbalanced tree) -----------------
TEST_F(DictionaryBasic, ManySequentialInsertsAndLookups) {
    Dictionary d;
    const int N = 1000;
    for (int i = 0; i < N; i++) {
        std::string k = "key" + std::to_string(i);
        std::string v = "val" + std::to_string(i);
        ASSERT_EQ(d.insert(k.c_str(), v.c_str()), DICTIONARY_OK) << "insert " << i;
    }
    ASSERT_EQ(d.count(), (size_t)N);
    for (int i = 0; i < N; i++) {
        std::string k = "key" + std::to_string(i);
        std::string v = "val" + std::to_string(i);
        ASSERT_STREQ(d.search(k.c_str()).c_str(), v.c_str()) << "lookup " << i;
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
