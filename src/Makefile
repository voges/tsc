EXEC      = tsc
CC        = gcc
CWARN     = -Wall #-Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wundef #-Wconversion -Wextra
CFLAGS    = -D _GNU_SOURCE $(CWARN) #-std=c99
INCLUDES  = #-I.
LIBRARIES = #-L.
LDFLAGS   = -lm -lz

TSC_SRC     = common.c samcodec.c samparser.c tsc.c tscfmt.c tsclib.c range.c
CODECS_SRC := $(shell find codecs -name '*.c')
TVC_SRC    := $(shell find tvclib -name '*.c')
SOURCES     = $(TSC_SRC) $(CODECS_SRC) $(TVC_SRC)
OBJECTS     = $(SOURCES:.c=.o)

.PHONY: default all debug clean

default: all

all: CFLAGS += -O2
all: $(EXEC)

debug: CFLAGS += -g -D DBG -O0
debug: clean $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(INCLUDES) $(LIBRARIES) -o $@ $(OBJECTS) $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	find . -name "*.o" -type f -delete

