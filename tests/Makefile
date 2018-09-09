CC=gcc
CFLAGS=-std=gnu99 -Wall -lrt -lpthread -lssl -lcrypto -ggdb -fsanitize=address
RELEASE=../bin
SRC=../src/ringbuf.c 	\
	../src/networking.c \
	../src/vessel.c 	\
	../src/list.c 		\
	vessel_test.c


test: unit_tests.c
	mkdir -p $(RELEASE) && $(CC) $(CFLAGS) $(SRC) -o $(RELEASE)/vessel_test unit_tests.c && $(RELEASE)/vessel_test
