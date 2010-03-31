CFLAGS?= -pedantic $(OPTIMIZATION) -Wall -W
CCLINK?= -lsocket #-ldl -lnsl -lsocket
DEBUG?= -g -rdynamic -ggdb

OBJ = libredis_object.o libredis_reply.o libredis_buffer.o libredis_send.o libredis_recv.o libredis.o

all: libredis

libredis: $(OBJ)
	$(CC) -o libredis $(OBJ)

libredis_object.c : libredis.h
libredis_reply.c  : libredis.h
libredis_buffer.c : libredis.h
libredis_send.c   : libredis.h
libredis_recv.c   : libredis.h libredis_private.h
libredis.c        : libredis.h libredis_private.h

libredis.h        : libredis_buffer.h

.c.o:
	$(CC) -c $(CFLAGS) $(DEBUG) $<

clean:
	rm *.o
