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

all: CFLAGS += -O2 -D TEST_MAIN
all: $(EXEC)

debug: CFLAGS += -g -O0 -D TEST_MAIN
debug: clean $(EXEC)

lib: CFLAGS += -O2
lib: clean $(OBJECTS)
	$(AR) r libarithcodec.a arithcodec.o
	ranlib libarithcodec.a

$(EXEC): $(OBJECTS) 
	$(CC) $(INCLUDES) $(LIBRARIES) -o $@ $(OBJECTS) $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	-$(RM) *.o *.a $(EXEC) $(ALIAS)

