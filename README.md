# Squirrel vs Squirrel
## Installation
To build the game, you will first need to install CMake. Head over to https://cmake.org/download/, and download the Windows 64-bit version of CMake.

You will also need a 64-bit version of MinGW. Follow the instructions at https://www.msys2.org/ to install it. Set the install location to `C:\mingw`. Make sure to install `mingw-w64-ucrt-x86_64-gcc`, which installs mingw.

Add `C:\MinGW\bin` to your PATH environemnt variable.

Now, you're ready to build the game. Go to the root of the source directory (which contains this README.md file).

Then, run 
```
mkdir build
cd build
cmake ..
```
