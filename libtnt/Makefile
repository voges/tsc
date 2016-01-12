CC = gcc
AR = ar rcs
CFLAGS = $(INCLUDE_DIRS) -fPIC -O2 -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wundef -Wconversion
LDFLAGS = -shared
INCLUDE_DIRS = -I./include
RM = rm -f
LIB_DIR = lib
SHARED_LIB = $(LIB_DIR)/libtnt.so
STATIC_LIB = $(LIB_DIR)/libtnt.a

SRCS = $(shell find src -name '*.c')
OBJS = $(SRCS:.c=.o)

.PHONY: default all shared static clean

default: all

all: ${SHARED_LIB} $(STATIC_LIB)

shared: $(SHARED_LIB)

static: $(STATIC_LIB)

$(SHARED_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

$(STATIC_LIB): $(OBJS)
	$(AR) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) $< >$@

clean:
	-${RM} ${SHARED_LIB} ${STATIC_LIB} ${OBJS}

