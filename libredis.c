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

struct RedisHandle * redis_create() {
	struct RedisHandle *h = malloc( sizeof(struct RedisHandle) );
	if (h == NULL)
		return NULL;

	if (buffer_init(&h->recv, 128) == NULL) {
		free(h);
		return NULL;
	}

	h->socket = INVALID_SOCKET;
	h->socketOwned = 1;
	h->lastErr = NULL;

	return h;
}

void redis_free(struct RedisHandle * h) {
	if (h == NULL)
		return;

	if (h->socket != INVALID_SOCKET && h->socketOwned)
		closesocket(h->socket);

	buffer_free(&h->recv);

	free(h);
}

const char * redis_error(struct RedisHandle * h) {
	return h->lastErr;
}

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

	/* Create the hint for getaddrinfo (we want AF_INET or AF_INET6, but it has to be
	 * a TCP SOCK_STREAM connection) */
	memset( &hint, 0, sizeof(hint) );
	hint.ai_family   = PF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;

	/* Lookup the hostname */
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
			/* If connecting was OK, we bail out */
			h->lastErr = NULL;
			h->socketOwned = 1;

			ret = 0;
			goto cleanup;
		}

		/* We cleanup the socket because the next ai_family might be different */
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


struct Object * redis_object_init() {
	struct Object *o = malloc( sizeof(*o) );
	if (o == NULL)
		return NULL;

	o->ptr = NULL;
	o->len = 0;
	o->type = REDIS_TYPE_UNKNOWN;
	o->ptrOwned = 0;
	return o;
}

struct Object * redis_object_init_copy(const char *src, size_t len) {
	struct Object *o = malloc( sizeof(*o) );
	if (o == NULL)
		return NULL;

	o->ptr = malloc(len);
	if (o->ptr == NULL) {
		free(o);
		return NULL;
	}
	memcpy(o->ptr, src, len);
	o->len = len;
	o->type = REDIS_TYPE_RAW;
	o->ptrOwned = 1;
	return o;
}

void redis_object_free( struct Object * o ) {
	if (o == NULL)
		return;

	if (o->ptrOwned)
		free(o->ptr);
	free(o);
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
