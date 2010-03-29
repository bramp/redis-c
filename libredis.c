/**
 * libRedis by Andrew Brampton 2010
 * A C library for the Redis server
 */
#include "libredis.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef WIN32
// These make us more compatable with Windows, and I like them :)
# define INVALID_SOCKET -1
# define SOCKET_ERROR -1
# define closesocket(x) close(x)
#endif

#define REDIS_TYPE_UNKNOWN 0
#define REDIS_TYPE_STR 1
#define REDIS_TYPE_RAW 2
#define REDIS_TYPE_INT 3

struct Object {
	char *ptr;                 /// Pointer to raw/str data
	size_t len;                /// The length of the data
	unsigned int type     :2; /// What type of data is pointed to
	unsigned int ptrOwned :1; /// Should we free the ptr?
};

struct RedisHandle {
	SOCKET socket;
	const char *lastErr;          // Keeps track of the last err
//	char * recvBuffer;            // Recv buffer
	unsigned int socketOwned :1; // Did we create this socket?
};

#define REDIS_STR(x)     {(char *)(x), strlen(x), REDIS_TYPE_STR}
#define REDIS_RAW(x,len) {(char *)(x), (len),     REDIS_TYPE_RAW}
#define REDIS_INT(x)     {(char *)(x), 0,         REDIS_TYPE_INT}

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
 * {@link redis_free}.
 *
 * @return A new {@link RedisHandle}, or NULL if a error occured.
 *
 * @see redis_free
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
 * @param handle The handle to free
 *
 * @see redis_create
 */
void redis_free(struct RedisHandle * h) {
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

	snprintf(portStr, sizeof(portStr), "%hu", port);

	// Create the hint for getaddrinfo (we want AF_INET or AF_INET6, but it has to be
	// a TCP SOCK_STREAM connection)
	memset( &hint, 0, sizeof(hint) );
	hint.ai_family   = PF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;

	// Lookup the hostname
	if ( getaddrinfo(host, portStr, &hint, &aiList) ) {
		h->lastErr = "Error resolving hostname";
		return -1;
	}

	for (ai = aiList; ai != NULL; ai = ai->ai_next) {
		h->socket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (h->socket == INVALID_SOCKET) {
			h->lastErr = "Error allocating socket";
			goto cleanup;
		}

		if (connect(h->socket, ai->ai_addr, ai->ai_addrlen) == 0) {
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

Object * redis_readInline(struct RedisHandle * h, Object *object, size_t &objectMaxLen) {

	char * result;
	char * ptr    = object->ptr;
	char * ptrEnd = ptr + objectMaxLen;

	if (*ptr != '-' && *ptr != '+') {
		h->lastErr = "Error reading inline data. Does not start with a '+' or '-'.";
		return NULL;
	}

	// Keep reading until we find \r\n
	ptr++;
	while ( ptr < ptrEnd ) {
		if (*(ptr - 1) == '\r' && *ptr == '\n') {
			
			return object;
		}

		// We can skip 1 if this character isn't a \r
		if (*ptr == '\r')
			ptr++;
		else
			ptr+=2;
	}
}

Object * redis_object_init() {
	Object *o = malloc( sizeof(*o) );
	if (o == NULL)
		return NULL;

	o->ptr = NULL;
	o->len = 0;
	o->type = REDIS_TYPE_UNKNOWN;
	o->ptrOwned = 0;
	return o;
}

Object * redis_object_init(size_t buflen) {
	Object *o = malloc( sizeof(*o) );
	if (o == NULL)
		return NULL;

	o->ptr = malloc(buflen);
	if (o->ptr == NULL) {
		free(o);
		return NULL;
	}
	o->len = buflen;
	o->type = REDIS_TYPE_RAW;
	o->ptrOwned = 1;
	return o;
}

void redis_object_free( Object * o ) {
	if (o == NULL)
		return;

	if (o->ptrOwned)
		free(o->ptr);
	free(o);
}

int redis_read(struct RedisHandle * h) {
	int len;
	Object * o;

	if (h != NULL)
		return -1;

	if (h->socket == INVALID_SOCKET) {
		h->lastErr = "Invalid socket";
		return -1;
	}

	o-

	len = recv(h->socket, buffer, sizeof(buffer), 0);
	if (len <= 0) {
		h->lastErr = "Error reading from redis server";
		return -1;
	}

	int type = RECV_INLINE RECV_BULK RECV_MULTI_BULK RECV_INTEGER

	switch (buffer[0]) {
		case '-':
		case '+':
			// Keep reading until we find a \r\n
			type = RECV_INLINE;
			break;

		case '$':
			// Keep reading for N bytes and then a \r\n
			type = RECV_BULK;
			break;

		case '*': // Do N RECV_BULK
			type = RECV_MULTI_BULK;
			break;

		case ':': // Keep reading until we find a \r\n
			type = RECV_INTEGER;
			break;
	}

	struct reply {
		int argc;
		Object argv;
	}

	if (len > 1) {

	}

}

int main(int argc, char *argv[]) {

	struct RedisHandle *handle = redis_create();
	if (!handle) {
		printf("Failed to create redis handle\n");
		return -1;
	}

	printf("A");

	const struct Object args[] = {
		REDIS_STR("SET"),
		REDIS_STR("key"),
		REDIS_STR("value"),
	};

	if ( redis_connect(handle, "localhost", 6379) ) {
		printf("%s\n", redis_error(handle));
		return 0;
	}

	printf("Connected\n");

	redis_sendBulk(handle, 3, args);

	printf("Sent bulk\n");

	redis_read(handle);

	redis_sendMultiBulk(handle, 3, args);

	redis_free(handle);

	return 0;
}
