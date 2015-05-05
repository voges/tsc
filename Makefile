EXECUTABLE       = tsc
CC               = gcc
CFLAGS           = -O0 -Wall
#INCLUDES        = -I./include
#LIBS            = -L./lib
#LDFLAGS         = -l<lib#1> -lm ...
SOURCEDIR        = .
SOURCES          = $(wildcard $(SOURCEDIR)/*.c)
OBJECTS          = $(SOURCES:.c=.o)

default: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
	
clean:
	$(RM) *.o *~ $(EXECUTABLE)

