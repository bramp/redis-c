#include "redis-c.h"

#include <stdio.h>

struct Object * redis_object_init(struct Object *o, size_t len) {
	if (o == NULL)
		return NULL;

	o->ptr = malloc(len);
	if (o->ptr == NULL) {
		return NULL;
	}

	o->len  = len;
	o->type = REDIS_TYPE_UNKNOWN;
	o->ptrOwned = 1;
	return o;
}

struct Object * redis_object_alloc(size_t len) {
	struct Object * o = malloc(sizeof(struct Object));

	if ( redis_object_init( o, len ) != NULL )
		return o;

	free(o);
	return NULL;
}

struct Object * redis_object_init_copy(struct Object *o, const char *src, size_t len) {
	if (src == NULL && len != 0)
		return NULL;

	o = redis_object_init(o, len);
	if (o == NULL)
		return NULL;

	memcpy(o->ptr, src, len);
	o->type = REDIS_TYPE_RAW;

	return o;
}

struct Object * redis_object_alloc_copy(const char *src, size_t len) {
	struct Object *o;

	if (src == NULL && len != 0)
		return NULL;

	o = malloc(sizeof(struct Object));
	if (redis_object_init_copy( o, src, len ) != NULL)
		return o;

	free(o);
	return NULL;
}

void redis_object_cleanup( struct Object * o ) {
	if (o == NULL)
		return;

	if (o->ptrOwned)
		free(o->ptr);
}

void redis_object_free( struct Object * o ) {
	if (o == NULL)
		return;

	redis_object_cleanup( o );
	free(o);
}

void redis_object_print( const struct Object * o ) {
	switch (o->type) {
		case REDIS_TYPE_UNKNOWN:
		case REDIS_TYPE_RAW:
			printf("{%d:RAW}", o->len);
			break;

		case REDIS_TYPE_STR:
			if (o->len > 10)
				printf("{%d:\"%.7s...\"}", o->len, o->ptr);
			else
				printf("{%d:\"%.s\"}", o->len, o->ptr);
			break;

		case REDIS_TYPE_INT:
			printf("{%d}", (int)o->ptr);
			break;
	}
}
