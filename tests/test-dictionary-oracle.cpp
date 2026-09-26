// test-dictionary-oracle.cpp - differential test against a reference model:
// a std::map of key -> rendered value plus a vector holding insertion order.
// Random set (new and update, several types), remove and occasional destroy;
// after every step count, lookups, positional order and json() must agree.
// Built with the hash index off (_DICT_INDEX_MIN=0), always on (=1), at the default
// threshold, and with _DICT_COMPRESS.
#include <gtest/gtest.h>
#include "Arduino.h"
#include "Dictionary.h"

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace {

struct Model {
    std::map<std::string, std::string> kv;
    std::vector<std::string> order;

    void set(const std::string& k, const std::string& v) {
        if (!kv.count(k)) order.push_back(k);
        kv[k] = v;
    }
    void remove(const std::string& k) {
        if (!kv.erase(k)) return;
        order.erase(std::find(order.begin(), order.end(), k));
    }
    void clear() { kv.clear(); order.clear(); }
};

std::string randomText(std::mt19937& rng) {
    static const char* words[] = {"alpha", "beta", "http://", "home", "the ", "sensor", "x", "",
                                  "192.168.1.", "value", "\xC3\xA9t\xC3\xA9", "a\"b", "back\\slash", "tab\t"};
    std::string s;
    int n = (int)(rng() % 5);
    for (int i = 0; i < n; i++) s += words[rng() % (sizeof(words) / sizeof(words[0]))];
    if (s.size() > _DICT_VALLEN) s.resize(_DICT_VALLEN);
    return s;
}

void check(const Dictionary& d, const Model& m, int step) {
    ASSERT_EQ(d.count(), m.order.size()) << "step " << step;
    for (size_t i = 0; i < m.order.size(); i++) {
        ASSERT_STREQ(d.keyAt(i), m.order[i].c_str()) << "step " << step << " pos " << i;
        ASSERT_EQ(std::string(d.value(i).c_str()), m.kv.at(m.order[i])) << "step " << step;
    }
    for (const auto& p : m.kv) {
        ASSERT_TRUE(d.has(p.first.c_str())) << "step " << step << " key " << p.first;
        ASSERT_EQ(std::string(d.getString(p.first.c_str()).c_str()), p.second) << "step " << step;
    }
}

} // namespace

TEST(DictionaryOracle, RandomOperationsMatchModel) {
    for (unsigned seed = 1; seed <= 8; seed++) {
        std::mt19937 rng(seed);
        Dictionary d;
        Model m;
        const int keySpace = 20 + (int)(seed * 40);    // up to 340 keys: crosses the index threshold
        for (int step = 0; step < 3000; step++) {
            std::string k = "k" + std::to_string(rng() % keySpace);
            unsigned op = rng() % 100;
            if (op < 45) {
                std::string v = randomText(rng);
                ASSERT_EQ(d.set(k.c_str(), v.c_str()), DICTIONARY_OK);
                m.set(k, v);
            }
            else if (op < 60) {
                int v = (int)(rng() % 200000) - 100000;
                ASSERT_EQ(d.set(k.c_str(), v), DICTIONARY_OK);
                m.set(k, std::to_string(v));
            }
            else if (op < 65) {
                bool v = rng() & 1;
                ASSERT_EQ(d.set(k.c_str(), v), DICTIONARY_OK);
                m.set(k, v ? "true" : "false");
            }
            else if (op < 99) {
                ASSERT_EQ(d.remove(k.c_str()), DICTIONARY_OK);
                m.remove(k);
            }
            else {
                d.destroy();
                m.clear();
            }
            if (step % 50 == 0 || step > 2950) check(d, m, step);
            if (::testing::Test::HasFatalFailure()) return;
        }
        check(d, m, -1);

        // json() -> jload() reproduces the same pairs in the same order
        Dictionary e;
        ASSERT_EQ(e.jload(d.json()), DICTIONARY_OK);
        ASSERT_EQ(e.count(), d.count());
        for (size_t i = 0; i < d.count(); i++) {
            ASSERT_STREQ(e.keyAt(i), d.keyAt(i));
            ASSERT_EQ(std::string(e.value(i).c_str()), std::string(d.value(i).c_str()));
        }
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
