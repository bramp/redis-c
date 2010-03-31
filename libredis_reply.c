#include "libredis.h"

#include <stdio.h>

struct Reply * redis_reply_alloc(int argc) {
	/* Malloc one Reply, and many Objects */
	struct Reply * r;

	if (argc > 0)
		r = malloc(sizeof(struct Reply) + (argc-1) * sizeof(struct Object));
	else
		r = malloc(sizeof(struct Reply));

	if (r == NULL)
		return NULL;

	r->argc = argc;
	r->next = NULL;

	/* Ensure the objects start blanked */
	memset(&r->argv, 0, argc *  sizeof(struct Object));

	return r;
}

/*
 * FIFO
 * Add to the back (lastReply->next = reply; lastReply = reply)
 * Take from the front (reply = reply->next)
 */


struct Reply * redis_reply_pop(struct RedisHandle * h) {
	struct Reply *r;

	if (h == NULL || h->replies == 0)
		return NULL;

	assert(h->reply != NULL);

	r = h->reply;
	h->reply = h->reply->next;
	h->replies--;
	return r;
}

void redis_reply_temp_push(struct RedisHandle * h, struct Reply *r) {
	if (h->lastReply)
		h->lastReply->next = r;

	h->lastReply = r;

	if (h->reply == NULL)
		h->reply = r;
}

void redis_reply_push(struct RedisHandle * h) {
	h->replies++;
}

void redis_reply_free(struct Reply *r) {
	unsigned int i;

	for (i=0; i < r->argc; i++)
		redis_object_cleanup(&r->argv[i]);

	free(r);
}

void redis_reply_print(const struct Reply *r) {
	unsigned int i;

	printf("Reply {");
	for (i=0; i < r->argc; i++) {
		printf("\n   ");
		redis_object_print(&r->argv[i]);
	}
	printf("\n}\n");
}
