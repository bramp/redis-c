#ifndef LIBREDIS_BUFFER_H
#define LIBREDIS_BUFFER_H

#include <assert.h>
#include <stdlib.h>

struct Buffer {
	char *buf;     /** Beginning of the buffer */
	size_t bufLen; /** The size of buffer */

	size_t data;    /** Where is the beginning of the data */
	size_t dataLen; /** The length of the data */
};

/**
 * Initialises the passed in Buffer ensuring it holds at least size.
 * The buffer must later have {@link buffer_free} called on it.
 * @param buf The buffer to init. Could have been created on the stack, or previous malloced.
 *        If malloced it is the callers responsibility to free it.
 * @param size The minimum size the buffer should start with. If zero is given a default size is used.
 * @return NULL or failure, otherwise the buf parameter.
 */
struct Buffer * buffer_init(struct Buffer *buf, size_t size);

/**
 * Frees the buffer
 * @param buf
 */
void buffer_free(struct Buffer *buf);

/**
 * Ensure there is at least this much space overall in the buffer
 * @warning Memory may be realloced, so any pointers to the buffer must be invalidated afterwards.
 * @param buf
 * @param size Th
 * @return NULL or failure, otherwise the buf parameter.
 */
struct Buffer * buffer_reserve(struct Buffer *buf, size_t size);

/**
 * Ensure there is at least this much extra space (after the current data).
 * @warning Memory may be realloced, so any pointers to the buffer must be invalidated afterwards.
 * @param buf
 * @param size The amount of extra space needed
 * @return NULL or failure, otherwise the buf parameter.
 */
struct Buffer * buffer_reserveExtra(struct Buffer *buf, size_t size);

/**
 * Shrinks the buffer to contain just the data/
 * @warning Memory may be realloced, so any pointers to the buffer must be invalidated afterwards.
 * @param buf
 * @return NULL or failure, otherwise the buf parameter.
 */
struct Buffer * buffer_shrink(struct Buffer *buf);

/**
 * @internal
 * Asserts that all the buffer parameters make sense.
 * Useful for debugging.
 */
static void buffer_assert(const struct Buffer *buf) {
	assert(buf != NULL);
	assert(buf->buf != NULL);
	assert(buf->data <= buf->bufLen);
	assert(buf->data + buf->dataLen <= buf->bufLen);
}

/**
 * Returns the beginning of the data
 * @param buf
 * @return
 */
static char *buffer_start(const struct Buffer *buf) {
	buffer_assert(buf);
	return &buf->buf[buf->data];
}

/**
 * Returns the end of the data
 * @param buf
 * @return
 */
static char *buffer_end(const struct Buffer *buf) {
	buffer_assert(buf);
	return &buf->buf[buf->data + buf->dataLen];
}

/**
 * Returns the length of the data
 * @param buf
 * @return
 */
static size_t buffer_len(const struct Buffer *buf) {
	buffer_assert(buf);
	return buf->dataLen;
}

/**
 * How much space is free in the remaining buffer. If there is not
 * enough consider using {@link buffer_reserve}.
 * @param buf The buffer
 * @return how much free space
 */
static size_t buffer_available(const struct Buffer *buf) {
	buffer_assert(buf);
	return buf->bufLen - buf->data - buf->dataLen;
}

/**
 * Pushes size bytes onto the end of data. No data is actually copied, as
 * it is assumed the user has already used {@link buffer_reserve} and copied
 * the data in themselves.
 * @param buf
 * @param size
 * @return The number of bytes pushed, this may be smaller than size.
 */
static size_t buffer_push(struct Buffer *buf, size_t size) {
	if (buffer_available(buf) < size)
		size = buffer_available(buf);

	buf->dataLen += size;
	return size;
}

/**
 * Pops size bytes from the end of the data.
 * @param buf
 * @param size
 * @return The number of bytes popped, this may be smaller than size.
 */
static size_t buffer_pop(struct Buffer *buf, size_t size) {
	if (buffer_len(buf) < size)
		size = buffer_len(buf);

	buf->dataLen -= size;

	// If we have no data we can easily move our pointer down
	if (buf->dataLen == 0)
		buf->data = 0;

	return size;
}

/**
 * Remove size bytes from the beginning of the data.
 * @note There is no buffer_shift.
 * @param buf
 * @param size
 * @return The number of bytes unshifted, this may be smaller than size.
 */
static size_t buffer_unshift(struct Buffer *buf, size_t size) {
	buffer_assert(buf);

	if (buffer_len(buf) < size)
		size = buffer_len(buf);

	buf->dataLen -= size;

	// If we have no data we can easily move our pointer down
	if (buf->dataLen == 0)
		buf->data = 0;
	else
		buf->data += size;

	return size;
}

#endif /* LIBREDIS_BUFFER_H */
