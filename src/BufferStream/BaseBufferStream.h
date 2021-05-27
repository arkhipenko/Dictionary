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

		using Print::write;

	protected:
		uint8_t *buff;
		size_t len;
};

#endif // __BaseBufferStream_H__
