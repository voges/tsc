EXEC        = tsc
CC          = gcc
CFLAGS      = -O0 -Wall -D _GNU_SOURCE
#INCLUDES    = -I./include
#LIBS        = -L./lib
#LDFLAGS     = -l<lib#1> -lm ...
SOURCEDIR   = .
SOURCES     = $(wildcard $(SOURCEDIR)/*.c)
OBJECTS     = $(SOURCES:.c=.o)

default: $(EXEC)

debug: CFLAGS += -g
debug: $(EXEC)

$(EXEC): $(OBJECTS) 
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
	
clean:
	$(RM) *.o $(EXEC)

