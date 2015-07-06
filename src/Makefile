EXEC        = tsc
ALIAS       = detsc
CC          = gcc
CFLAGS      = -Wall -D _GNU_SOURCE
INCLUDES    = #-I.
LIBRARIES   = #-L.
LDFLAGS     = -lm
SOURCEDIR   = .
SOURCES    := $(shell find $(SOURCEDIR) -name '*.c') # find C files recursively
OBJECTS     = $(SOURCES:.c=.o)
LN_SF       = ln -sf

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

