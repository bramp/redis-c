
struct Buffer {
	char *buf;     /// Beginning of the buffer
	size_t bufLen; /// The size of buffer

	size_t data;    /// Where is the beginning of the data
	size_t dataLen; /// The length of the data
};

/**
 * Initialises the passed in Buffer
 * @param buf
 * @return NULL or failure, otherwise the buf parameter.
 */
struct Buffer * buffer_init(struct Buffer *buf) {
	return buffer_init(buf, 128);
}

/**
 * Initialises the passed in Buffer ensuring it holds at least size.
 * @param buf
 * @param size The minimum size the buffer should start with
 * @return NULL or failure, otherwise the buf parameter.
 */
struct Buffer * buffer_init(struct Buffer *buf, size_t size) {
	buf->buf = malloc(size);
	if (buf->buf == NULL)
		return NULL;

	buf->bufLen  = size;
	buf->data    = 0;
	buf->dataLen = 0;
}

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
 * Ensure there is at least this much space overall in the buffer
 * @warning Memory may be realloced, so any pointers to the buffer must be invalidated afterwards.
 * @param buf
 * @param size
 * @return NULL or failure, otherwise the buf parameter.
 */
struct Buffer * buffer_reserve(struct Buffer *buf, size_t size) {
	buffer_assert(buf);

	if (size > buf->bufLen) {
		// If the size is bigger than we can handle, just realloc
		buf->buf = realloc(buf->buf, size);
		if (buf->buf == NULL)
			return NULL;
		buf->bufLen = size;
	}

	if (size > (buf->bufLen - buf->data)) {
		// If the size is larger than our effective size (then move stuff down)
		size_t len = buf->dataLen;

		memmove(buf->buf, &buf->buf[buf->data], len);
		buf->data    -= len;
		buf->dataLen -= len;
	}

	return buf;
}

/**
 * Ensure there is at least this much extra space (after the current data).
 * @warning Memory may be realloced, so any pointers to the buffer must be invalidated afterwards.
 * @param buf
 * @param size The amount of extra space needed
 */
void buffer_reserveExtra(struct Buffer *buf, size_t size) {
	buffer_reserve(size + buffer_len(buf));
}

/***
 * Shrinks the buffer to contain just the data
 */
void buffer_shrink(struct Buffer *buf) {

}

/**
 * Returns the beginning of the data
 * @param buf
 * @return
 */
char *buffer_start(const struct Buffer *buf) {
	buffer_assert(buf);
	return &buf->buf[buf->data];
}

/**
 * Returns the end of the data
 * @param buf
 * @return
 */
char *buffer_end(const struct Buffer *buf) {
	buffer_assert(buf);
	return &buf->buf[buf->data + buf->dataLen];
}

/**
 * Returns the length of the data
 * @param buf
 * @return
 */
size_t buffer_len(const struct Buffer *buf) {
	buffer_assert(buf);
	return buf->dataLen;
}

/**
 * How much space is free in the remaining buffer. If there is not
 * enough consider using {@link buffer_reserve}.
 * @param buf The buffer
 * @return how much free space
 */
size_t buffer_available(const struct Buffer *buf) {
	buffer_assert(buf);
	return buf->bufLen - buf->data - buf->dataLen;
}
