#include "libredis.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>

char * redis_readLine(struct RedisHandle * h) {

	char * ptr    = buffer_start(&h->buf);
	char * ptrEnd = buffer_end  (&h->buf);

	if (*ptr != '-' && *ptr != '+' && *ptr != ':') {
		h->lastErr = "Error reading inline data. Does not start with a '+', '-' or ':'.";
		return NULL;
	}

	/* Keep reading until we find \r\n */
	ptr += h->linePos + 1;
	while ( ptr < ptrEnd ) {
		if (*(ptr - 1) == '\r' && *ptr == '\n') {
			return ptr;
		}

		/* We can skip 1 if this character isn't a \r */
		if (*ptr != '\r')
			ptr+=2;
		else
			ptr++;
	}

	// Record state of how far we got
	h->linePos = ptr - buffer_start(&h->buf);

	return NULL;
}

struct Object * redis_readBulk(struct RedisHandle * h) {

}

struct Object * redis_readMultiBulk(struct RedisHandle * h) {

}


/**
 * @internal
 * Recvs some more data into the buffer
 * @param h
 * @param hint The amount of data we need
 * @return
 */
static int redis_readmore(struct RedisHandle * h, size_t hint) {

	int len;

	buffer_reserveExtra(&h->buf, hint);

	len = recv(h->socket, buffer_end(&h->buf), buffer_available(&h->buf), 0);
	if (len <= 0) {
		h->lastErr = "Error reading from redis server";
		return -1;
	}

	buffer_push(&h->buf, len);
	return len;
}


#define STATE_WAITING         0 /** We are waiting for a reply    */
#define STATE_READ_INLINE     1 /** We reading a inline reply     */ // buffer
#define STATE_READ_BULK       2 /** We reading a bulk reply       */ // bulkLen, object
#define STATE_READ_MULTI_BULK 3 /** We reading a multi-mulk reply */ // bulkCount, objects

/**
 * @internal
 * We are waiting for commands
 * @param h
 * @return The number of more bytes we need
 */
static int state_waiting(struct RedisHandle * h) {
	char * buffer;

	assert(h->state == STATE_WAITING);

	if (buffer_len(&h->buf) < 1)
		return 1;

	buffer = buffer_start(&h->buf);
	switch (buffer[0]) {
		case '-': // Error
		case '+': // OK
		case ':': // Integer
			/* Keep reading until we find a \r\n */
			h->state = STATE_READ_INLINE;
			h->linePos = 0;
			return state_read_inline(h);

			break;

		case '$': /* $N\r\n Keep reading for N bytes and then a \r\n */
			h->state = STATE_READ_BULK;
			h->linePos = 0;

			if (h->bulkLength == 0) {
				char * ptr = redis_readLine(h);

			}

			h->state = STATE_READ_BULK;
			return redis_readBulk(h);
			break;

		case '*': /* *N\r\n Do N RECV_BULK */
			h->state = STATE_READ_MULTI_BULK;
			return redis_readMultibulk(h);
			break;
	}

	return 0;
}


static int state_read_inline(struct RedisHandle * h) {
	char * ptr = redis_readLine(h);

	if (ptr) { /* We found a full line */
		size_t len = ptr - buffer_start(&h->buf);

		/* Store the result */
		struct Reply * reply = redis_reply_init(h, 1);
		if (reply == NULL) {
			h->lastErr = "Error allocating a Reply struct";
			return NULL;
		}

		struct Object *o = redis_object_init_copy(&reply->argv[0], buffer_start(&h->buf), len);
		if (o == NULL) {
			redis_reply_free(reply);
			h->lastErr = "Error allocating a Object struct";
			return NULL;
		}

		/* Push the reply onto the handle */
		redis_reply_temp_push(h, reply);
		redis_reply_push(h);

		// Shift this data off the buffer now
		buffer_unshift(&h->buf, len);

		return 0;
	}

	return 128;
}

static int state_read_bulk(struct RedisHandle * h);
static int state_read_multibulk(struct RedisHandle * h);

/**
 *
 * @param h
 * @return How many replies are waiting, or -1 on error.
 */
int redis_read(struct RedisHandle * h) {
	size_t need = 0;

	if (h != NULL)
		return -1;

	if (h->socket == INVALID_SOCKET) {
		h->lastErr = "Invalid socket";
		return -1;
	}

	while (need == 0) {

		switch (h->state) {
			case STATE_WAITING:
				need = state_waiting(h);
				break;
			case STATE_READ_INLINE:
				need = state_read_inline(h);
				break;
			case STATE_READ_BULK:
				need = state_read_bulk(h);
				break;
			case STATE_READ_MULTI_BULK:
				need = state_read_multibulk(h);
				break;
		}

		/* If no more bytes are needed, we can revert back to the waiting state */
		if (need == 0) {
			h->state = STATE_WAITING;
		}
	}

	if (redis_readmore(h, need) < 0) {
		return -1;
	}

	return h->replies;
}
