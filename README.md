# Overview
---

This project is designed for my use, to make it easier to build modular programs where code is decoupled and reuseable between projects.

# Running
---

This is my example I made while learning winsock, to run, start the server exe first, and then run the client exe from the command line with the argument localhost (no '-' or quote marks). The commands usable are:

### disconnect
This tells the server to close the connection and shut down, sending a response to the client to tell it to shut down as well

### domain
Simple sends the message localhost back to the client

### echo
Echoes back whatever you put after echo

# Building
---

Windows
---

Ninja (Preferred):

Prerequisite: Ensure you have gcc c compiler and ninja installed and set to path, cd to root directory of project

1. mkdir build && cd build
2. cmake .. -DCMAKE_CXX_COMPILER=gcc -G "Ninja"
3. cmake --build . -j10

MSVC (Visual Studio 16 2019 preferred, any version should work however):

Prerequisite: Ensure you have a visual studio sdk installed, cd to root directory of project

1. mkdir build && cd build
2. cmake .. -G "Visual Studio 16 2019"
3. Use the produced solution file to compile, setting the startup project to the desired project

Linux
---

Currently there is not a unix based implementation, this will be changed in the future
