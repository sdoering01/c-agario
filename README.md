# C Agario

This is a remake of [agar.io](https://agar.io) in pure C (although GitHub falsely
claims that there is some C++ present as well).

The motivation for this project is to learn about low level network
programming, server programming, and game programming. Concepts include
building an event loop with low level primitives (`kqueue` in this case, since
I am on MacOS), handling TCP connections, implementing a custom binary game
protocol based on TCP. Being a learning project, only a minimal amount of
external dependencies is used. Currently, the project only uses a simple
testing library (unity -- not to be confused with the Unity game engine) and a
library that provides a layer of abstraction for the GUI (raylib). In the
future, I may try to remove the dependency for raylib and write the graphics
code myself.

I am aware that TCP is not the most optimal protocol for real-time games
(because of head-of-line blocking), so I may switch the project over to some
UDP-based protocol in the future, that contains mechanisms to handle dropped
packets, retransmission, and package ordering.

## Project Status

This project is still in development.

The server and GUI are already functional, the protocol is working, and players
can join the game and move on the field.

Currently I am working on implementing an AVL tree from scratch as a more
efficient data structure to store and look up player information on the server
and in the GUI.

Some features that are missing include:

- Spawning and eating foods
- Eating different players
- Leaderboard
- Polishing

## Development

Since this project uses OS-dependent low-level primitives and APIs, like
`kqueue`, it can currently only be executed on MacOS.

Before you start, ensure that you have Raylib installed: `brew install raylib`.

To compile the server and the GUI, run `make all`. The server and the GUI
binaries can be (re-)compiled and executed via the make targets `run-server`
and `run-gui` respectively.

To run all tests, run `make test`.

To clean up all the generated files, run `make clean`.
