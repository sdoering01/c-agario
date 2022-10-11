TARGET = agario
OBJECTS = agario.o
HEADERS = 
CFLAGS = -Wall -O2

%.o: %.c $(HEADERS)
	gcc $(CFLAGS) $< -c

$(TARGET): $(OBJECTS)
	gcc $(OBJECTS) -o $(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean all
