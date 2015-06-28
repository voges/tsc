EXEC        = rice
CC          = gcc
CFLAGS      = -O0 -Wall #-ansi -pedantic
INCLUDES    = #-I
LIBRARIES   = #-L
LDFLAGS     = #-lm
SOURCEDIR   = .
SOURCES     = $(wildcard $(SOURCEDIR)/*.c)
OBJECTS     = $(SOURCES:.c=.o)

.PHONY: default all debug clean

default: all

all: $(EXEC)

debug: CFLAGS += -g
debug: clean all

$(EXEC): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	-$(RM) *.o $(EXEC) $(ALIAS)

