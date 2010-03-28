/**
 * libRedis by Andrew Brampton 2010
 * A C library for the Redis server
 */
#include "libredis.h"

#include <assert.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#ifndef WIN32
// These make us more compatable with Windows, and I like them :)
# define INVALID_SOCKET -1
# define closesocket(x) close(x)
#endif

#define REDIS_TYPE_STR 0
#define REDIS_TYPE_RAW 1

struct Object {
	const char *ptr; // Pointer to raw/str data
	size_t len;      // The length of the data
	int type;        // What type of data is pointed to
	// Type
};

struct RedisHandle {
	SOCKET socket;
	const char *lastErr; // Keeps track of the last err
	int socketOwned :1;  // Did we create this socket?
};

#define REDIS_STR(x)     {x, strlen(x)}
#define REDIS_RAW(x,len) {x, len}

/* Ensures all the data is sent */
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

/* Saves a system call if we can easily put two buffers together
   TODO BENCHMARK THIS!
*/
static int sendObject(SOCKET s, const struct Object *obj, const char *extra, int flags) {
	char buf[1024];
	size_t extraLen = strlen(extra);

	assert(s     != INVALID_SOCKET);
	assert(obj   != NULL);
	assert(extra != NULL);

	if (obj->len + extraLen < sizeof(buf)) {
		// If we have room send in one buffer
		memcpy(buf, obj->ptr, obj->len);
		memcpy(buf + obj->len, extra, extraLen);
		return fullsend(s, buf, obj->len + extraLen, flags);
	} else {
		// Otherwise send in two buffers
		int ret1, ret2;
		ret1 = fullsend(s, obj->ptr, obj->len, flags);
		if (ret1 < 0) return ret1;

		ret2 = fullsend(s, extra, extraLen, flags);
		if (ret2 < 0) return ret2;

		return ret1 + ret2;
	}
}


/* Send a single object as a bulk */
static int sendSingleBulk(SOCKET s, const struct Object *obj, int printStar) {

	const char *fmt = printStar ? "*%ld\r\n" : "%ld\r\n";
	char lenString[16];

	assert(handle != NULL);
	assert(obj    != NULL);

	// Send the argument's length
	snprintf(lenString, sizeof(lenString), fmt, obj->len);
	if (fullsend(s, lenString, strlen(lenString), 0) < 0)
		return -1;

	// Sent the argument's data (followed by a newline)
	if (sendObject(s, obj, "\r\n", 0) < 0)
		return -1;

	return 1;
}

static inline int checkParameters(struct RedisHandle *handle, const int argc, const struct Object argv[]) {
	// Quick parameter check
	if (handle == NULL) {
		h->lastErr = "Error RedisHandle is null";
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
	return 0;
}

int redis_sendMultiBulk(struct RedisHandle *handle, const int argc, const struct Object argv[] ) {

	const struct Object *obj;
	const struct Object *last;

	if (checkParameters(handle, argc, argv))
		return -1;

	// Send the number of arguments
	snprintf(lenString, sizeof(lenString), "*%d\r\n", argc);
	if (fullsend(handle->socket, lenString, strlen(lenString), 0) < 0)
		return -1;

	// Now loop sending each argument
	obj  = &argv[0];
	last = &argv[argc];
	while (obj < last) {
		if (sendSingleBulk(handle, obj, 1) < 0)
			return -1;
		obj++;
	}
	return argc;
}

int redis_sendBulk(struct RedisHandle *h, const int argc, const struct Object argv[] ) {
	const struct Object *obj;
	const struct Object *last;
	char lenString[16];

	if (checkParameters(handle, argc, argv))
		return -1;

	// Check all but the last argument are not raw
	obj  = &argv[0];
	last = &argv[argc - 1];
	while (obj < last) {
		// We can't send RAW types in bulk, instead use multi-bulk
		if (obj->type == REDIS_TYPE_RAW) {
			h->lastErr = "Error sendBulk only allows the last argument to be TYPE_RAW";
			return -1;
		}
		obj++;
	}

	// Now loop sending all but the last argument
	obj  = &argv[0];
	last = &argv[argc - 1];
	while (obj < last) {
		if (sendcat(handle->socket, s, " ", 0) < 0) {
			h->lastErr = "Error sending argument";
			return -1;
		}
		obj++;
	}

	// For the last argument we send as bulk
	if (sendSingleBulk(handle, obj, 0) < 0) {
		h->lastErr = "Error sending bulk argument";
		return -1;
	}

	return 0;
}

/**
 * Creates a new handle to connect to a Redis server. This handle will be passed to most
 * of the API's functions. The handle must be freed once it is no longer needed by
 * {@link redis_cleanup}.
 *
 * @return A new {@link RedisHandle}.
 *
 * @see redis_cleanup
 */
struct RedisHandle * redis_create() {
	struct RedisHandle *h = malloc( sizeof(struct RedisHandle) );
	if (h) {
		h->socket = INVALID_SOCKET;
		h->socketOwned = 1;
		h->lastErr = NULL;
	}
	return h;
}

/**
 * Cleans up a RedisHandle releasing all resources and making it no longer valid.
 *
 * @param handle The handle to cleanup
 *
 * @see redis_create
 */
void redis_cleanup(struct RedisHandle * h) {
	if (h == NULL)
		return;

	if (h->socket != INVALID_SOCKET && h->socketOwned)
		closesocket(h->socket);

	free(h);
}

/**
 * Returns the last error to have occured on this handle.
 *
 * @return The last error as a string, or NULL if no error occured.
 */
const char * redis_error(struct RedisHandle * h) {
	return h->lastErr;
}

/**
 * Connects to a Redis Server
 *
 * @param host Server's hostname. If NULL localhost is used.
 * @param port Server's port. If 0 the default 6379 is used.
 *
 * @return 0 on success, -1 on failure.
 */
int redis_connect(struct RedisHandle * h, const char *host, unsigned short port) {

	if (host == NULL)
		host = "localhost";
	if (port == 0)
		port = 6379;

	h->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (h->socket == INVALID_SOCKET) {
		h->lastErr = "Error allocating a socket";
		return -1;
	}

	if (connect(sockets[i], target, target_len) == SOCKET_ERROR) {
		h->lastErr = "Error connecting to redis server";
		closesocket(h->socket);
		h->socket = INVALID_SOCKET;
		return -1;
	}

	h->lastErr = NULL;

	h->socketOwned = 1;
	return 0;
}

int redis_use_socket(SOCKET s) {
	h->socket = s;
	h->socketOwned = 0;
	return 0;
}

int main(int argc, char *argv[]) {

	struct RedisHandle *handle = redis_create();

	const struct Object args[] = {
		REDIS_STR("SET"),
		REDIS_STR("key"),
		REDIS_STR("value"),
	};

	redis_connect(handle, "localhost", 6379);

	redis_sendBulk(handle, 3, args);
	redis_sendMultiBulk(handle, 3, args);

	redis_cleanup(handle);

	return 0;
}
