TARGET = agario
OBJECTS = agario.o geometry.o
HEADERS = geometry.h
CFLAGS = -Wall -O2
LINK_FLAGS = -lm

%.o: %.c $(HEADERS)
	gcc $(CFLAGS) $< -c

$(TARGET): $(OBJECTS)
	gcc $(OBJECTS) -o $(TARGET) $(LINK_FLAGS)

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean all
