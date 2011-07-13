#
# Copyright (c) 2011 Luka Marčetić<paxcoder@gmail.com>
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted.
# 
# There's ABSOLUTELY NO WARRANTY, express or implied.
#

CFLAGS = -std=c99 -Wall -Wextra -O2
LIBS = -lpthread -lrt -lm
SRC = $(wildcard tests/*.c) $(wildcard *.c)
BIN = $(SRC:.c=)

%:%.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ $<
all: $(BIN)

%.run:%
	./$<
test: all $(SRC:.c=.run)
	
clean:
	rm -f $(BIN)
