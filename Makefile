CFLAGS?= -pedantic $(OPTIMIZATION) -Wall -W
CCLINK?= -lsocket #-ldl -lnsl -lsocket
DEBUG?= -g -rdynamic -ggdb

OBJ = libredis_buffer.o libredis_send.o libredis_recv.o libredis.o

all: libredis

libredis: $(OBJ)
	$(CC) -o libredis $(OBJ)

.c.o:
	$(CC) -c $(CFLAGS) $(DEBUG) $<

clean:
	rm *.o
