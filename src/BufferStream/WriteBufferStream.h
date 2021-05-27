#ifndef __WriteBufferStream_H__
#define __WriteBufferStream_H__

#include "BaseBufferStream.h"

class WriteBufferStream : public BaseBufferStream {
	public:
		explicit WriteBufferStream(uint8_t *buff, size_t len);

	public:
		virtual size_t write(uint8_t value);

		using Print::write;
};

#endif
