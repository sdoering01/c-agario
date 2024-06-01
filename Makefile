TARGET = agario
OBJECTS = agario.o geometry.o protocol.o
HEADERS = geometry.h protocol.h
CFLAGS = -Wall -O2
LINK_FLAGS = -lm

UNITY_SRC = test/unity/unity.c
UNITY_HEADERS = test/unity/unity.h test/unity/unity_internals.h
UNITY_OBJ = test/unity/unity.o
TEST_TARGETS = test/test_protocol

# To add a new test
#  - add the compilation recipe
#  - add the file to TEST_TARGETS
#  - call the test binary in the test recipe
#  - Add binary to .gitignore

%.o: %.c $(HEADERS)
	gcc $(CFLAGS) $< -c -o $@

$(TARGET): $(OBJECTS)
	gcc $(OBJECTS) -o $(TARGET) $(LINK_FLAGS)

$(UNITY_OBJ): $(UNITY_SRC) $(UNITY_HEADERS)
	gcc $(CFLAGS) $< -c -o $@

test/%.o: test/%.c $(HEADERS)
	gcc $(CFLAGS) $< -c -o $@

test/test_protocol: test/test_protocol.o protocol.o $(UNITY_OBJ)
	gcc $^ -o $@ $(LINK_FLAGS)

all: $(TARGET)

test: $(TEST_TARGETS)
	./test/test_protocol

clean:
	rm -f $(OBJECTS) $(TARGET) $(UNITY_OBJ) test/*.o $(TEST_TARGETS)

.PHONY: all test clean
