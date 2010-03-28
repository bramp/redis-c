CFLAGS?= -std=c99 -pedantic $(OPTIMIZATION) -Wall -W
CCLINK?= -ldl -lnsl -lsocket -lm -lpthread
DEBUG?= -g -rdynamic -ggdb

OBJ = libredis.o

all: libredis

libredis.o: libredis.c

libredis: $(OBJ)
	$(CC) -o libredis $(OBJ)

.c.o:
	$(CC) -c $(CFLAGS) $(DEBUG) $<

clean:
	rm *.o
