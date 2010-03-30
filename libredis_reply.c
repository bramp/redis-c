#include "libredis.h"

struct Reply * redis_reply_alloc(int argc) {
	// Malloc one Reply, and many Objects
	struct Reply * r = malloc(sizeof(struct Reply) + (argc-1) * sizeof(struct Object));
	if (r == NULL)
		return NULL;

	r->argc = argc;

	// Ensure the objects start blanked
	memset(&r->argv, 0, argc *  sizeof(struct Object));

	return r;
}

struct Reply * redis_reply_pop(struct RedisHandle * h) {
	if (h->replies == 0)
		return NULL;

	struct Reply *r = h->reply;
	h->reply = h->reply->next;
	h->replies--;
	return r;
}

void redis_reply_temp_push(struct RedisHandle * h, struct Reply *r) {
	h->lastReply->next = r;
	h->lastReply = r;
}

void redis_reply_push(struct RedisHandle * h) {
	h->replies++;
}

void redis_reply_free(struct Reply *r) {
	int i;

	for (i=0; i < r->argc; i++)
		redis_object_cleanup(r->argv[i]);

	free(r);
}
