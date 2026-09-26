// test-dictionary-jload.cpp - the 4.0 jload() parser: the 3.x defects it fixes
// (empty values, trailing comments, last pair without a separator, escapes, error
// codes, token length cap, nested structures, ASCII filter), typed loading, streams
// and json() round trips. Built for the default configuration and for
// _DICT_TYPED_JSON, _DICT_ASCII_ONLY and a large _DICT_VALLEN (heap value buffer).
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"
#include "TestStream.h"

#include <string>

class DictionaryJload : public ::testing::Test {};

// ---- 3.x defects (ids from the 2026-09-26 review) ------------------------------------
TEST_F(DictionaryJload, EmptyQuotedValueLoads) {                     // D8
    Dictionary d;
    ASSERT_EQ(d.jload("{\"k\":\"\"}"), DICTIONARY_OK);
    EXPECT_TRUE(d.has("k"));
    EXPECT_STREQ(d.getString("k", "x").c_str(), "");
    Dictionary e;
    ASSERT_EQ(e.jload(d.json()), DICTIONARY_OK);
    EXPECT_TRUE(e == d);
}

TEST_F(DictionaryJload, EmptyBareValueLoads) {
    Dictionary d;
    ASSERT_EQ(d.jload("a:,b:2"), DICTIONARY_OK);
    EXPECT_TRUE(d.has("a"));
    EXPECT_STREQ(d.getString("a", "x").c_str(), "");
    EXPECT_STREQ(d.getString("b").c_str(), "2");
}

TEST_F(DictionaryJload, TrailingCommentWithoutComma) {                // D9
    Dictionary d;
    ASSERT_EQ(d.jload("{\n\"a\":\"1\" # first\n\"b\":\"2\" # last\n}\n"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_STREQ(d.getString("a").c_str(), "1");
    EXPECT_STREQ(d.getString("b").c_str(), "2");
}

TEST_F(DictionaryJload, LastPairWithoutSeparator) {                   // D10
    Dictionary d;
    ASSERT_EQ(d.jload("a:1\nb:2"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_STREQ(d.getString("b").c_str(), "2");
}

TEST_F(DictionaryJload, StandardEscapesDecoded) {                     // D11
    Dictionary d;
    ASSERT_EQ(d.jload("{\"a\":\"x\\ny\\tz\\/\\\"\\\\\",\"u\":\"\\u00e9\\u20ac\\ud83d\\ude00\"}"), DICTIONARY_OK);
    EXPECT_STREQ(d.getString("a").c_str(), "x\ny\tz/\"\\");
#ifdef _DICT_ASCII_ONLY
    EXPECT_STREQ(d.getString("u").c_str(), "");                   // non-ASCII code points dropped
#else
    EXPECT_STREQ(d.getString("u").c_str(), "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");   // UTF-8
#endif
}

TEST_F(DictionaryJload, ControlCharactersRoundTrip) {                 // D11
    Dictionary a;
    a.set("k", "line1\nline2\r\n\ttab\x02");
    Dictionary b;
    ASSERT_EQ(b.jload(a.json()), DICTIONARY_OK);
    EXPECT_TRUE(a == b);
}

TEST_F(DictionaryJload, InsertErrorsPropagate) {                      // D12
    Dictionary d;
    // an empty quoted key is rejected by the parser; an overlong one by the length cap
    EXPECT_EQ(d.jload("{\"\":\"v\"}"), DICTIONARY_FMT);
    std::string k(_DICT_KEYLEN + 1, 'k');
    EXPECT_EQ(d.jload(String(("{\"" + k + "\":\"v\"}").c_str())), DICTIONARY_OOB);
}

TEST_F(DictionaryJload, TokenLengthIsCapped) {                        // D13
    Dictionary d;
    std::string big = "{\"a\":\"" + std::string(_DICT_VALLEN + 50, 'v') + "\"}";
    EXPECT_EQ(d.jload(String(big.c_str())), DICTIONARY_OOB);
    EXPECT_EQ(d.count(), 0u);
    std::string exact = "{\"a\":\"" + std::string(_DICT_VALLEN, 'v') + "\"}";
    EXPECT_EQ(d.jload(String(exact.c_str())), DICTIONARY_OK);
    EXPECT_EQ(d.getString("a").length(), (unsigned)_DICT_VALLEN);
}

TEST_F(DictionaryJload, NestedStructuresRejected) {                   // D14
    Dictionary d;
    EXPECT_EQ(d.jload("{\"a\":[1,2]}"), DICTIONARY_FMT);
    EXPECT_EQ(d.jload("{\"a\":{\"b\":1}}"), DICTIONARY_FMT);
    EXPECT_EQ(d.jload("[1,2]"), DICTIONARY_FMT);
    EXPECT_EQ(d.count(), 0u);
}

TEST_F(DictionaryJload, AsciiFilter) {                                // D15
    Dictionary d;
    ASSERT_EQ(d.jload("{\"k\":\"a\xC3\xA9z\"}"), DICTIONARY_OK);
#ifdef _DICT_ASCII_ONLY
    EXPECT_STREQ(d.getString("k").c_str(), "az");
#else
    EXPECT_STREQ(d.getString("k").c_str(), "a\xC3\xA9z");
#endif
}

// ---- error codes ---------------------------------------------------------------------
TEST_F(DictionaryJload, ErrorCodes) {
    Dictionary d;
    EXPECT_EQ(d.jload("{\"a\" \"b\"}"), DICTIONARY_COLON);       // missing ':'
    EXPECT_EQ(d.jload("{\"a\":\"1\" \"b\":\"2\"}"), DICTIONARY_COMMA);   // missing separator
    EXPECT_EQ(d.jload("{\"a\":\"x\ny\"}"), DICTIONARY_QUOTE);   // newline inside quotes
    EXPECT_EQ(d.jload("{\"a\":\"\\uZZZZ\"}"), DICTIONARY_BCKSL); // bad \u escape
    EXPECT_EQ(d.jload("{\"a\":\"\\ud800x\"}"), DICTIONARY_BCKSL); // lone surrogate
    EXPECT_EQ(d.jload("{\"a\":\"\\u0000\"}"), DICTIONARY_BCKSL); // NUL
    EXPECT_EQ(d.jload("{\"a\":\"open"), DICTIONARY_EOF);         // ends inside quotes
    EXPECT_EQ(d.jload("{\"a\":"), DICTIONARY_EOF);               // ends before the value
    EXPECT_EQ(d.jload("{\"a\""), DICTIONARY_EOF);                // ends before ':'
    EXPECT_EQ(d.jload("{,}"), DICTIONARY_FMT);                   // empty pair
    EXPECT_EQ(d.jload("{:1}"), DICTIONARY_FMT);                  // missing key
    EXPECT_EQ(d.jload((const char*)NULL), DICTIONARY_ERR);
}

TEST_F(DictionaryJload, NotAtomicPairsBeforeAnErrorRemain) {
    Dictionary d;
    EXPECT_EQ(d.jload("{\"a\":\"1\",\"b\":[2]}"), DICTIONARY_FMT);
    EXPECT_TRUE(d.has("a"));
    EXPECT_FALSE(d.has("b"));
}

// ---- lenient format (kept from 3.x) ----------------------------------------------------
TEST_F(DictionaryJload, BareTokensCommentsAndLayout) {
    Dictionary d;
    const char* js =
        "# config\r\n"
        "{\r\n"
        "  ssid : \"my net\",   # quoted value with a space\r\n"
        "  url  : http://ota.home.lan:8080/fw.bin\r\n"   // bare value with ':' and newline separator
        "  note : is ok ,\r\n"                         // bare value, inner space kept, ends trimmed
        "  \"k\\\"q\" : \"v\",\r\n"                    // escaped quote in a key
        "}\r\n";
    ASSERT_EQ(d.jload(js), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 4u);
    EXPECT_STREQ(d.getString("ssid").c_str(), "my net");
    EXPECT_STREQ(d.getString("url").c_str(), "http://ota.home.lan:8080/fw.bin");
    EXPECT_STREQ(d.getString("note").c_str(), "is ok");
    EXPECT_STREQ(d.getString("k\"q").c_str(), "v");
}

TEST_F(DictionaryJload, ValueOnNextLine) {
    Dictionary d;
    ASSERT_EQ(d.jload("{\n  \"a\":\n    \"value\"\n}"), DICTIONARY_OK);
    EXPECT_STREQ(d.getString("a").c_str(), "value");
}

TEST_F(DictionaryJload, PartialLoadCount) {
    Dictionary d;
    EXPECT_EQ(d.jload("{\"a\":1,\"b\":2,\"c\":3}", 2), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_FALSE(d.has("c"));
    Dictionary e;
    EXPECT_EQ(e.jload("{\"a\":1}", 3), DICTIONARY_EOF);         // fewer pairs than asked
    EXPECT_EQ(e.count(), 1u);
}

TEST_F(DictionaryJload, DuplicateKeysLastWins) {
    Dictionary d;
    ASSERT_EQ(d.jload("{\"a\":\"1\",\"a\":\"2\"}"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 1u);
    EXPECT_STREQ(d.getString("a").c_str(), "2");
}

TEST_F(DictionaryJload, FromStream) {
    Dictionary d;
    TestStream s("{\"x\":\"10\",\"y\":true}");
    ASSERT_EQ(d.jload(s), DICTIONARY_OK);
    EXPECT_EQ(d.getInt("x"), 10);
    EXPECT_TRUE(d.getBool("y"));
}

// ---- typing ------------------------------------------------------------------------------
TEST_F(DictionaryJload, BareValuesTyping) {
    Dictionary d;
    ASSERT_EQ(d.jload("{\"i\":80,\"f\":0.50,\"t\":true,\"n\":null,\"q\":\"80\",\"lz\":007,\"e\":1e3}"), DICTIONARY_OK);
    EXPECT_EQ(d.type("q"), DICT_STR);                // quoted is always text
    EXPECT_EQ(d.type("lz"), DICT_STR);               // not a JSON number
#ifdef _DICT_TYPED_JSON
    EXPECT_EQ(d.type("i"), DICT_INT);
    EXPECT_EQ(d.type("f"), DICT_FLOAT);
    EXPECT_EQ(d.type("t"), DICT_BOOL);
    EXPECT_EQ(d.type("n"), DICT_NULL);
    EXPECT_EQ(d.type("e"), DICT_FLOAT);
    EXPECT_STREQ(d.json().c_str(), "{\"i\":80,\"f\":0.5,\"t\":true,\"n\":null,\"q\":\"80\",\"lz\":\"007\",\"e\":1000}");
#else
    // default: values are stored as given, so json() output matches 3.x
    EXPECT_EQ(d.type("i"), DICT_STR);
    EXPECT_EQ(d.type("t"), DICT_STR);
    EXPECT_STREQ(d.getString("f").c_str(), "0.50");  // verbatim
    EXPECT_STREQ(d.json().c_str(), "{\"i\":\"80\",\"f\":\"0.50\",\"t\":\"true\",\"n\":\"null\",\"q\":\"80\",\"lz\":\"007\",\"e\":\"1e3\"}");
#endif
    EXPECT_EQ(d.getInt("i"), 80);                    // converts on read either way
    EXPECT_TRUE(d.getBool("t"));
}

TEST_F(DictionaryJload, TypedRoundTrip) {
    Dictionary a;
    a.set("s", "text"); a.set("i", -5); a.set("f", 2.5f); a.set("b", false); a.setNull("z");
    Dictionary b;
    ASSERT_EQ(b.jload(a.json()), DICTIONARY_OK);
#ifdef _DICT_TYPED_JSON
    EXPECT_TRUE(a == b);
#else
    EXPECT_STREQ(b.json().c_str(), "{\"s\":\"text\",\"i\":\"-5\",\"f\":\"2.5\",\"b\":\"false\",\"z\":\"null\"}");
    EXPECT_EQ(b.getInt("i"), -5);
#endif
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
