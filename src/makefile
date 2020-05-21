# ------------------------------------------------
# Makefile
#
# Author		:	Steven Kolamkuzhiyil
# Email			:	stevenkolamkuzhiyil@gmail.com
# Date			:	31.12.2019
# Program name	:	generator & supervisor
# ------------------------------------------------

CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c99 -pedantic -Wall $(DEFS) -g

SUPERVISOR_OBJECTS = supervisor.o common.o
GENERATOR_OBJECTS = generator.o common.o

.PHONY: all clean
all: supervisor generator

supervisor: $(SUPERVISOR_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ -lrt -pthread

generator: $(GENERATOR_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ -lrt -pthread

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

supervisor.o : supervisor.c common.h
generator.o: generator.c common.h
common.o: common.c common.h

clean:
	rm -rf *.o supervisor generator