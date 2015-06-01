EXEC        = tsc
CC          = gcc
CFLAGS      = -O0 -Wall -D _GNU_SOURCE
INCLUDES    = #-I
LIBRARIES   = #-L
LDFLAGS     = -lm
SOURCEDIR   = .
SOURCES     = $(wildcard $(SOURCEDIR)/*.c)
OBJECTS     = $(SOURCES:.c=.o)

.PHONY: all debug clean

default: all
all: $(EXEC)

debug: CFLAGS += -g 
debug: EXEC += .debug
debug: $(EXEC)

$(EXEC): $(OBJECTS) 
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
	
clean:
	$(RM) *.o $(EXEC)

