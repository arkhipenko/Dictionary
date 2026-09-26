// test-dictionary-compress.cpp - the built-in value codec (_DICT_COMPRESS).
//
// Checks the token table rules the encoder relies on, that every value reads back
// exactly (including incompressible and non-ASCII input), that a value is packed
// only when that makes it smaller, and that keys are never compressed. Also built
// with the 3.x flag _DICT_COMPRESS_SMAZ, which 4.0 maps to _DICT_COMPRESS.
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"

#include <random>
#include <string>
#include <utility>

#ifndef _DICT_COMPRESS
#error "This suite must be built with _DICT_COMPRESS (or a 3.x compression flag)"
#endif

class DictionaryCompress : public ::testing::Test {};

// ---- token table ------------------------------------------------------------------
TEST_F(DictionaryCompress, TableFollowsTheEncoderRules) {
    const unsigned count = DictCodecTable::COUNT;
    ASSERT_GE(count, 1u);
    ASSERT_LE(count, 127u);                               // codes 0x80..0xFE
    for (unsigned t = 0; t < count; t++) {
        unsigned off = DictCodecTable::OFFS[t], end = DictCodecTable::OFFS[t + 1];
        ASSERT_GE(end - off, 2u) << "token " << t << " must be at least 2 bytes";
        for (unsigned k = off; k < end; k++)
            ASSERT_LT((uint8_t)DictCodecTable::DATA[k], 0x80u) << "token " << t;
        if (t) {
            uint8_t prevFirst = (uint8_t)DictCodecTable::DATA[DictCodecTable::OFFS[t - 1]];
            uint8_t first = (uint8_t)DictCodecTable::DATA[off];
            ASSERT_LE(prevFirst, first) << "tokens must be sorted by first byte";
            if (prevFirst == first) {
                ASSERT_GE(DictCodecTable::OFFS[t] - DictCodecTable::OFFS[t - 1], end - off)
                    << "longest token first within one first byte";
            }
        }
    }
}

// ---- round trips --------------------------------------------------------------------
TEST_F(DictionaryCompress, TypicalValuesRoundTripAndShrink) {
    Dictionary d;
    const char* vals[] = {
        "https://api.example.com/api/v1/the/resource",
        "the quick brown fox jumps over the lazy dog",
        "home/livingroom/temperature",
        "192.168.1.100",
        "mqtt://broker.local:1883",
    };
    size_t raw = 0;
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        ASSERT_EQ(d.set(("k" + std::to_string(i)).c_str(), vals[i]), DICTIONARY_OK);
        raw += strlen(vals[i]) + 1;
    }
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++)
        EXPECT_STREQ(d.getString(("k" + std::to_string(i)).c_str()).c_str(), vals[i]);
    Dictionary plain;                                     // same keys, values that cannot shrink
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++)
        plain.set(("k" + std::to_string(i)).c_str(), std::string(strlen(vals[i]), '\x7F').c_str());
    EXPECT_LT(d.size(), plain.size());                    // compressed storage is smaller
}

TEST_F(DictionaryCompress, IncompressibleValuesAreStoredPlain) {
    Dictionary d;
    std::string hi(_DICT_VALLEN, '\xE9');                 // every byte would need an escape
    ASSERT_EQ(d.set("hi", hi.c_str()), DICTIONARY_OK);    // 3.x: stored empty or rejected
    EXPECT_EQ(std::string(d.getString("hi").c_str()), hi);
    EXPECT_NE(d.peek("hi"), (const char*)NULL);           // not packed: zero-copy view exists
}

TEST_F(DictionaryCompress, RandomBytesRoundTrip) {
    std::mt19937 rng(7);
    for (int i = 0; i < 500; i++) {
        size_t len = rng() % (_DICT_VALLEN + 1);
        std::string v(len, ' ');
        for (auto& c : v) {
            unsigned r = rng() % 4;
            c = r == 0 ? (char)(1 + rng() % 255)              // any non-NUL byte
              : r == 1 ? " the ab/"[rng() % 8]                 // token-rich
              : (char)('a' + rng() % 26);
        }
        Dictionary d;
        ASSERT_EQ(d.set("k", v.c_str()), DICTIONARY_OK) << i;
        ASSERT_EQ(std::string(d.getString("k").c_str()), v) << i;
        ASSERT_EQ(std::string(d.value(0).c_str()), v) << i;
    }
}

TEST_F(DictionaryCompress, JsonDecodesPackedValues) {
    Dictionary d;
    d.set("a", "the answer is \"no\"");
    d.set("b", "https://the.server/path");
    EXPECT_STREQ(d.json().c_str(), "{\"a\":\"the answer is \\\"no\\\"\",\"b\":\"https://the.server/path\"}");
    EXPECT_EQ(d.jsize(), d.json().length() + 1);
    Dictionary e;
    ASSERT_EQ(e.jload(d.json()), DICTIONARY_OK);
    EXPECT_TRUE(e == d);
}

TEST_F(DictionaryCompress, UpdateBetweenPackedAndPlain) {
    Dictionary d;
    d.set("k", "the the the the");                        // packs
    d.set("k", "zqzqzq");                                 // cannot pack
    EXPECT_STREQ(d.getString("k").c_str(), "zqzqzq");
    d.set("k", "https://the.org/the");                    // packs again
    EXPECT_STREQ(d.getString("k").c_str(), "https://the.org/the");
    EXPECT_EQ(d.count(), 1u);
}

TEST_F(DictionaryCompress, TextConversionsReadPackedValues) {
    Dictionary d;
    d.set("n", "1234567890");                             // digits: plain or packed, must parse
    d.set("t", "true");
#ifndef _DICT_STRICT_GET
    EXPECT_EQ(d.getInt("n"), 1234567890);
    EXPECT_TRUE(d.getBool("t"));
#endif
}

TEST_F(DictionaryCompress, KeysAreNeverCompressed) {
    Dictionary d;
    d.set("the the the", "v");
    EXPECT_STREQ(d.keyAt(0), "the the the");              // zero-copy key access still works
    EXPECT_TRUE(d.has("the the the"));
}

TEST_F(DictionaryCompress, MergeAndMoveCopyPackedEntries) {
    Dictionary a;
    a.set("u", "https://the.example.org/the/path");
    Dictionary b;
    ASSERT_EQ(b.merge(a), DICTIONARY_OK);
    EXPECT_STREQ(b.getString("u").c_str(), "https://the.example.org/the/path");
    Dictionary c(std::move(b));
    EXPECT_TRUE(c == a);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
