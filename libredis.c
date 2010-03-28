/**
 * libRedis by Andrew Brampton 2010
 * A C library for the Redis server
 */
#include "libredis.h"

#include <assert.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef WIN32
// These make us more compatable with Windows, and I like them :)
# define INVALID_SOCKET -1
# define SOCKET_ERROR -1
# define closesocket(x) close(x)
#endif

#define REDIS_TYPE_STR 0
#define REDIS_TYPE_RAW 1

struct Object {
	const char *ptr; /// Pointer to raw/str data
	size_t len;      /// The length of the data
	int type;        /// What type of data is pointed to
};

struct RedisHandle {
	SOCKET socket;
	const char *lastErr; // Keeps track of the last err
	unsigned int socketOwned :1;  // Did we create this socket?
};

#define REDIS_STR(x)     {x, strlen(x), REDIS_TYPE_STR}
#define REDIS_RAW(x,len) {x, len,       REDIS_TYPE_RAW}

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
static int sendObject(struct RedisHandle *h, const struct Object *obj, const char *extra, int flags) {
	char buf[1024];
	size_t extraLen = strlen(extra);

	assert(h         != NULL);
	assert(h->socket != INVALID_SOCKET);
	assert(obj       != NULL);
	assert(extra     != NULL);

	if (obj->len + extraLen < sizeof(buf)) {
		// If we have room send in one buffer
		memcpy(buf, obj->ptr, obj->len);
		memcpy(buf + obj->len, extra, extraLen);
		return fullsend(h->socket, buf, obj->len + extraLen, flags);
	} else {
		// Otherwise send in two buffers
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
static int sendSingleBulk(struct RedisHandle *h, const struct Object *obj, int printStar) {

	const char *fmt = printStar ? "*%ld\r\n" : "%ld\r\n";
	char lenString[16];

	assert(h         != NULL);
	assert(h->socket != INVALID_SOCKET);
	assert(obj       != NULL);

	// Send the argument's length
	snprintf(lenString, sizeof(lenString), fmt, obj->len);
	if (fullsend(h->socket, lenString, strlen(lenString), 0) < 0)
		return -1;

	// Sent the argument's data (followed by a newline)
	if (sendObject(h, obj, "\r\n", 0) < 0)
		return -1;

	return 1;
}

/**
 * @internal
 * Removes a bit of repeated code. Just checks if the arguments are valid
 * @param strings The number of arguments which must be strings
 */
static inline int checkSendParameters(struct RedisHandle *h, const int argc, const struct Object argv[], int strings) {

	const struct Object *obj;
	const struct Object *last;

	// Quick parameter check
	if (h == NULL) {
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

	// Check that the first strings arguments are not raw
	obj  = &argv[0];
	last = &argv[strings];
	while (obj < last) {
		// We can't send RAW types in bulk, instead use multi-bulk
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

/**
 * Sends a multi bulk encoded command to a Redis server. All arguments may be of  any type.
 * This is not as effecient as @link{redis_send} or @link{redis_sendBulk}, but it does allow
 * @link{REDIS_TYPE_RAW} keys and values to be used as any argument.
 *
 * @param h
 * @param argc The number of arguments stored in argv.
 * @param argv An array of @link{Object}s to be sent in the command.
 *
 * @return 0 on success, -1 on failure. Use {@link redis_error} to determine the error
 */
int redis_sendMultiBulk(struct RedisHandle *handle, const int argc, const struct Object argv[] ) {
	const struct Object *obj;
	const struct Object *last;
	char lenString[16];

	if (checkSendParameters(handle, argc, argv, 0))
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

/**
 * Sends a bulk encoded command to a Redis server. All but the last argument of a bulk
 * comamnd must be @link{REDIS_TYPE_STR}, whereas the last argument may be
 * @link{REDIS_TYPE_RAW}.
 *
 * @param h
 * @param argc The number of arguments stored in argv.
 * @param argv An array of @link{Object}s to be sent in the command.
 *
 * @return 0 on success, -1 on failure. Use {@link redis_error} to determine the error
 */
int redis_sendBulk(struct RedisHandle *handle, const int argc, const struct Object argv[] ) {
	const struct Object *obj;
	const struct Object *last;

	if (checkSendParameters(handle, argc, argv, argc - 1))
		return -1;

	// Now loop sending all but the last argument
	obj  = &argv[0];
	last = &argv[argc - 1];
	while (obj < last) {
		if (sendObject(handle, obj, " ", 0) < 0) {
			handle->lastErr = "Error sending argument";
			return -1;
		}
		obj++;
	}

	// For the last argument we send as bulk
	if (sendSingleBulk(handle, obj, 0) < 0) {
		handle->lastErr = "Error sending bulk argument";
		return -1;
	}

	return 0;
}

/**
 * Sends a bulk encoded command to a Redis server. All but the last argument of a bulk
 * comamnd must be @link{REDIS_TYPE_STR}, whereas the last argument may be
 * @link{REDIS_TYPE_RAW}.
 *
 * @param h
 * @param argc The number of arguments stored in argv.
 * @param argv An array of @link{Object}s to be sent in the command.
 *
 * @return 0 on success, -1 on failure. Use {@link redis_error} to determine the error
 */
int redis_send(struct RedisHandle *handle, const int argc, const struct Object argv[] ) {
	const struct Object *obj;
	const struct Object *last;

	if (checkSendParameters(handle, argc, argv, argc))
		return -1;

	// Now loop sending all arguments
	obj  = &argv[0];
	last = &argv[argc];
	while (obj < last) {
		if (sendObject(handle, obj, " ", 0) < 0) {
			handle->lastErr = "Error sending argument";
			return -1;
		}
		obj++;
	}

	return 0;
}

/**
 * Creates a new handle to connect to a Redis server. This handle will be passed to most
 * of the API's functions. The handle must be freed once it is no longer needed by
 * {@link redis_cleanup}.
 *
 * @return A new {@link RedisHandle}, or NULL if a error occured.
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
 * Connects to a Redis Server.
 *
 * @param host Server's hostname. If NULL localhost is used.
 * @param port Server's port. If 0 the default 6379 is used.
 *
 * @return 0 on success, -1 on failure. Use {@link redis_error} to determine the error
 */
int redis_connect(struct RedisHandle * h, const char *host, unsigned short port) {
	struct addrinfo *aiList;
	struct addrinfo *ai;

	struct addrinfo hint;
	char portStr[8];
	int ret = -1;

	if (host == NULL)
		host = "localhost";

	if (port == 0)
		port = 6379;

	itoa(port, portStr, 10);

	// Create the hint for getaddrinfo (we want AF_INET or AF_INET6, but it has to be
	// a TCP SOCK_STREAM connection)
	memset( &hint, 0, sizeof(hint) );
	hint->ai_family   = PF_UNSPEC;
	hint->ai_socktype = SOCK_STREAM;

	// Lookup the hostname
	if ( getaddrinfo(host, portStr, &hint, &aiList) ) {
		h->lastErr = "Error resolving hostname";
		return -1;
	}

	for (ai = aiList; ai != NULL; ai = ai->ai_next) {
		h->socket = socket(ai->ai_family, ai->ai_socktype, a->ai_protocol);
		if (h->socket == INVALID_SOCKET) {
			h->lastErr = "Error allocating socket";
			goto cleanup;
		}

		if (connect(h->socket, target, target_len) == 0) {
			// If connecting was OK, we bail out
			h->lastErr = NULL;
			h->socketOwned = 1;

			ret = 0;
			goto cleanup;
		}

		// We cleanup the socket because the next ai_family might be different
		closesocket(h->socket);
		h->socket = INVALID_SOCKET;
	}

	h->lastErr = "Error connecting to redis server";

cleanup:
	freeaddrinfo( aiList );
	return ret;
}

int redis_use_socket(struct RedisHandle * h, SOCKET s) {
	if (h->socket != INVALID_SOCKET)
		closesocket(h->socket);
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
