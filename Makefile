# -D_POSIX_C_SOURCE=200112L Allows getaddrinfo to be used when -std=c99 is in use.

CFLAGS?= $(OPTIMIZATION) -Wall -W #-pedantic -std=c99 -D_POSIX_C_SOURCE=200112L
CCLINK?= -lsocket #-ldl -lnsl -lsocket
DEBUG?= -g -rdynamic -ggdb

OBJ = redis_object.o redis_reply.o redis_buffer.o redis_cmd.o redis_send.o redis_recv.o redis-c.o

all: redis-c

redis-c: $(OBJ)
	$(CC) -o redis-c $(OBJ)

redis_object.c : redis-c.h
redis_reply.c  : redis-c.h
redis_buffer.c : redis-c.h
redis_cmd.c    : redis-c.h
redis_send.c   : redis-c.h
redis_recv.c   : redis-c.h redis_private.h
redis-c.c      : redis-c.h redis_private.h

redis-c.h         : redis_buffer.h

.c.o:
	$(CC) -c $(CFLAGS) $(DEBUG) $<

clean:
	rm *.o redis-c
