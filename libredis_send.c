#include "libredis.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>

/**
 * @internal
 * Ensures all the data is sent. On Windows send may not send all the requested data,
 * I don't know what the case is on *nix.
 */
static int fullsend(SOCKET s, const char *buf, size_t len, int flags) {
	int remain;

	assert(s != INVALID_SOCKET);
	assert(buf != NULL || len == 0);

	remain = len;
	while (remain > 0) {
		int sent = send(s, buf, remain, flags);
		if (sent < 0)
			return sent;
		buf    += sent;
		remain -= sent;
	}

	return len;
}

/**
 * @internal
 * Sends a Object followed by the string in extra.
 * It concat the two to save a single system call.
 * @todo BENCHMARK THE COST OF SYSCAL VS MEMCPY!
 *
 * @return The number of bytes written
*/
static int send_object(struct RedisHandle *h, const struct Object *obj, const char *extra, int flags) {
	char buf[1024];
	size_t extraLen = strlen(extra);

	assert(h         != NULL);
	assert(h->socket != INVALID_SOCKET);
	assert(obj       != NULL);
	assert(extra     != NULL);

	if (obj->len + extraLen < sizeof(buf)) {
		/* If we have room send in one buffer */
		memcpy(buf, obj->ptr, obj->len);
		memcpy(buf + obj->len, extra, extraLen);
		return fullsend(h->socket, buf, obj->len + extraLen, flags);
	} else {
		/* Otherwise send in two buffers */
		int ret1, ret2;
		ret1 = fullsend(h->socket, obj->ptr, obj->len, flags);
		if (ret1 < 0) return ret1;

		ret2 = fullsend(h->socket, extra, extraLen, flags);
		if (ret2 < 0) return ret2;

		return ret1 + ret2;
	}
}


/**
 * @internal
 * Send a single object as a bulk (that is send the length then the data)
 */
static int send_single_bulk(struct RedisHandle *h, const struct Object *obj, int printStar) {

	const char *fmt = printStar ? "*%ld\r\n" : "%ld\r\n";
	char lenString[16];

	assert(h         != NULL);
	assert(h->socket != INVALID_SOCKET);
	assert(obj       != NULL);

	/* Send the argument's length */
	snprintf(lenString, sizeof(lenString), fmt, obj->len);
	if (fullsend(h->socket, lenString, strlen(lenString), 0) < 0)
		return -1;

	/* Sent the argument's data (followed by a newline) */
	if (send_object(h, obj, "\r\n", 0) < 0)
		return -1;

	return 1;
}

/**
 * @internal
 * Removes a bit of repeated code. Just checks if the arguments are valid
 * @param strings The number of arguments which must be strings
 */
static inline int check_send_parameters(struct RedisHandle *h, const int argc, const struct Object argv[], int strings) {

	const struct Object *obj;
	const struct Object *last;

	/* Quick parameter check */
	if (h == NULL)
		return -1;

	if (h->socket == INVALID_SOCKET) {
		h->lastErr = "Invalid socket";
		return -1;
	}

	if (argc == 0) {
		h->lastErr = "Error argc is zero";
		return -1;
	}

	if (argv == NULL) {
		h->lastErr = "Error argv is null";
		return -1;
	}

	/* Check that the first strings arguments are not raw */
	obj  = &argv[0];
	last = &argv[strings];
	while (obj < last) {
		/* We can't send RAW types in bulk, instead use multi-bulk */
		if (obj->type == REDIS_TYPE_RAW) {
			if (strings == argc) {
				h->lastErr = "Error none of the arguments are allowed to be TYPE_RAW, use sendBulk or sendMultiBulk instead";
			} else if (strings == (argc - 1)) {
				h->lastErr = "Error only allows the last argument is allowed to be TYPE_RAW, use sendMultiBulk instead";
			} else {
				h->lastErr = "Error argument is not allowed to be TYPE_RAW, use sendMultiBulk instead";
			}
			return -1;
		}
		obj++;
	}

	return 0;
}

int redis_send_multibulk(struct RedisHandle *handle, const int argc, const struct Object argv[] ) {
	const struct Object *obj;
	const struct Object *last;
	char lenString[16];

	if (check_send_parameters(handle, argc, argv, 0))
		return -1;

	/* Send the number of arguments */
	snprintf(lenString, sizeof(lenString), "*%d\r\n", argc);
	if (fullsend(handle->socket, lenString, strlen(lenString), 0) < 0)
		return -1;

	/* Now loop sending each argument */
	obj  = &argv[0];
	last = &argv[argc];
	while (obj < last) {
		if (send_single_bulk(handle, obj, 1) < 0)
			return -1;
		obj++;
	}
	return argc;
}

int redis_send_bulk(struct RedisHandle *handle, const int argc, const struct Object argv[] ) {
	const struct Object *obj;
	const struct Object *last;

	if (check_send_parameters(handle, argc, argv, argc - 1))
		return -1;

	/* Now loop sending all but the last argument */
	obj  = &argv[0];
	last = &argv[argc - 1];
	while (obj < last) {
		if (send_object(handle, obj, " ", 0) < 0) {
			handle->lastErr = "Error sending argument";
			return -1;
		}
		obj++;
	}

	/* For the last argument we send as bulk */
	if (send_single_bulk(handle, obj, 0) < 0) {
		handle->lastErr = "Error sending bulk argument";
		return -1;
	}

	return 0;
}

int redis_send(struct RedisHandle *handle, const int argc, const struct Object argv[] ) {
	const struct Object *obj;
	const struct Object *last;

	if (check_send_parameters(handle, argc, argv, argc))
		return -1;

	/* Now loop sending all arguments */
	obj  = &argv[0];
	last = &argv[argc];
	while (obj < last) {
		if (send_object(handle, obj, " ", 0) < 0) {
			handle->lastErr = "Error sending argument";
			return -1;
		}
		obj++;
	}

	return 0;
}
