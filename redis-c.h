#ifndef REDIS_C_H
#define REDIS_C_H

#include "redis_buffer.h"

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
	struct Reply *next;       /** Next reply in the list of replies */

	unsigned int argc;        /** Number of responses this reply contains */
	struct Object argv[1];    /** The responses */
};

struct RedisHandle {
	SOCKET socket;
	const char *lastErr;         /** Keeps track of the last err */

	unsigned int state;          /** What state is this handle in */
	struct Buffer buf;           /** Receive buffer to keep track of data between calls. */

	unsigned int replies;        /** Number of replies waiting (this may be less than the number of replies in the following linked list */
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
struct RedisHandle * redis_alloc();

/**
 * Cleans up a RedisHandle releasing all resources and making it no longer valid.
 *
 * @param handle The handle to free
 *
 * @see redis_alloc
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
SOCKET redis_get_socket(struct RedisHandle * h);

int redis_use_socket(struct RedisHandle * h, SOCKET s);

/*
 * Object
 */
struct Object * redis_object_alloc(size_t buflen);
struct Object * redis_object_alloc_copy(const char *buf, size_t buflen);

struct Object * redis_object_init(struct Object *o, size_t buflen);
struct Object * redis_object_init_copy(struct Object * o, const char *buf, size_t buflen);

/**
 * Cleanup any memory used internally by the object.
 * Use this function if you created the Object and used {@link redis_object_init}
 * @param o
 */
void redis_object_cleanup( struct Object * o );

/**
 * Cleanup any memory used by the object, and free the Object's memory.
 * Use this function if the Object was created with {@link redis_object_alloc}
 * @param o
 */
void redis_object_free( struct Object * o );

/**
 * Prints the Object out to stdout. Useful for debugging.
 * @param o
 */
void redis_object_print( const struct Object * o );

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
int redis_send_multibulk(struct RedisHandle *handle, const int argc, const struct Object argv[] );

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
int redis_send_bulk(struct RedisHandle *handle, const int argc, const struct Object argv[] );

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
int redis_read(struct RedisHandle * h);

/*
 * Reply
 */

/**
 * Creates a new Reply with argc responses.
 * @param argc Number of responses to attach to the reply
 * @return The new reply, or NULL on error.
 */
struct Reply * redis_reply_alloc(int argc);

/**
 * Retrieves a Reply from the RedisHandle.
 * @param h A new Reply, or NULL if there are no Replies.
 */
struct Reply * redis_reply_pop(struct RedisHandle * h);

/**
 * Push the Reply onto the end of list of replies, BUT don't increment
 * the count of replies. This allows us to store the reply while we are
 * working on it.
 * @param h
 * @param r
 */
void redis_reply_temp_push(struct RedisHandle * h, struct Reply *r);

/**
 * We have now finished creating the reply, so increment the count of replies.
 * @param h
 */
void redis_reply_push(struct RedisHandle * h);

/**
 * Frees a reply that was created with {@link redis_reply_alloc}
 * @param r
 */
void redis_reply_free(struct Reply *r);


void redis_reply_print(const struct Reply *r);

#endif /* REDIS_C_H */
