EXEC        = tsc
ALIAS       = detsc
CC          = gcc
CFLAGS      = -O0 -Wall -D _GNU_SOURCE
INCLUDES    = #-I
LIBRARIES   = #-L
LDFLAGS     = -lm
SOURCEDIR   = .
SOURCES     = $(wildcard $(SOURCEDIR)/*.c)
OBJECTS     = $(SOURCES:.c=.o)
LN          = ln -s

.PHONY: default all debug clean

default: all

all: $(EXEC) $(ALIAS)

debug: CFLAGS += -g
debug: clean all

$(EXEC): $(OBJECTS) 
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

$(ALIAS):
	-$(LN) $(EXEC) $(ALIAS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	-$(RM) *.o $(EXEC) $(ALIAS)

