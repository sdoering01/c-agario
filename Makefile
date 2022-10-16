TARGET = agario
OBJECTS = agario.o geometry.o protocol.o
HEADERS = geometry.h protocol.h
CFLAGS = -Wall -O2
LINK_FLAGS = -lm

%.o: %.c $(HEADERS)
	gcc $(CFLAGS) $< -c

$(TARGET): $(OBJECTS)
	gcc $(OBJECTS) -o $(TARGET) $(LINK_FLAGS)

all: $(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
