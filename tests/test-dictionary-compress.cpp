// test-dictionary-compress.cpp - key/value compression round-trips.
//
// This one source is compiled twice, once with -D_DICT_COMPRESS_SHOCO and once
// with -D_DICT_COMPRESS_SMAZ (see tests/CMakeLists.txt). It only asserts on
// externally observable behavior (values go in and come back out unchanged), so
// it is agnostic to which compressor is active.
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"

#include <string>

#ifndef _DICT_COMPRESS
#error "This suite must be built with _DICT_COMPRESS_SHOCO or _DICT_COMPRESS_SMAZ"
#endif

class DictionaryCompress : public ::testing::Test {};

TEST_F(DictionaryCompress, RoundTripTypicalStrings) {
    Dictionary d;
    d("ssid", "my home network");
    d("url",  "http://ota.home.lan/firmware.bin");
    d("note", "the quick brown fox jumps over the lazy dog");
    EXPECT_STREQ(d["ssid"].c_str(), "my home network");
    EXPECT_STREQ(d["url"].c_str(),  "http://ota.home.lan/firmware.bin");
    EXPECT_STREQ(d["note"].c_str(), "the quick brown fox jumps over the lazy dog");
}

TEST_F(DictionaryCompress, UpdateCompressedValue) {
    Dictionary d;
    d("k", "initial english text value");
    d("k", "a different, somewhat longer english value");
    EXPECT_EQ(d.count(), 1u);
    EXPECT_STREQ(d["k"].c_str(), "a different, somewhat longer english value");
}

TEST_F(DictionaryCompress, JsonReturnsDecompressedValues) {
    Dictionary d;
    d("a", "hello world");
    d("b", "another value");
    // json() must round-trip through decompression to the original text.
    EXPECT_STREQ(d.json().c_str(), "{\"a\":\"hello world\",\"b\":\"another value\"}");
}

TEST_F(DictionaryCompress, DeleteFromCompressedDictionary) {
    Dictionary d;
    d("one", "first english value");
    d("two", "second english value");
    d("three", "third english value");
    ASSERT_EQ(d.remove("two"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_STREQ(d["one"].c_str(),   "first english value");
    EXPECT_STREQ(d["two"].c_str(),   "");
    EXPECT_STREQ(d["three"].c_str(), "third english value");
}

TEST_F(DictionaryCompress, ManyEntriesRoundTrip) {
    Dictionary d;
    const int N = 200;
    for (int i = 0; i < N; i++) {
        std::string k = "configuration_key_" + std::to_string(i);
        std::string v = "some english value number " + std::to_string(i);
        ASSERT_EQ(d.insert(k.c_str(), v.c_str()), DICTIONARY_OK);
    }
    for (int i = 0; i < N; i++) {
        std::string k = "configuration_key_" + std::to_string(i);
        std::string v = "some english value number " + std::to_string(i);
        ASSERT_STREQ(d.search(k.c_str()).c_str(), v.c_str());
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
