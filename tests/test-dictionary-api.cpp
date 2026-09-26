// test-dictionary-api.cpp - the 4.0 API: typed set/get, conversions, peek/has/type,
// positional access, order after remove, reserve, merge, equality, sizes, move.
// Built for the default configuration and for _DICT_STRICT_GET, _DICT_WIDE_NUMBERS
// and _DICT_COMPRESS (see tests/CMakeLists.txt); expectations switch on those flags.
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"
#include "TestStream.h"

#include <cmath>
#include <string>
#include <utility>

class DictionaryApi : public ::testing::Test {};

// ---- setters and types --------------------------------------------------------
TEST_F(DictionaryApi, SetStoresTypedValues) {
    Dictionary d;
    ASSERT_EQ(d.set("s", "text"), DICTIONARY_OK);
    ASSERT_EQ(d.set("i", 42), DICTIONARY_OK);
    ASSERT_EQ(d.set("f", 1.5f), DICTIONARY_OK);
    ASSERT_EQ(d.set("b", true), DICTIONARY_OK);
    ASSERT_EQ(d.setNull("n"), DICTIONARY_OK);
    EXPECT_EQ(d.type("s"), DICT_STR);
    EXPECT_EQ(d.type("i"), DICT_INT);
    EXPECT_EQ(d.type("f"), DICT_FLOAT);
    EXPECT_EQ(d.type("b"), DICT_BOOL);
    EXPECT_EQ(d.type("n"), DICT_NULL);
    EXPECT_EQ(d.type("missing"), DICT_NONE);
    EXPECT_EQ(d.count(), 5u);
}

TEST_F(DictionaryApi, EveryIntegerWidthCompiles) {
    Dictionary d;
    // The 3.x overload set made these ambiguous (unsigned, long, size_t).
    EXPECT_EQ(d.set("a", (signed char)-5), DICTIONARY_OK);
    EXPECT_EQ(d.set("b", (unsigned char)200), DICTIONARY_OK);
    EXPECT_EQ(d.set("c", (short)-300), DICTIONARY_OK);
    EXPECT_EQ(d.set("d", (unsigned short)60000), DICTIONARY_OK);
    EXPECT_EQ(d.set("e", 5u), DICTIONARY_OK);
    EXPECT_EQ(d.set("f", -7L), DICTIONARY_OK);
    EXPECT_EQ(d.set("g", 7UL), DICTIONARY_OK);
    EXPECT_EQ(d.set("h", d.count()), DICTIONARY_OK);          // size_t
    EXPECT_EQ(d.set("i", (int32_t)-8), DICTIONARY_OK);
    EXPECT_EQ(d.set("j", (uint16_t)9), DICTIONARY_OK);
    EXPECT_EQ(d.getInt("a"), -5);
    EXPECT_EQ(d.getInt("b"), 200);
    EXPECT_EQ(d.getInt("c"), -300);
    EXPECT_EQ(d.getInt("d"), 60000);
    EXPECT_EQ(d.getInt("e"), 5);
    EXPECT_EQ(d.getInt("f"), -7);
    EXPECT_EQ(d.getInt("g"), 7);
    EXPECT_EQ(d.getInt("h"), 7);
    EXPECT_EQ(d.type("b"), DICT_INT);
}

TEST_F(DictionaryApi, IntegersOutsideInt32AreNeverAltered) {
    Dictionary d;
    d.set("big", 9000000000LL);
    d.set("ubig", 18446744073709551615ULL);
    d.set("min32", -2147483647 - 1);
    EXPECT_EQ(d.getInt("min32"), -2147483647 - 1);
    EXPECT_STREQ(d.getString("ubig").c_str(), "18446744073709551615");
    EXPECT_STREQ(d.getString("big").c_str(), "9000000000");
#ifdef _DICT_WIDE_NUMBERS
    EXPECT_EQ(d.type("big"), DICT_INT64);
    EXPECT_EQ(d.getInt64("big"), 9000000000LL);
    EXPECT_EQ(d.type("ubig"), DICT_STR);        // above INT64_MAX: text
#else
    EXPECT_EQ(d.type("big"), DICT_STR);         // stored as decimal text
    EXPECT_EQ(d.type("ubig"), DICT_STR);
#endif
    EXPECT_EQ(d.getInt("big", -1), -1);         // does not fit int32: default
}

TEST_F(DictionaryApi, FlashStringValueIsTextNotBool) {
    Dictionary d;
    d.set("k", F("flash text"));
    EXPECT_EQ(d.type("k"), DICT_STR);
    EXPECT_STREQ(d.getString("k").c_str(), "flash text");
}

TEST_F(DictionaryApi, NullCharPointerStoresNull) {
    Dictionary d;
    d.set("k", (const char*)NULL);
    EXPECT_EQ(d.type("k"), DICT_NULL);
    EXPECT_STREQ(d.getString("k").c_str(), "null");
}

TEST_F(DictionaryApi, StringKeysWork) {
    Dictionary d;
    String k("key");
    d.set(k, String("v"));
    EXPECT_TRUE(d.has(k));
    EXPECT_TRUE(d.has("key"));
    EXPECT_STREQ(d.getString(k).c_str(), "v");
    EXPECT_EQ(d.remove(k), DICTIONARY_OK);
    EXPECT_FALSE(d.has("key"));
}

TEST_F(DictionaryApi, ReplacingChangesTypeAndKeepsPosition) {
    Dictionary d;
    d.set("a", 1); d.set("b", "two"); d.set("c", 3);
    d.set("b", 2.5f);                          // same 4-byte footprint as "two\0": in place
    EXPECT_EQ(d.type("b"), DICT_FLOAT);
    d.set("b", "a much longer text value");   // new allocation, same position
    EXPECT_EQ(d.type("b"), DICT_STR);
    EXPECT_STREQ(d.keyAt(1), "b");
    EXPECT_EQ(d.count(), 3u);
}

TEST_F(DictionaryApi, InvalidKeysAndValues) {
    Dictionary d;
    std::string longKey(_DICT_KEYLEN + 1, 'k');
    std::string longVal(_DICT_VALLEN + 1, 'v');
    EXPECT_EQ(d.set("", 1), DICTIONARY_ERR);
    EXPECT_EQ(d.set(longKey.c_str(), 1), DICTIONARY_ERR);
    EXPECT_EQ(d.set("k", longVal.c_str()), DICTIONARY_ERR);
    EXPECT_EQ(d.set((const char*)NULL, 1), DICTIONARY_ERR);
    EXPECT_EQ(d.count(), 0u);
    EXPECT_FALSE(d.has(""));
    EXPECT_FALSE(d.has(longKey.c_str()));
}

// ---- getters and conversion -----------------------------------------------------
TEST_F(DictionaryApi, GettersOnMatchingTypes) {
    Dictionary d;
    d.set("i", -12); d.set("f", 0.25f); d.set("b", false); d.set("s", "hello");
    EXPECT_EQ(d.getInt("i"), -12);
    EXPECT_FLOAT_EQ(d.getFloat("f"), 0.25f);
    EXPECT_FALSE(d.getBool("b", true));
    EXPECT_STREQ(d.getString("s").c_str(), "hello");
}

TEST_F(DictionaryApi, MissingKeyReturnsDefault) {
    Dictionary d;
    EXPECT_EQ(d.getInt("x", 7), 7);
    EXPECT_FLOAT_EQ(d.getFloat("x", 1.5f), 1.5f);
    EXPECT_TRUE(d.getBool("x", true));
    EXPECT_STREQ(d.getString("x", "dflt").c_str(), "dflt");
    EXPECT_EQ(d.peek("x"), (const char*)NULL);
}

TEST_F(DictionaryApi, GetStringRendersEveryType) {
    Dictionary d;
    d.set("i", -42); d.set("f", 3.25f); d.set("t", true); d.set("n", false); d.setNull("z");
    d.set("pi", 3.14159f); d.set("tenth", 0.1f); d.set("whole", 2.0f);
    EXPECT_STREQ(d.getString("i").c_str(), "-42");
    EXPECT_STREQ(d.getString("f").c_str(), "3.25");
    EXPECT_STREQ(d.getString("t").c_str(), "true");
    EXPECT_STREQ(d.getString("n").c_str(), "false");
    EXPECT_STREQ(d.getString("z").c_str(), "null");
    EXPECT_STREQ(d.getString("pi").c_str(), "3.14159");
    EXPECT_STREQ(d.getString("tenth").c_str(), "0.1");
    EXPECT_STREQ(d.getString("whole").c_str(), "2");
}

TEST_F(DictionaryApi, FloatRenderingExtremes) {
    Dictionary d;
    d.set("big", 1.5e20f); d.set("tiny", 2.5e-7f); d.set("neg", -0.5f);
    d.set("inf", INFINITY); d.set("nan", NAN);
    EXPECT_STREQ(d.getString("big").c_str(), "1.5e20");
    EXPECT_STREQ(d.getString("tiny").c_str(), "2.5e-7");
    EXPECT_STREQ(d.getString("neg").c_str(), "-0.5");
    EXPECT_STREQ(d.getString("inf").c_str(), "inf");
    EXPECT_STREQ(d.getString("nan").c_str(), "nan");
    // JSON has no nan/inf: they are written as null
    Dictionary j; j.set("inf", INFINITY);
    EXPECT_STREQ(j.json().c_str(), "{\"inf\":null}");
}

TEST_F(DictionaryApi, ConversionFromText) {
    Dictionary d;
    d.set("port", "80"); d.set("ratio", "0.5"); d.set("on", "TRUE"); d.set("one", "1");
    d.set("junk", "80abc"); d.set("big", "99999999999");
#ifdef _DICT_STRICT_GET
    EXPECT_EQ(d.getInt("port", -1), -1);
    EXPECT_FLOAT_EQ(d.getFloat("ratio", -1), -1);
    EXPECT_FALSE(d.getBool("on", false));
#else
    EXPECT_EQ(d.getInt("port", -1), 80);
    EXPECT_FLOAT_EQ(d.getFloat("ratio", -1), 0.5f);
    EXPECT_TRUE(d.getBool("on", false));
    EXPECT_TRUE(d.getBool("one", false));
    EXPECT_FLOAT_EQ(d.getFloat("port", -1), 80.0f);
#endif
    EXPECT_EQ(d.getInt("junk", -1), -1);        // never a partial parse
    EXPECT_EQ(d.getInt("big", -1), -1);         // out of int32 range
}

TEST_F(DictionaryApi, ConversionBetweenNumbersAndBool) {
    Dictionary d;
    d.set("i", 5); d.set("f", 2.0f); d.set("frac", 2.5f); d.set("t", true); d.setNull("z");
    EXPECT_FLOAT_EQ(d.getFloat("i"), 5.0f);     // numbers widen in every mode
#ifdef _DICT_STRICT_GET
    EXPECT_EQ(d.getInt("f", -1), -1);
    EXPECT_EQ(d.getInt("t", -1), -1);
    EXPECT_FALSE(d.getBool("i", false));
#else
    EXPECT_EQ(d.getInt("f", -1), 2);            // integral float
    EXPECT_EQ(d.getInt("frac", -1), -1);        // not integral: default
    EXPECT_EQ(d.getInt("t", -1), 1);
    EXPECT_TRUE(d.getBool("i", false));
    EXPECT_TRUE(d.getBool("f", true));          // float -> bool: default
#endif
    EXPECT_EQ(d.getInt("z", -1), -1);           // null never converts
    EXPECT_TRUE(d.getBool("z", true));
}

TEST_F(DictionaryApi, GetStringIntoBuffer) {
    Dictionary d;
    d.set("k", "hello world"); d.set("n", 12345);
    char buf[6];
    EXPECT_EQ(d.getString("k", buf, sizeof(buf)), 11u);   // full length, truncated copy
    EXPECT_STREQ(buf, "hello");
    char big[32];
    EXPECT_EQ(d.getString("n", big, sizeof(big)), 5u);
    EXPECT_STREQ(big, "12345");
    EXPECT_EQ(d.getString("missing", big, sizeof(big)), 0u);
    EXPECT_STREQ(big, "");
}

TEST_F(DictionaryApi, PeekIsZeroCopyForPlainText) {
    Dictionary d;
    d.set("s", "plain value"); d.set("i", 3);
    const char* p = d.peek("s");
#ifndef _DICT_COMPRESS
    ASSERT_NE(p, (const char*)NULL);
    EXPECT_STREQ(p, "plain value");
    EXPECT_EQ(p, d.peek("s"));                  // same storage, not a copy
#else
    EXPECT_EQ(p, (const char*)NULL);            // this value packs: no zero-copy view
#endif
    EXPECT_EQ(d.peek("i"), (const char*)NULL);  // not text
}

// ---- positional access and order ------------------------------------------------
TEST_F(DictionaryApi, PositionalAccess) {
    Dictionary d;
    d.set("a", 1); d.set("b", "x"); d.set("c", true);
    EXPECT_STREQ(d.keyAt(0), "a");
    EXPECT_STREQ(d.keyAt(2), "c");
    EXPECT_EQ(d.keyAt(3), (const char*)NULL);
    EXPECT_EQ(d.typeAt(0), DICT_INT);
    EXPECT_EQ(d.typeAt(1), DICT_STR);
    EXPECT_EQ(d.typeAt(9), DICT_NONE);
    EXPECT_STREQ(d.key(1).c_str(), "b");
    EXPECT_STREQ(d.value(0).c_str(), "1");
    EXPECT_STREQ(d.value(2).c_str(), "true");
    EXPECT_STREQ(d.value(5).c_str(), "");
}

TEST_F(DictionaryApi, RemoveKeepsInsertionOrder) {
    // 3.x reordered positions when the removed tree node had two children (D18).
    Dictionary d;
    d.set("b", 1); d.set("a", 2); d.set("c", 3); d.set("d", 4);
    ASSERT_EQ(d.remove("b"), DICTIONARY_OK);
    ASSERT_EQ(d.count(), 3u);
    EXPECT_STREQ(d.keyAt(0), "a");
    EXPECT_STREQ(d.keyAt(1), "c");
    EXPECT_STREQ(d.keyAt(2), "d");
    EXPECT_EQ(d.remove("missing"), DICTIONARY_OK);
    EXPECT_EQ(d.remove(""), DICTIONARY_OK);
}

TEST_F(DictionaryApi, ManyKeysWithIndexAndRemovals) {
    Dictionary d;
    const int N = 2000;
    for (int i = 0; i < N; i++) ASSERT_EQ(d.set(("key" + std::to_string(i)).c_str(), i), DICTIONARY_OK);
    for (int i = 0; i < N; i += 3) ASSERT_EQ(d.remove(("key" + std::to_string(i)).c_str()), DICTIONARY_OK);
    int expectPos = 0;
    for (int i = 0; i < N; i++) {
        std::string k = "key" + std::to_string(i);
        if (i % 3 == 0) { EXPECT_FALSE(d.has(k.c_str())) << k; continue; }
        EXPECT_EQ(d.getInt(k.c_str(), -1), i) << k;
        EXPECT_STREQ(d.keyAt(expectPos++), k.c_str());
    }
    EXPECT_EQ((int)d.count(), expectPos);
}

// ---- memory management ------------------------------------------------------------
TEST_F(DictionaryApi, ReserveAndDestroy) {
    Dictionary d;
    ASSERT_EQ(d.reserve(100), DICTIONARY_OK);
    size_t reserved = d.size();
    EXPECT_GE(reserved, 100 * sizeof(void*));
    for (int i = 0; i < 100; i++) d.set(("k" + std::to_string(i)).c_str(), i);
    EXPECT_EQ(d.count(), 100u);
    d.destroy();
    EXPECT_EQ(d.count(), 0u);
    EXPECT_EQ(d.size(), 0u);
    EXPECT_EQ(d.set("again", 1), DICTIONARY_OK);
    EXPECT_EQ(d.reserve(_DICT_MAX_ENTRIES + 1), DICTIONARY_OOB);
}

TEST_F(DictionaryApi, SizeCountsEntries) {
    Dictionary d;
    EXPECT_EQ(d.size(), 0u);
    d.set("ab", "cde");
    EXPECT_GT(d.size(), 5u);
    size_t before = d.size();
    d.set("n", true);                           // bool: header + key only
    EXPECT_GT(d.size(), before);
}

TEST_F(DictionaryApi, MoveAndCopyAssignment) {
    Dictionary a;
    a.set("x", 1); a.set("y", "two");
    Dictionary b(std::move(a));
    EXPECT_EQ(b.count(), 2u);
    EXPECT_EQ(a.count(), 0u);
    EXPECT_EQ(a.set("z", 3), DICTIONARY_OK);    // moved-from stays usable
    Dictionary c;
    c = b;                                      // deep copy
    EXPECT_TRUE(c == b);
    c.set("x", 100);
    EXPECT_EQ(b.getInt("x"), 1);                // independent storage
    const Dictionary& cref = c;
    Dictionary e;
    e = cref;                                   // assignment from const
    EXPECT_TRUE(e == c);
}

// ---- equality, merge -------------------------------------------------------------
TEST_F(DictionaryApi, EqualityComparesTypesAndDistinguishesEmptyValues) {
    Dictionary a, b;
    a.set("x", ""); b.set("y", "");
    EXPECT_FALSE(a == b);                       // 3.x said equal (D16)
    Dictionary c, d;
    c.set("n", "80"); d.set("n", 80);
    EXPECT_FALSE(c == d);                       // text "80" is not the number 80
    Dictionary e, f;
    e.set("a", 1); e.set("b", "two");
    f.set("b", "two"); f.set("a", 1);
    EXPECT_TRUE(e == f);                        // order does not matter
}

TEST_F(DictionaryApi, MergeCopiesTypesAndOverwrites) {
    Dictionary a, b;
    a.set("keep", 1); a.set("shared", "old");
    b.set("shared", 2.5f); b.set("new", true);
    ASSERT_EQ(a.merge(b), DICTIONARY_OK);
    EXPECT_EQ(a.count(), 3u);
    EXPECT_EQ(a.type("shared"), DICT_FLOAT);
    EXPECT_TRUE(a.getBool("new"));
    EXPECT_EQ(a.merge(a), DICTIONARY_OK);       // self-merge is a no-op
    EXPECT_EQ(a.count(), 3u);
}

// ---- JSON output ---------------------------------------------------------------------
TEST_F(DictionaryApi, JsonWritesTypedValues) {
    Dictionary d;
    d.set("s", "v"); d.set("i", 7); d.set("f", 0.5f); d.set("t", true); d.setNull("z");
    EXPECT_STREQ(d.json().c_str(), "{\"s\":\"v\",\"i\":7,\"f\":0.5,\"t\":true,\"z\":null}");
    EXPECT_EQ(d.jsize(), d.json().length() + 1);    // exact
}

TEST_F(DictionaryApi, JsonEscapesControlCharacters) {
    Dictionary d;
    d.set("k\"ey", "a\nb\tc\x01\\");
    EXPECT_STREQ(d.json().c_str(), "{\"k\\\"ey\":\"a\\nb\\tc\\u0001\\\\\"}");
    EXPECT_EQ(d.jsize(), d.json().length() + 1);
}

TEST_F(DictionaryApi, JsonToPrint) {
    Dictionary d;
    d.set("a", 1); d.set("b", "x");
    TestPrint p;
    size_t n = d.json(p);
    EXPECT_EQ(p.out, std::string(d.json().c_str()));
    EXPECT_EQ(n, p.out.size());
}

TEST_F(DictionaryApi, EsizeUsesTextLengths) {
    Dictionary d;
    d.set("ab", "cde"); d.set("n", 123);
    EXPECT_EQ(d.esize(), (2u + 1 + 3 + 1) + (1u + 1 + 3 + 1));
}

#ifdef _DICT_WIDE_NUMBERS
TEST_F(DictionaryApi, WideNumbers) {
    Dictionary d;
    d.set("d", 3.141592653589793);
    d.set("i", -9000000000LL);
    EXPECT_EQ(d.type("d"), DICT_DOUBLE);
    EXPECT_DOUBLE_EQ(d.getDouble("d"), 3.141592653589793);
    EXPECT_EQ(d.getInt64("i"), -9000000000LL);
    EXPECT_EQ(d.getInt64("missing", 5), 5);
}
#else
TEST_F(DictionaryApi, DoubleIsStoredAsFloat) {
    Dictionary d;
    d.set("d", 0.1);
    EXPECT_EQ(d.type("d"), DICT_FLOAT);
    EXPECT_STREQ(d.getString("d").c_str(), "0.1");
}
#endif

#ifdef _DICT_COMPRESS
TEST_F(DictionaryApi, CompressedTextIsPackedButReadsBack) {
    Dictionary d;
    const char* v = "https://the.server.example.com/api/v1/the/resource";
    d.set("url", v);
    EXPECT_EQ(d.peek("url"), (const char*)NULL);    // packed: no zero-copy view
    EXPECT_STREQ(d.getString("url").c_str(), v);
    Dictionary raw;
    raw.set("k", "zq");                              // nothing to gain: stays plain
    EXPECT_STREQ(raw.peek("k"), "zq");
}
#endif

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
