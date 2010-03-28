CFLAGS?= -std=c99 -pedantic $(OPTIMIZATION) -Wall -W -D__EXTENSIONS__ -D_XPG6
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
