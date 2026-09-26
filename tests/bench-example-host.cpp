// bench-example-host.cpp - builds examples/Dict_Benchmark against the host shim and
// runs it once. Keeps the sketch compiling and runs its result checks (counts,
// lookups, values, JSON round trip, removal) on every push. The exit code is the
// number of failed checks. Timings printed here are PC numbers, relative only.
#include "Arduino.h"
#include <cstdio>

// The part of the Arduino Serial API the sketch uses.
class HostSerial : public Print {
  public:
    void begin(unsigned long) {}
    size_t write(uint8_t c) override { return fputc(c, stdout) == EOF ? 0 : 1; }
    size_t write(const uint8_t* b, size_t n) override { return fwrite(b, 1, n, stdout); }

    size_t print(const char* s)                    { return fputs(s, stdout) < 0 ? 0 : strlen(s); }
    size_t print(const __FlashStringHelper* s)     { return print(reinterpret_cast<const char*>(s)); }
    size_t print(const String& s)                  { return print(s.c_str()); }
    size_t print(char c)                           { return write((uint8_t)c); }
    size_t print(int v)                            { return (size_t)printf("%d", v); }
    size_t print(unsigned int v)                   { return (size_t)printf("%u", v); }
    size_t print(long v)                           { return (size_t)printf("%ld", v); }
    size_t print(unsigned long v)                  { return (size_t)printf("%lu", v); }
    size_t print(double v, int digits = 2)         { return (size_t)printf("%.*f", digits, v); }

    template<class T> size_t println(const T& v)   { size_t n = print(v); return n + println(); }
    size_t println(double v, int digits)           { size_t n = print(v, digits); return n + println(); }
    size_t println()                               { return print("\n"); }
};

HostSerial Serial;

#include "../examples/Dict_Benchmark/Dict_Benchmark.ino"

int main() {
    setup();
    fflush(stdout);
    return failures;
}
