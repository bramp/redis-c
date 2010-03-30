#ifndef LIBREDIS_H
#define LIBREDIS_H

#include "libredis_buffer.h"

#include <string.h>
#include <stdlib.h>

#ifndef WIN32
/* These make us more compatible with Windows, and I like them :) */
# define INVALID_SOCKET -1
# define SOCKET_ERROR -1
# define closesocket(x) close(x)
  typedef int SOCKET;
#endif

#define REDIS_TYPE_UNKNOWN 0
#define REDIS_TYPE_STR 1
#define REDIS_TYPE_RAW 2
#define REDIS_TYPE_INT 3

struct Object {
	char *ptr;                /** Pointer to raw/str data */
	size_t len;               /** The length of the data */
	unsigned int type     :2; /** What type of data is pointed to */
	unsigned int ptrOwned :1; /** Should we free the ptr? */
};

struct Reply {
	struct Reply *next;

	int argc;
	Object *argv[];
};

struct RedisHandle {
	SOCKET socket;
	const char *lastErr;         /** Keeps track of the last err */

	unsigned int state;          /** What state is this handle in */
	struct Buffer buf;           /** Receive buffer to keep track of data between libredis calls. */

	unsigned int replies;        /** Number of replies waiting */
	struct Reply *reply;         /** List of replies */
	struct Reply *lastReply;     /** The last reply we received (points to end of list) */

	size_t linePos;              /** Keeps track of how far we have looked for \r\n */

	unsigned int socketOwned :1; /** Did we create this socket? */
};

#define REDIS_STR(x)     {(char *)(x), strlen(x), REDIS_TYPE_STR, 0}
#define REDIS_RAW(x,len) {(char *)(x), (len),     REDIS_TYPE_RAW, 0}
#define REDIS_INT(x)     {(char *)(x), 0,         REDIS_TYPE_INT, 0}
#define REDIS_NIL()      {NULL, 0,                REDIS_TYPE_RAW, 0}

/**
 * Creates a new handle to connect to a Redis server. This handle will be passed to most
 * of the API's functions. The handle must be freed once it is no longer needed by
 * {@link redis_free}.
 *
 * @return A new {@link RedisHandle}, or NULL if a error occurred.
 *
 * @see redis_free
 */
struct RedisHandle * redis_create();

/**
 * Cleans up a RedisHandle releasing all resources and making it no longer valid.
 *
 * @param handle The handle to free
 *
 * @see redis_create
 */
void redis_free(struct RedisHandle * h);

/**
 * Returns the last error to have occurred on this handle.
 *
 * @return The last error as a string, or NULL if no error occurred.
 */
const char * redis_error(struct RedisHandle * h);

/**
 * Connects to a Redis Server.
 *
 * @param host Server's hostname. If NULL localhost is used.
 * @param port Server's port. If 0 the default 6379 is used.
 *
 * @return 0 on success, -1 on failure. Use {@link redis_error} to determine the error
 */
int redis_connect(struct RedisHandle * h, const char *host, unsigned short port);

/**
 * Returns the socket used to connect to the Redis Server.
 *
 * @return The socket which is being used, or INVALID_SOCKET if no socket is being used.
 */
static SOCKET redis_get_socket(struct RedisHandle * h) {
	return h->socket;
}

int redis_use_socket(struct RedisHandle * h, SOCKET s);

/*
 * Object
 */
struct Object * redis_object_init();
struct Object * redis_object_init_copy(const char *buf, size_t buflen);
void redis_object_free( struct Object * o );

/*
 * Send
 */

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
int redis_sendMultiBulk(struct RedisHandle *handle, const int argc, const struct Object argv[] );

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
int redis_sendBulk(struct RedisHandle *handle, const int argc, const struct Object argv[] );

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
int redis_send(struct RedisHandle *handle, const int argc, const struct Object argv[] );

/*
 * Recv
 */
struct Object * redis_readInline(struct RedisHandle * h, struct Object *object, size_t * objectMaxLen);
int redis_read(struct RedisHandle * h);

#endif /* LIBREDIS_H */
