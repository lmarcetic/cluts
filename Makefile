#
# Copyright (c) 2011 Luka Marčetić<paxcoder@gmail.com>
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted.
# 
# There's ABSOLUTELY NO WARRANTY, express or implied.
#

CFLAGS=-std=c99 -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2
LIBS = -lpthread -lrt -lm
SRC = $(wildcard tests/*.c) $(wildcard *.c)
BIN = $(SRC:.c=)

help:
	@echo
	@echo "Compiling all tests in all collections will most likely fail because"
	@echo "of functions missing from C libraries. Special rules have therefore"
	@echo "been created for GNU and Musl C libraries - namely 'glibc' and"
	@echo "'musl'. Also, you may wish to define a special compiler."
	@echo "In the case of Musl, a probable line is: make musl CC=musl-gcc"
	@echo



%:%.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ $<

real-all: $(BIN)



all: help real-all
	
musl: CFLAGS += -DMUSL=1
musl: real-all

glibc: CFLAGS += -DGLIBC=1
glibc: real-all



%.run:%
	./$<
test: all $(SRC:.c=.run)
	
clean:
	rm -f $(BIN)
