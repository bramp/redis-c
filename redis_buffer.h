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
 * The buffer must later have #buffer_cleanup called on it.
 *
 * @param buf The buffer to init. Could have been created on the stack, or previous malloced.
 *        If malloced it is the callers responsibility to free it.
 * @param size The minimum size the buffer should start with. If zero is given a default size is used.
 *
 * @return NULL on failure.
 * @return Otherwise the buf parameter.
 */
struct Buffer * buffer_init(struct Buffer *buf, size_t size);

/**
 * Frees the buffer
 * @param buf
 */
void buffer_cleanup(struct Buffer *buf);

/**
 * Ensure there is at least this much space overall in the buffer
 *
 * @warning Memory may be realloced, so any pointers to the buffer must be invalidated afterwards.
 *
 * @param buf
 * @param size
 *
 * @return NULL on failure.
 * @return Otherwise the buf parameter.
 */
struct Buffer * buffer_reserve(struct Buffer *buf, size_t size);

/**
 * Ensure there is at least this much extra space (after the current data).
 *
 * @warning Memory may be realloced, so any pointers to the buffer must be invalidated afterwards.
 *
 * @param buf
 * @param size The amount of extra space needed
 *
 * @return NULL on failure.
 * @return Otherwise the buf parameter.
 */
struct Buffer * buffer_reserveExtra(struct Buffer *buf, size_t size);

/**
 * Shrinks the buffer to contain just the data/
 *
 * @warning Memory may be realloced, so any pointers to the buffer must be invalidated afterwards.
 *
 * @param buf
 *
 * @return NULL on failure.
 * @return Otherwise the buf parameter.
 */
struct Buffer * buffer_shrink(struct Buffer *buf);

/**
 * Returns the beginning of the data
 *
 * @param buf
 *
 * @return The beginning of the data
 */
char *buffer_start(const struct Buffer *buf);

/**
 * Returns the end of the data
 *
 * @param buf
 *
 * @return The end of the data
 */
char *buffer_end(const struct Buffer *buf);

/**
 * Returns the length of the data
 *
 * @param buf
 *
 * @return The length of the data
 */
size_t buffer_len(const struct Buffer *buf);

/**
 * How much space is free in the remaining buffer. If there is not
 * enough consider using #buffer_reserve.
 *
 * @param buf The buffer
 *
 * @return The amount of free space after the data
 */
size_t buffer_available(const struct Buffer *buf);

/**
 * Pushes size bytes onto the end of data. No data is actually copied, as
 * it is assumed the user has already used #buffer_reserve and copied
 * the data in themselves.
 *
 * @param buf
 * @param size
 *
 * @return The number of bytes pushed, this may be smaller than size.
 */
size_t buffer_push(struct Buffer *buf, size_t size);

/**
 * Pops size bytes from the end of the data.
 *
 * @param buf
 * @param size
 *
 * @return The number of bytes popped, this may be smaller than size.
 */
size_t buffer_pop(struct Buffer *buf, size_t size);

/**
 * Remove size bytes from the beginning of the data.
 *
 * @note There is no buffer_shift.
 *
 * @param buf
 * @param size
 *
 * @return The number of bytes unshifted, this may be smaller than size.
 */
size_t buffer_unshift(struct Buffer *buf, size_t size);

#endif /* LIBREDIS_BUFFER_H */
