EXEC         = tsc
CC           = gcc
CFLAGS       = $(INCLUDE_DIRS) -Wall #-Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wundef #-Wconversion -Wextra
INCLUDE_DIRS = -I../libtnt/include -I.
LIB_DIRS     = -L../libtnt/lib
LDFLAGS      = -lm -lz -ltnt

TSC_SRCS     = samcodec.c samparser.c tsc.c tscfmt.c tsclib.c
CODECS_SRCS := $(shell find codecs -name '*.c')
COMMON_SRCS := $(shell find common -name '*.c')
SRCS         = $(TSC_SRCS) $(CODECS_SRCS) $(COMMON_SRCS)
OBJS         = $(SRCS:.c=.o)

RM = rm -f

.PHONY: default all debug clean

default: all

all: CFLAGS += -O2
all: $(EXEC)

debug: CFLAGS += -g -D DBG -O0
debug: clean $(EXEC)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIB_DIRS) $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS)

