// test-dictionary-delete.cpp - remove() semantics (leaf, one-child, two-child),
// the documented bulk-delete idiom, destroy(), and delete/reinsert cycles.
// Default configuration.
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"

#include <string>
#include <set>

class DictionaryDelete : public ::testing::Test {};

TEST_F(DictionaryDelete, RemoveSingleKey) {
    Dictionary d;
    d("a", "1"); d("b", "2");
    ASSERT_EQ(d.remove("a"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 1u);
    EXPECT_STREQ(d["a"].c_str(), "");
    EXPECT_STREQ(d["b"].c_str(), "2");
}

TEST_F(DictionaryDelete, RemoveNonExistentIsNoOp) {
    Dictionary d;
    d("a", "1");
    EXPECT_EQ(d.remove("zzz"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 1u);
    EXPECT_STREQ(d["a"].c_str(), "1");
}

// Single-char keys order by byte value, so "b" is the root with "a" (left) and
// "c" (right) as children. Removing "b" exercises the two-children path where
// the in-order successor ("c") is promoted into the deleted node's slot.
TEST_F(DictionaryDelete, TwoChildDeletePromotesSuccessor) {
    Dictionary d;
    d("a", "va"); d("b", "vb"); d("c", "vc");   // root "b" gains two children
    ASSERT_EQ(d.remove("b"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 2u);
    EXPECT_STREQ(d["b"].c_str(), "");
    EXPECT_STREQ(d["a"].c_str(), "va");
    EXPECT_STREQ(d["c"].c_str(), "vc");
}

TEST_F(DictionaryDelete, RemoveRootLeafEmptiesTree) {
    Dictionary d;
    d("only", "1");
    ASSERT_EQ(d.remove("only"), DICTIONARY_OK);
    EXPECT_EQ(d.count(), 0u);
    EXPECT_STREQ(d["only"].c_str(), "");
}

TEST_F(DictionaryDelete, BulkDeleteIdiomEmptiesFully) {
    Dictionary d;
    const int N = 300;
    for (int i = 0; i < N; i++)
        d.insert(("key" + std::to_string(i)).c_str(), ("v" + std::to_string(i)).c_str());
    ASSERT_EQ(d.count(), (size_t)N);
    while (d.count()) d.remove(d(0).c_str());
    EXPECT_EQ(d.count(), 0u);
}

TEST_F(DictionaryDelete, RemoveHalfKeepsTheRest) {
    Dictionary d;
    const int N = 200;
    for (int i = 0; i < N; i++)
        d.insert(("k" + std::to_string(i)).c_str(), ("v" + std::to_string(i)).c_str());

    // Remove all even-indexed keys.
    for (int i = 0; i < N; i += 2)
        ASSERT_EQ(d.remove(("k" + std::to_string(i)).c_str()), DICTIONARY_OK);

    EXPECT_EQ(d.count(), (size_t)(N / 2));
    for (int i = 0; i < N; i++) {
        std::string k = "k" + std::to_string(i);
        if (i % 2 == 0) EXPECT_STREQ(d.search(k.c_str()).c_str(), "") << k;
        else            EXPECT_STREQ(d.search(k.c_str()).c_str(), ("v" + std::to_string(i)).c_str()) << k;
    }
}

TEST_F(DictionaryDelete, DestroyEmptiesAndObjectIsReusable) {
    Dictionary d;
    d("a", "1"); d("b", "2");
    d.destroy();
    EXPECT_EQ(d.count(), 0u);
    // reusable after destroy()
    d("x", "9");
    EXPECT_EQ(d.count(), 1u);
    EXPECT_STREQ(d["x"].c_str(), "9");
}

TEST_F(DictionaryDelete, DeleteThenReinsertSameKey) {
    Dictionary d;
    d("k", "first");
    ASSERT_EQ(d.remove("k"), DICTIONARY_OK);
    EXPECT_STREQ(d["k"].c_str(), "");
    d("k", "second");
    EXPECT_EQ(d.count(), 1u);
    EXPECT_STREQ(d["k"].c_str(), "second");
}

// Interleaved insert/remove churn should never lose or corrupt live entries.
TEST_F(DictionaryDelete, InterleavedChurnRemainsConsistent) {
    Dictionary d;
    std::set<int> live;
    for (int i = 0; i < 400; i++) {
        std::string k = "key" + std::to_string(i);
        d.insert(k.c_str(), std::to_string(i).c_str());
        live.insert(i);
        if (i % 3 == 0 && !live.empty()) {   // periodically remove an old key
            int victim = *live.begin();
            d.remove(("key" + std::to_string(victim)).c_str());
            live.erase(victim);
        }
    }
    EXPECT_EQ(d.count(), live.size());
    for (int v : live)
        EXPECT_STREQ(d.search(("key" + std::to_string(v)).c_str()).c_str(),
                     std::to_string(v).c_str());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
