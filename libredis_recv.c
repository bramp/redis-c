#include "libredis.h"
#include "libredis_private.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>

#define STATE_WAITING         0 /** We are waiting for the first line of the reply */
#define STATE_READ_BULK       1 /** We reading a bulk reply                        */
#define STATE_READ_MULTI_BULK 2 /** We reading a multi-mulk reply                  */

static int state_waiting(struct RedisHandle * h);
static int state_read_bulk(struct RedisHandle * h);
static int state_read_multibulk(struct RedisHandle * h);

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
			h->linePos = 0;
			return ptr;
		}

		/* We can skip 1 if this character isn't a \r */
		if (*ptr != '\r')
			ptr+=2;
		else
			ptr++;
	}

	/* Record state of how far we got */
	h->linePos = ptr - buffer_start(&h->buf);

	return NULL;
}


/**
 * Parses a int from the line, ignoring the first character
 * @param line
 * @param num The parsed number
 * @return
 */
static int parse_int(const char *line, int *num) {
	assert(line != NULL);
	assert(num  != NULL);

	*num = atol(line);

	/* Is zero a valid number? Why does atol not tell us if a problem occured :( */

	return 0;
}


static int return_read_inline(struct RedisHandle * h, size_t len) {

	struct Reply * reply;
	struct Object *o;

	/* Store the result */
	reply = redis_reply_alloc(1);
	if (reply == NULL) {
		h->lastErr = "Error allocating a Reply struct";
		return -1;
	}

	o = redis_object_init_copy(&reply->argv[0], buffer_start(&h->buf), len);
	if (o == NULL) {
		redis_reply_free(reply);
		h->lastErr = "Error allocating a Object struct";
		return -1;
	}

	/* Shift this data off the buffer now */
	buffer_unshift(&h->buf, len);

	/* Push the reply onto the handle */
	redis_reply_temp_push(h, reply);
	redis_reply_push(h);

	return 0;
}

/**
 * @internal
 * We are waiting for commands
 * @param h
 * @return The number of more bytes we need
 */
static int state_waiting(struct RedisHandle * h) {
	const char * lineEnd;
	int num;

	assert(h->state == STATE_WAITING);

	/* We can't continue until a full line has been read */
	lineEnd = redis_readLine(h);
	if (lineEnd) {
		struct Reply  * reply;
		struct Object * o;

		const char *line = buffer_start(&h->buf);
		size_t len = lineEnd - line;

		switch (line[0]) {
			case '-': /* Error   */
			case '+': /* OK      */
			case ':': /* Integer */
				return return_read_inline(h, len);

			case '$': /* $N\r\n Keep reading for N bytes and then a \r\n */
				h->state = STATE_READ_BULK;
				if ( parse_int(line, &num) ) {
					h->lastErr = "Error parsing integer from reponse";
					return -1;
				}

				/* Create new reply (with one argument) */
				reply = redis_reply_alloc(1);
				if (reply == NULL) {
					h->lastErr = "Error allocating a Reply struct";
					return -1;
				}

				o = redis_object_init(&reply->argv[0], num);
				if (o == NULL) {
					redis_reply_free(reply);
					h->lastErr = "Error allocating a Object struct";
					return -1;
				}

				redis_reply_temp_push(h, reply);

				return state_read_bulk(h);

			case '*': /* *N\r\n Do N RECV_BULK */
				h->state = STATE_READ_MULTI_BULK;

				parse_int(line, &num);

				return state_read_multibulk(h);
				break;

			default:
				h->lastErr = "Error reading response, unknown reply";
				return -1;
		}
	}

	return UNKNOWN_READ_LENGTH;
}

static int state_read_bulk(struct RedisHandle * h) {

	struct Object * o = &h->reply[0].argv[0];
	size_t len = o->len;

	if (buffer_len(&h->buf) < len)
		return len - buffer_len(&h->buf);

	/* Copy the data into the reply */
	/* TODO Reduce the copies, by setting this reply as a buffer when we start to read the bulk */
	memcpy(o->ptr, buffer_start(&h->buf), len);

	/* Shift this data off the buffer now */
	buffer_unshift(&h->buf, len);

	/* and finally push this reply on */
	redis_reply_push(h);

	return 0;

}

static int state_read_multibulk(struct RedisHandle * h) {
	return 0;
}

/**
 *
 * @param h
 * @return How many replies are waiting, or -1 on error.
 */
int redis_read(struct RedisHandle * h) {
	int need = 0;

	if (h != NULL)
		return -1;

	if (h->socket == INVALID_SOCKET) {
		h->lastErr = "Invalid socket";
		return -1;
	}

	switch (h->state) {
		case STATE_WAITING:
			need = state_waiting(h);
			break;
		case STATE_READ_BULK:
			need = state_read_bulk(h);
			break;
		case STATE_READ_MULTI_BULK:
			need = state_read_multibulk(h);
			break;
	}

	/* If no more bytes are needed, we can revert back to the waiting state */
	if (need <= 0) {
		h->state = STATE_WAITING;
	} else {
		need = redis_readmore(h, need);
	}

	/* Did a error occur? */
	if (need < 0)
		return -1;

	/* Return how many replies are waiting */
	return h->replies;
}
