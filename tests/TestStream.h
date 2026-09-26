// TestStream.h - in-memory Stream and Print for the tests (BufferStream was
// removed from the library in 4.0; jload(Stream&) reads any Stream directly).
#ifndef TEST_STREAM_H
#define TEST_STREAM_H

#include "Arduino.h"
#include <string>

// Reads bytes from a std::string. read() returns -1 at the end.
class TestStream : public Stream {
    std::string data;
    size_t pos;
  public:
    explicit TestStream(const std::string& s) : data(s), pos(0) {}
    int available() override { return (int)(data.size() - pos); }
    int read() override { return pos < data.size() ? (uint8_t)data[pos++] : -1; }
    int peek() override { return pos < data.size() ? (uint8_t)data[pos] : -1; }
};

// Collects everything written to it.
class TestPrint : public Print {
  public:
    std::string out;
    size_t write(uint8_t c) override { out.push_back((char)c); return 1; }
    using Print::write;
};

#endif
