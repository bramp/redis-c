#include "libredis_buffer.h"

#include <string.h>

static void buffer_assert(const struct Buffer *buf) {
	assert(buf != NULL);
	assert(buf->buf != NULL);
	assert(buf->data <= buf->bufLen);
	assert(buf->data + buf->dataLen <= buf->bufLen);
}

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

	/* We must always have at least 1 byte, otherwise realloc will free the pointer */
	if (size == 0)
		size = 1;

	if (size > buf->bufLen) {
		/* If the size is bigger than we can handle, just realloc */
		buf->buf = realloc(buf->buf, size);
		if (buf->buf == NULL)
			return NULL;
		buf->bufLen = size;
	}

	if (size > (buf->bufLen - buf->data)) {
		/* If the size is larger than our effective size (then move stuff down) */
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

char *buffer_start(const struct Buffer *buf) {
	buffer_assert(buf);
	return &buf->buf[buf->data];
}

char *buffer_end(const struct Buffer *buf) {
	buffer_assert(buf);
	return &buf->buf[buf->data + buf->dataLen];
}

size_t buffer_len(const struct Buffer *buf) {
	buffer_assert(buf);
	return buf->dataLen;
}

size_t buffer_available(const struct Buffer *buf) {
	buffer_assert(buf);
	return buf->bufLen - buf->data - buf->dataLen;
}

size_t buffer_push(struct Buffer *buf, size_t size) {
	if (buffer_available(buf) < size)
		size = buffer_available(buf);

	buf->dataLen += size;
	return size;
}

size_t buffer_pop(struct Buffer *buf, size_t size) {
	if (buffer_len(buf) < size)
		size = buffer_len(buf);

	buf->dataLen -= size;

	/* If we have no data we can easily move our pointer down */
	if (buf->dataLen == 0)
		buf->data = 0;

	return size;
}

size_t buffer_unshift(struct Buffer *buf, size_t size) {
	buffer_assert(buf);

	if (buffer_len(buf) < size)
		size = buffer_len(buf);

	buf->dataLen -= size;

	/* If we have no data we can easily move our pointer down */
	if (buf->dataLen == 0)
		buf->data = 0;
	else
		buf->data += size;

	return size;
}
