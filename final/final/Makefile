CC = gcc

all : mymake

mymake : bbfs.c log.c
	$(CC) -o hybrid bbfs.c log.c -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -lfuse -lpthread

clean :
	rm -f hybrid


