#include "redis-c.h"

#include <assert.h>

/**
 * Commands operating on all the kind of values
 */

int redis_int_bulk_command(struct RedisHandle *h, const int argc, const struct Object argv[] ) {
	int ret = 0;
	struct Reply *r;

	if (redis_send_bulk(h, argc, argv))
		return -1;

	while (ret == 0) {
		ret = redis_read(h);
		if (ret < 0)
			return -1;
	}

	r = redis_reply_pop(h);
	assert(r != NULL);

	/* r should be a int */
	if (r->argc != 1 || r->argv[0].type != REDIS_TYPE_INT) {
		h->lastErr = "Error reading int reply, the reply does not have exactly one integer response.";
		redis_reply_free(r);
		return -1;
	}

	ret = (int)r->argv[0].ptr;

	redis_reply_free(r);

	return ret;
}

/**
 * EXISTS key test if a key exists
 * @param h
 * @param key
 * @param len
 * @return
 */
int redis_exists(struct RedisHandle *h, const char *key, size_t len) {
	const struct Object args[] = {
		REDIS_STR("EXISTS"),
		REDIS_RAW(key, len),
	};
	return redis_int_bulk_command(h, sizeof(args) / sizeof(args[0]), args);
}

/**
 * DEL key delete a key
 */

/**
 * TYPE key return the type of the value stored at key
 */

/**
 * KEYS pattern return all the keys matching a given pattern
 */

/**
 * RANDOMKEY return a random key from the key space
 */
/*
RENAME oldname newname rename the old key in the new one, destroing the newname key if it already exists
RENAMENX oldname newname rename the old key in the new one, if the newname key does not already exist
DBSIZE return the number of keys in the current db
EXPIRE set a time to live in seconds on a key
TTL get the time to live in seconds of a key
SELECT index Select the DB having the specified index
MOVE key dbindex Move the key from the currently selected DB to the DB having as index dbindex
FLUSHDB Remove all the keys of the currently selected DB
FLUSHALL Remove all the keys from all the databases
*/
