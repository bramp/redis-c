#include "libredis_buffer.h"

#include <string.h>

struct Buffer * buffer_init(struct Buffer *buf, size_t size) {

	if (size == 0)
		size = 128;

	buf->buf = malloc(size);
	if (buf->buf == NULL)
		return NULL;

	buf->bufLen  = size;
	buf->data    = 0;
	buf->dataLen = 0;

	return buf;
}

void buffer_cleanup(struct Buffer *buf) {
	buffer_assert(buf);
	free(buf->buf);
}

struct Buffer * buffer_reserve(struct Buffer *buf, size_t size) {
	buffer_assert(buf);

	// We must always have at least 1 byte, otherwise realloc will free the pointer
	if (size == 0)
		size = 1;

	if (size > buf->bufLen) {
		// If the size is bigger than we can handle, just realloc
		buf->buf = realloc(buf->buf, size);
		if (buf->buf == NULL)
			return NULL;
		buf->bufLen = size;
	}

	if (size > (buf->bufLen - buf->data)) {
		// If the size is larger than our effective size (then move stuff down)
		memmove(buf->buf, &buf->buf[buf->data], buf->dataLen);
		buf->data = 0;
	}

	return buf;
}

struct Buffer * buffer_reserveExtra(struct Buffer *buf, size_t size) {
	return buffer_reserve(buf, size + buf->dataLen);
}

struct Buffer * buffer_shrink(struct Buffer *buf) {
	if (buf->data > 0) {
		/* Move all the data down (if needed) */
		memmove(buf->buf, &buf->buf[buf->data], buf->dataLen);
		buf->data = 0;
	}

	/* Shrink the malloced area */
	if (buf->bufLen > buf->dataLen) {
		buf->buf = realloc(buf->buf, buf->dataLen);
		if (buf->buf == NULL)
			return NULL;
		buf->bufLen = buf->dataLen;
	}

	return buf;
}
