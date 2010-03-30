#include "libredis.h"

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
	o = redis_object_init(o, len);
	if (o == NULL)
		return NULL;

	memcpy(o->ptr, src, len);
	o->type = REDIS_TYPE_RAW;

	return o;
}

struct Object * redis_object_alloc_copy(const char *src, size_t len) {
	struct Object *o = malloc(sizeof(struct Object));
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
