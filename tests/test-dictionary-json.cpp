// test-dictionary-json.cpp - json() output, jload() parsing (string & stream),
// JSON comments, CRLF, unquoted values, escaping round-trips, size estimators,
// and parse error codes. Default configuration.
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"

#include <string>

class DictionaryJson : public ::testing::Test {};

// ---- json() -----------------------------------------------------------------
TEST_F(DictionaryJson, EmitsInsertionOrder) {
    Dictionary d;
    d("a", "1"); d("b", "2"); d("c", "3");
    EXPECT_STREQ(d.json().c_str(), "{\"a\":\"1\",\"b\":\"2\",\"c\":\"3\"}");
}

TEST_F(DictionaryJson, EmptyDictionary) {
    Dictionary d;
    EXPECT_STREQ(d.json().c_str(), "{}");
}

TEST_F(DictionaryJson, EscapesQuotesAndBackslashesInValues) {
    Dictionary d;
    d("k", "he said \"hi\"");
    // " must be escaped as \"
    EXPECT_STREQ(d.json().c_str(), "{\"k\":\"he said \\\"hi\\\"\"}");
}

TEST_F(DictionaryJson, EscapesBackslash) {
    Dictionary d;
    d("path", "a\\b");
    EXPECT_STREQ(d.json().c_str(), "{\"path\":\"a\\\\b\"}");
}

// ---- json() -> jload() round-trip (the v3.6.0 escaping fix) ------------------
TEST_F(DictionaryJson, RoundTripWithSpecialChars) {
    Dictionary a;
    a("plain", "value");
    a("quote", "say \"yes\"");
    a("slash", "c:\\temp\\x");
    a("mix",   "a\"b\\c");

    String js = a.json();

    Dictionary b;
    ASSERT_EQ(b.jload(js), DICTIONARY_OK);
    EXPECT_EQ(b.count(), a.count());
    EXPECT_STREQ(b["plain"].c_str(), "value");
    EXPECT_STREQ(b["quote"].c_str(), "say \"yes\"");
    EXPECT_STREQ(b["slash"].c_str(), "c:\\temp\\x");
    EXPECT_STREQ(b["mix"].c_str(),   "a\"b\\c");
    EXPECT_TRUE(a == b);
}

// ---- jload() from String ----------------------------------------------------
TEST_F(DictionaryJson, LoadBasic) {
    Dictionary d;
    ASSERT_EQ(d.jload("{\"ssid\":\"devices\",\"pwd\":\"secret\"}"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_STREQ(d["ssid"].c_str(), "devices");
    EXPECT_STREQ(d["pwd"].c_str(),  "secret");
}

TEST_F(DictionaryJson, LoadEmptyObject) {
    Dictionary d;
    ASSERT_EQ(d.jload("{}"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 0u);
}

TEST_F(DictionaryJson, LoadPartialWithCount) {
    Dictionary d;
    ASSERT_EQ(d.jload("{\"a\":\"1\",\"b\":\"2\",\"c\":\"3\"}", 2), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_STREQ(d["a"].c_str(), "1");
    EXPECT_STREQ(d["b"].c_str(), "2");
    EXPECT_STREQ(d["c"].c_str(), "");   // not loaded
}

TEST_F(DictionaryJson, LoadUnquotedValuesAndKeys) {
    Dictionary d;
    ASSERT_EQ(d.jload("{ value : 3, anothervalue : \"23\", and_this : is_ok_too }"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 3u);
    EXPECT_STREQ(d["value"].c_str(), "3");
    EXPECT_STREQ(d["anothervalue"].c_str(), "23");
    EXPECT_STREQ(d["and_this"].c_str(), "is_ok_too");
}

TEST_F(DictionaryJson, LoadWithComments) {
    Dictionary d;
    const char* js =
        "# a leading comment line\n"
        "{\n"
        "  \"a\" : \"1\", # trailing line comment\n"
        "  \"b\" : \"2\"\n"
        "}\n";
    ASSERT_EQ(d.jload(js), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_STREQ(d["a"].c_str(), "1");
    EXPECT_STREQ(d["b"].c_str(), "2");
}

TEST_F(DictionaryJson, LoadWindowsLineEndings) {
    Dictionary d;
    ASSERT_EQ(d.jload("{\r\n\"a\":\"1\",\r\n\"b\":\"2\"\r\n}"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_STREQ(d["a"].c_str(), "1");
    EXPECT_STREQ(d["b"].c_str(), "2");
}

// ---- jload() from a Stream --------------------------------------------------
TEST_F(DictionaryJson, LoadFromStream) {
    Dictionary d;
    std::string buf = "{\"x\":\"10\",\"y\":\"20\"}";
    ReadBufferStream stream((uint8_t*)buf.data(), buf.size());
    ASSERT_EQ(d.jload(stream), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_STREQ(d["x"].c_str(), "10");
    EXPECT_STREQ(d["y"].c_str(), "20");
}

// ---- parse errors -----------------------------------------------------------
TEST_F(DictionaryJson, NewlineInsideQuotedStringIsError) {
    Dictionary d;
    EXPECT_EQ(d.jload("{\"a\":\"line1\nline2\"}"), DICTIONARY_QUOTE);
}

// ---- size estimators --------------------------------------------------------
TEST_F(DictionaryJson, JsizeIsAtLeastActualJsonLength) {
    Dictionary d;
    d("ssid", "devices");
    d("port", "80");
    String j = d.json();
    // jsize() reserves for the unescaped form; must cover the plain JSON length.
    EXPECT_GE(d.jsize(), j.length());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
