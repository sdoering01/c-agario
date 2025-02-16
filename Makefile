SERVER_TARGET = agario
SERVER_OBJECTS = agario.o geometry.o protocol.o networking.o

GUI_TARGET = gui
GUI_OBJECTS = gui.o protocol.o networking.o

HEADERS = geometry.h protocol.h networking.h

CFLAGS = -Wall -Wpedantic -Wextra -O2
GUI_CFLAGS = $(CFLAGS) `pkg-config --cflags raylib`

LINK_FLAGS = -lm
GUI_LINK_FLAGS = $(LINK_FLAGS) `pkg-config --libs raylib`

UNITY_SRC = test/unity/unity.c
UNITY_HEADERS = test/unity/unity.h test/unity/unity_internals.h
UNITY_OBJ = test/unity/unity.o
TEST_TARGETS = test/test_protocol test/test_tree

# To add a new test
#  - add the compilation recipe
#  - add the file to TEST_TARGETS
#  - call the test binary in the test recipe
#  - Add binary to .gitignore

gui.o: gui.c $(HEADERS)
	gcc $(GUI_CFLAGS) $< -c -o $@

%.o: %.c $(HEADERS)
	gcc $(CFLAGS) $< -c -o $@

$(SERVER_TARGET): $(SERVER_OBJECTS)
	gcc $(SERVER_OBJECTS) -o $(SERVER_TARGET) $(LINK_FLAGS)

$(GUI_TARGET): $(GUI_OBJECTS)
	gcc $(GUI_OBJECTS) -o $(GUI_TARGET) $(GUI_LINK_FLAGS)

$(UNITY_OBJ): $(UNITY_SRC) $(UNITY_HEADERS)
	gcc $(CFLAGS) $< -c -o $@

test/%.o: test/%.c $(HEADERS)
	gcc $(CFLAGS) $< -c -o $@

test/test_protocol: test/test_protocol.o protocol.o $(UNITY_OBJ)
	gcc $^ -o $@ $(LINK_FLAGS)

test/test_tree: test/test_tree.o tree.o $(UNITY_OBJ)
	gcc $^ -o $@ $(LINK_FLAGS)

compile_flags.txt: generate_compile_flags.sh
	./generate_compile_flags.sh

all: $(SERVER_TARGET) $(GUI_TARGET) compile_flags.txt

test: $(TEST_TARGETS)
	./test/test_protocol
	@echo "\n"
	./test/test_tree

run-server: $(SERVER_TARGET)
	./$(SERVER_TARGET)

run-gui: $(GUI_TARGET)
	./$(GUI_TARGET)

debug_tree: tree.o debug_tree.o
	gcc $^ -o $@ $(LINK_FLAGS)

debug-tree: debug_tree
	./$<

clean:
	rm -f $(SERVER_OBJECTS) $(SERVER_TARGET) $(GUI_OBJECTS) $(GUI_TARGET) $(UNITY_OBJ) test/*.o $(TEST_TARGETS)

.PHONY: all test run-server run-gui clean
