EXEC       = tsc
ALIAS      = detsc
CC         = gcc
CWARN      = -Wall #-Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wundef -Wconversion
CFLAGS     = -D _GNU_SOURCE $(CWARN) #-std=c99
INCLUDES   = #-I.
LIBRARIES  = #-L.
LDFLAGS    = -lm -lz

TSC_SRC     = common.c samcodec.c samparser.c tsc.c tscfmt.c tsclib.c range.c
CODECS_SRC := $(shell find codecs -name '*.c')
TVC_SRC    := $(shell find tvclib -name '*.c')
SOURCES = $(TSC_SRC) $(CODECS_SRC) $(TVC_SRC)

OBJECTS = $(SOURCES:.c=.o)
LN_SF   = ln -sf

.PHONY: default all debug clean

default: all

all: CFLAGS += -O2
all: $(EXEC) $(ALIAS)

debug: CFLAGS += -g -D DBG -O0
debug: clean $(EXEC) $(ALIAS)

$(EXEC): $(OBJECTS)
	$(CC) $(INCLUDES) $(LIBRARIES) -o $@ $(OBJECTS) $(LDFLAGS)

$(ALIAS):
	-$(LN_SF) $(EXEC) $(ALIAS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-$(RM) $(EXEC) $(ALIAS)
	find . -name "*.o" -type f -delete

