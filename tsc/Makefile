EXEC        = tsc
ALIAS       = detsc
CC          = gcc
CFLAGS      = -O0 -Wall -D _GNU_SOURCE
INCLUDES    = -I.
LIBRARIES   = -L../arithcodec -L../ricecodec
LDFLAGS     = -lm -larithcodec -lricecodec
SOURCEDIR   = .
SOURCES     = $(wildcard $(SOURCEDIR)/*.c)
OBJECTS     = $(SOURCES:.c=.o)
LN_SF       = ln -sf

.PHONY: default all debug clean

default: all

all: $(EXEC) $(ALIAS)

debug: CFLAGS += -g -DDBG
debug: clean all

$(EXEC): $(OBJECTS)
	$(CC) $(INCLUDES) $(LDFLAGS) $(LIBRARIES) -o $@ $(OBJECTS)

$(ALIAS):
	-$(LN_SF) $(EXEC) $(ALIAS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	-$(RM) *.o $(EXEC) $(ALIAS)

