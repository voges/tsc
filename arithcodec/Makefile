EXEC        = arith
CC          = gcc
CFLAGS      = -Wall
INCLUDES    = #-I
LIBRARIES   = #-L
LDFLAGS     = #-lm
SOURCEDIR   = .
SOURCES     = $(wildcard $(SOURCEDIR)/*.c)
OBJECTS     = $(SOURCES:.c=.o)

.PHONY: default all debug lib clean

default: all

all: CFLAGS += -O2
all: $(EXEC)

debug: CFLAGS += -g -O0
debug: clean $(EXEC)

lib: $(OBJECTS)
	ar r libarithcodec.a arithcodec.o
	ranlib libarithcodec.a

$(EXEC): $(OBJECTS) 
	$(CC) $(INCLUDES) $(LIBRARIES) -o $@ $(OBJECTS) $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	-$(RM) *.o $(EXEC) $(ALIAS)

