#ifndef __BaseBufferStream_H__
#define __BaseBufferStream_H__

#include <Arduino.h>

class BaseBufferStream : public Stream {
	public:
		explicit BaseBufferStream(uint8_t *buff, size_t len);

	public:
		virtual size_t write(uint8_t value) { return 0; }
		virtual int available() { return 0; }
		virtual int read() { return -1; }
		virtual int peek() { return -1; }
		virtual void flush() {}

    // inline uint8_t* bufPtr() { return buff; }
    // inline size_t   size() { return sz; }
		using Print::write;

	public:
		uint8_t *buff;
		size_t len;
		size_t sz;
};

#endif // __BaseBufferStream_H__
