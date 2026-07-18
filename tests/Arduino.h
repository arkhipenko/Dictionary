// mock Arduino.h - host-side shim so the Dictionary library can be built and
// unit-tested natively (no microcontroller) under Google Test, mirroring the
// TaskScheduler test harness.
//
// Unlike TaskScheduler, Dictionary depends on the Arduino String class and on
// the Stream/Print base classes (used by src/BufferStream and by jload()), so
// this shim provides working, std::string-backed implementations of those.
#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <chrono>
#include <thread>

// ---- Arduino timing / misc primitives (only what tests might need) ----------
inline unsigned long millis() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}
inline unsigned long micros() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return (unsigned long)std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
}
inline void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
inline void yield() { std::this_thread::yield(); }

// ---- Print / Stream ---------------------------------------------------------
// Minimal, non-abstract bases matching what src/BufferStream expects. Concrete
// behavior is supplied by the BufferStream subclasses.
class Print {
  public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) { return 0; }
    virtual size_t write(const uint8_t* buf, size_t size) {
        size_t n = 0;
        while (size--) n += write(*buf++);
        return n;
    }
};

class Stream : public Print {
  public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual void flush() {}
};

// ---- String -----------------------------------------------------------------
// std::string-backed re-implementation of the subset of the Arduino String API
// that the Dictionary library and its tests use. Behavior matches Arduino for
// the text operations exercised here (concat, +=, replace, length, c_str, ==).
class String {
    std::string s;
  public:
    String() {}
    String(const char* c) : s(c ? c : "") {}
    String(const std::string& c) : s(c) {}
    String(char c) : s(1, c) {}
    String(int v)            : s(std::to_string(v)) {}
    String(long v)           : s(std::to_string(v)) {}
    String(unsigned int v)   : s(std::to_string(v)) {}
    String(unsigned long v)  : s(std::to_string(v)) {}
    String(float v)          : s(std::to_string(v)) {}
    String(double v)         : s(std::to_string(v)) {}

    const char* c_str() const { return s.c_str(); }
    unsigned length() const { return (unsigned)s.size(); }
    char charAt(unsigned i) const { return s[i]; }
    char operator[](unsigned i) const { return s[i]; }
    void reserve(unsigned) {}

    void concat(char c) { s.push_back(c); }
    void concat(const char* c) { s += c; }

    // Replace every occurrence of `from` with `to` (Arduino semantics).
    void replace(const char* from, const char* to) {
        std::string f(from), t(to);
        if (f.empty()) return;
        size_t pos = 0;
        while ((pos = s.find(f, pos)) != std::string::npos) {
            s.replace(pos, f.size(), t);
            pos += t.size();
        }
    }

    String& operator=(char c)          { s.assign(1, c); return *this; }
    String& operator=(const char* c)   { s.assign(c ? c : ""); return *this; }
    String& operator+=(const String& o){ s += o.s; return *this; }
    String& operator+=(const char* o)  { s += o; return *this; }
    String& operator+=(char c)         { s.push_back(c); return *this; }

    bool operator==(const String& o) const { return s == o.s; }
    bool operator==(const char* o) const { return s == std::string(o ? o : ""); }
    bool operator!=(const String& o) const { return s != o.s; }
    bool operator!=(const char* o) const { return !(*this == o); }

    String operator+(const String& o) const { String r(*this); r += o; return r; }
    String operator+(const char* o) const   { String r(*this); r += o; return r; }
    String operator+(char c) const           { String r(*this); r += c; return r; }

    // std::string accessor for test convenience.
    const std::string& str() const { return s; }
};

inline String operator+(char c, const String& b)         { String r(c); r += b; return r; }
inline String operator+(const char* c, const String& b)  { String r(c); r += b; return r; }

#endif // MOCK_ARDUINO_H
