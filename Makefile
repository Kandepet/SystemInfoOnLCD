CC=gcc
CFLAGS=-c -Wall
LDFLAGS=-lm
SOURCES=exec.c main.c sockets.c cJSON.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=lcd

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o lcd


boost:
	g++ boost.cc -o boost -lpthread -lboost_system
