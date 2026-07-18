// test-dictionary-oom.cpp - out-of-memory safety, the core of the v3.6.0 fixes.
//
// malloc() is intercepted via the linker's --wrap so we can force an allocation
// failure at a precise point and assert that the library (a) never crashes,
// (b) returns an error code, and (c) leaves existing data intact. Run this suite
// under AddressSanitizer to also catch any invalid free / use-after-free on the
// failure paths (that was the original create()/delete() bug).
//
// Link with: -Wl,--wrap=malloc
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"

#include <string>

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

static void arm(long fail_at)  { g_armed = true; g_calls = 0; g_fail_at = fail_at; }
static void disarm()           { g_armed = false; g_fail_at = -1; }

class DictionaryOOM : public ::testing::Test {};

// Fail at every allocation point of a single insert. Whatever the failure point,
// the call must either fully succeed or cleanly fail - never crash or corrupt.
// (Before the fix, failing the value buffer left valbuf uninitialized and the
// subsequent delete freed a garbage pointer.)
TEST_F(DictionaryOOM, InsertSurvivesFailureAtEveryAllocationPoint) {
    for (long failPoint = 1; failPoint <= 10; ++failPoint) {
        Dictionary d;
        arm(failPoint);
        int8_t rc = d.insert("some_key", "some_value");
        disarm();

        if (rc == DICTIONARY_OK) {
            EXPECT_EQ(d.count(), 1u) << "failPoint=" << failPoint;
            EXPECT_STREQ(d.search("some_key").c_str(), "some_value");
        } else {
            EXPECT_LT(rc, 0) << "failPoint=" << failPoint;   // negative error code
            EXPECT_EQ(d.count(), 0u) << "failPoint=" << failPoint;
        }

        // The dictionary must remain fully usable after a failed insert.
        EXPECT_EQ(d.insert("recovery", "ok"), DICTIONARY_OK) << "failPoint=" << failPoint;
        EXPECT_STREQ(d.search("recovery").c_str(), "ok");
    }
}

// A failed insert into an existing dictionary must not disturb prior entries.
TEST_F(DictionaryOOM, FailedInsertLeavesExistingEntriesIntact) {
    Dictionary d;
    for (int i = 0; i < 20; i++)
        ASSERT_EQ(d.insert(("k" + std::to_string(i)).c_str(),
                           ("v" + std::to_string(i)).c_str()), DICTIONARY_OK);

    arm(1);   // fail the very next allocation
    int8_t rc = d.insert("brand_new_key", "brand_new_value");
    disarm();

    EXPECT_LT(rc, 0);
    EXPECT_EQ(d.count(), 20u);
    for (int i = 0; i < 20; i++)
        EXPECT_STREQ(d.search(("k" + std::to_string(i)).c_str()).c_str(),
                     ("v" + std::to_string(i)).c_str());
    EXPECT_STREQ(d.search("brand_new_key").c_str(), "");
}

// Two-child delete promotes the in-order successor by copying its (longer)
// value into the victim node - which needs an allocation. If that fails the
// remove must be atomic: report the error and leave the tree exactly as it was.
TEST_F(DictionaryOOM, AtomicTwoChildDeleteOnAllocationFailure) {
    Dictionary d;
    d("b", "x");                                   // root, short value
    d("a", "left");
    d("c", "a very long successor value here");    // right; promotes into "b"

    arm(1);   // the first alloc during remove is the successor value buffer
    int8_t rc = d.remove("b");
    disarm();

    EXPECT_LT(rc, 0) << "remove should report the allocation failure";
    // Tree unchanged: all three keys still present with original values.
    EXPECT_EQ(d.count(), 3u);
    EXPECT_STREQ(d["a"].c_str(), "left");
    EXPECT_STREQ(d["b"].c_str(), "x");
    EXPECT_STREQ(d["c"].c_str(), "a very long successor value here");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
