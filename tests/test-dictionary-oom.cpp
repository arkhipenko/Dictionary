// test-dictionary-oom.cpp - out-of-memory safety.
//
// malloc() is intercepted via the linker's --wrap so we can force an allocation
// failure at a precise point and assert that the library (a) never crashes,
// (b) returns an error code, and (c) leaves existing data intact. Run this suite
// under AddressSanitizer to also catch any invalid free / use-after-free on the
// failure paths. Built for the default configuration and with _DICT_COMPRESS.
//
// Link with: -Wl,--wrap=malloc,--wrap=_Znwm
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"

#include <string>
#include <new>
#include <utility>

// ---- malloc fault injection -------------------------------------------------
extern "C" void* __real_malloc(size_t);

namespace {
    bool  g_armed = false;   // only inject while armed (keeps gtest/std allocs safe)
    long  g_calls = 0;       // mallocs seen since arming
    long  g_fail_at = -1;    // fail the g_fail_at-th malloc (1-based); -1 = never
}

extern "C" void* __wrap_malloc(size_t n) {
    if (g_armed) {
        ++g_calls;
        if (g_fail_at >= 0 && g_calls >= g_fail_at) return nullptr;
    }
    return __real_malloc(n);
}

// Global operator new is wrapped too (-Wl,--wrap=_Znwm, the mangled
// operator new(size_t)). The malloc wrap alone does not see it, because the
// malloc call inside operator new lives in libstdc++, outside this link. Without
// this, a library path that uses `new` (as destroy() did in v3.6.0) would be
// neither counted nor fault-injected. Failure is reported the way operator new
// must report it: by throwing. Deallocation is untouched, so allocator pairs stay
// consistent under the sanitizer.
extern "C" void* __real__Znwm(size_t);

extern "C" void* __wrap__Znwm(size_t n) {
    if (g_armed) {
        ++g_calls;
        if (g_fail_at >= 0 && g_calls >= g_fail_at) throw std::bad_alloc();
    }
    return __real__Znwm(n);
}

static void arm(long fail_at)  { g_armed = true; g_calls = 0; g_fail_at = fail_at; }
static void disarm()           { g_armed = false; g_fail_at = -1; }

class DictionaryOOM : public ::testing::Test {};

static std::string key(int i) { return "k" + std::to_string(i); }

// Every allocation point of one insert: the call either fully succeeds or fails
// cleanly, and the dictionary stays usable.
TEST_F(DictionaryOOM, InsertSurvivesFailureAtEveryAllocationPoint) {
    for (long failPoint = 1; failPoint <= 4; ++failPoint) {
        Dictionary d;
        arm(failPoint);
        int8_t rc = d.set("some_key", "some_value");
        disarm();
        if (rc == DICTIONARY_OK) {
            EXPECT_EQ(d.count(), 1u) << "failPoint=" << failPoint;
            EXPECT_STREQ(d.getString("some_key").c_str(), "some_value");
        } else {
            EXPECT_EQ(rc, DICTIONARY_MEM) << "failPoint=" << failPoint;
            EXPECT_EQ(d.count(), 0u) << "failPoint=" << failPoint;
        }
        EXPECT_EQ(d.set("recovery", "ok"), DICTIONARY_OK) << "failPoint=" << failPoint;
        EXPECT_STREQ(d.getString("recovery").c_str(), "ok");
    }
}

// Entry allocation (1) or array growth (2) failing leaves prior entries intact.
TEST_F(DictionaryOOM, FailedInsertLeavesExistingEntriesIntact) {
    for (long failPoint = 1; failPoint <= 2; ++failPoint) {
        Dictionary d;                               // capacity 10, 20: the 21st insert grows
        for (int i = 0; i < 20; i++) ASSERT_EQ(d.set(key(i).c_str(), i), DICTIONARY_OK);
        arm(failPoint);
        int8_t rc = d.set("brand_new_key", "brand_new_value");
        disarm();
        EXPECT_EQ(rc, DICTIONARY_MEM) << "failPoint=" << failPoint;
        EXPECT_EQ(d.count(), 20u);
        EXPECT_FALSE(d.has("brand_new_key"));
        for (int i = 0; i < 20; i++) EXPECT_EQ(d.getInt(key(i).c_str(), -1), i);
    }
}

// Replacing with a larger value needs a new entry; if that fails the old value stays.
TEST_F(DictionaryOOM, FailedUpdateKeepsOldValue) {
    Dictionary d;
    d.set("k", "short");
    arm(1);
    int8_t rc = d.set("k", "a much longer replacement value");
    disarm();
    EXPECT_EQ(rc, DICTIONARY_MEM);
    EXPECT_STREQ(d.getString("k").c_str(), "short");
    EXPECT_EQ(d.count(), 1u);
}

// remove() never allocates, with or without the hash index.
TEST_F(DictionaryOOM, RemoveNeverAllocates) {
    Dictionary d;
    for (int i = 0; i < 100; i++) ASSERT_EQ(d.set(key(i).c_str(), i), DICTIONARY_OK);
    arm(1);
    int8_t rc = DICTIONARY_OK;
    for (int i = 0; i < 100; i += 2) rc |= d.remove(key(i).c_str());
    long calls = g_calls;
    disarm();
    EXPECT_EQ(rc, DICTIONARY_OK);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(d.count(), 50u);
    for (int i = 1; i < 100; i += 2) EXPECT_EQ(d.getInt(key(i).c_str(), -1), i);
}

// The hash index is an accelerator: if it cannot be allocated, inserts still
// succeed and lookups fall back to scanning.
TEST_F(DictionaryOOM, IndexFailureFallsBackToScanning) {
    for (long failPoint = 1; failPoint <= 3; ++failPoint) {
        Dictionary d;
        const int before = _DICT_INDEX_MIN > 1 ? _DICT_INDEX_MIN - 1 : 0;
        for (int i = 0; i < before; i++) ASSERT_EQ(d.set(key(i).c_str(), i), DICTIONARY_OK);
        arm(failPoint);                             // this insert reaches the index threshold
        int8_t rc = d.set(key(before).c_str(), before);
        disarm();
        int stored = before + (rc == DICTIONARY_OK ? 1 : 0);
        EXPECT_TRUE(rc == DICTIONARY_OK || rc == DICTIONARY_MEM) << "failPoint=" << failPoint;
        EXPECT_EQ((int)d.count(), stored);
        for (int i = 0; i < stored; i++) EXPECT_EQ(d.getInt(key(i).c_str(), -1), i) << "failPoint=" << failPoint;
        for (int i = stored; i < stored + 40; i++) ASSERT_EQ(d.set(key(i).c_str(), i), DICTIONARY_OK);
        for (int i = 0; i < stored + 40; i++) EXPECT_EQ(d.getInt(key(i).c_str(), -1), i);
    }
}

TEST_F(DictionaryOOM, ReserveFailureIsReported) {
    Dictionary d;
    d.set("a", 1);
    arm(1);
    int8_t rc = d.reserve(500);
    disarm();
    EXPECT_EQ(rc, DICTIONARY_MEM);
    EXPECT_EQ(d.getInt("a"), 1);
    EXPECT_EQ(d.set("b", 2), DICTIONARY_OK);
}

// jload stops at the failing insert; pairs loaded before it remain valid.
TEST_F(DictionaryOOM, JloadFailureMidway) {
    for (long failPoint = 1; failPoint <= 8; ++failPoint) {
        Dictionary d;
        arm(failPoint);
        int8_t rc = d.jload("{\"a\":\"1\",\"b\":\"2\",\"c\":\"3\",\"d\":\"4\",\"e\":\"5\"}");
        disarm();
        EXPECT_TRUE(rc == DICTIONARY_OK || rc == DICTIONARY_MEM) << "failPoint=" << failPoint;
        const char* keys[] = {"a", "b", "c", "d", "e"};
        for (size_t i = 0; i < d.count(); i++) EXPECT_STREQ(d.keyAt(i), keys[i]);
        if (rc == DICTIONARY_OK) { EXPECT_EQ(d.count(), 5u); }
        EXPECT_EQ(d.set("after", "ok"), DICTIONARY_OK);
    }
}

TEST_F(DictionaryOOM, MergeFailureMidway) {
    Dictionary src;
    for (int i = 0; i < 10; i++) src.set(key(i).c_str(), i);
    for (long failPoint = 1; failPoint <= 6; ++failPoint) {
        Dictionary d;
        arm(failPoint);
        int8_t rc = d.merge(src);
        disarm();
        EXPECT_TRUE(rc == DICTIONARY_OK || rc == DICTIONARY_MEM);
        for (size_t i = 0; i < d.count(); i++) EXPECT_EQ(d.getInt(d.keyAt(i), -1), (int)i);
    }
}

TEST_F(DictionaryOOM, ConstructionDoesNotAllocate) {
    long calls;
    arm(1);
    {
        Dictionary d;
        calls = g_calls;
    }
    disarm();
    EXPECT_EQ(calls, 0);
}

TEST_F(DictionaryOOM, DestroyAndMoveDoNotAllocate) {
    Dictionary d;
    for (int i = 0; i < 50; i++) ASSERT_EQ(d.set(key(i).c_str(), "v"), DICTIONARY_OK);
    arm(1);
    Dictionary m(std::move(d));
    d = std::move(m);
    d.destroy();
    long calls = g_calls;
    disarm();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(d.count(), 0u);
    EXPECT_EQ(d.set("after", "ok"), DICTIONARY_OK);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
