/**
 * libRedis by Andrew Brampton 2010
 * A C library for the Redis server
 */
#include "libredis.h"
#include "libredis_private.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct RedisHandle * redis_alloc() {
	struct RedisHandle *h = malloc( sizeof(struct RedisHandle) );
	if (h == NULL)
		return NULL;

	if (buffer_init(&h->buf, UNKNOWN_READ_LENGTH) == NULL) {
		free(h);
		return NULL;
	}

	h->replies   = 0;
	h->reply     = NULL;
	h->lastReply = NULL;
	h->linePos   = 0;

	h->socket      = INVALID_SOCKET;
	h->socketOwned = 1;
	h->lastErr     = NULL;

	h->state = STATE_WAITING;

	return h;
}

void redis_free(struct RedisHandle * h) {

	struct Reply *r;

	if (h == NULL)
		return;

	/* Close the socket if we own it */
	if (h->socket != INVALID_SOCKET && h->socketOwned)
		closesocket(h->socket);

	buffer_cleanup(&h->buf);

	/* Free all the replies */
	r = h->reply;
	while (r) {
		struct Reply *next = r->next;
		redis_reply_free(r);
		r = next;
	}

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

SOCKET redis_get_socket(struct RedisHandle * h) {
	return h->socket;
}

int redis_use_socket(struct RedisHandle * h, SOCKET s) {
	if (h->socket != INVALID_SOCKET)
		closesocket(h->socket);
	h->socket = s;
	h->socketOwned = 0;
	return 0;
}

int main(int argc, char *argv[]) {

	struct RedisHandle *handle = redis_alloc();
	if (!handle) {
		printf("Failed to create redis handle\n");
		return -1;
	}

	const struct Object args[] = {
		REDIS_STR("SET"),
		REDIS_STR("key"),
		REDIS_STR("value"),
	};

	if ( redis_connect(handle, "localhost", 6379) ) {
		printf("redis_connect: %s\n", redis_error(handle));
		return 0;
	}

	printf("Connected\n");

	if ( redis_send_bulk(handle, 3, args) ) {
		printf("redis_sendBulk: %s\n", redis_error(handle));
		return 0;
	}

	printf("Sent bulk\n");

	int ret;
	int i;
	for (i = 0; i < 10; i++) {
		ret = redis_read(handle);
		printf("%d\n", ret);

		if (ret == -1) {
			printf("redis_read: %s\n", redis_error(handle));
		} else if (ret > 0) {
			struct Reply *r = redis_reply_pop(handle);
			redis_reply_print(r);
		}

	}

	//redis_sendMultiBulk(handle, 3, args);

	redis_free(handle);

	return 0;
}
