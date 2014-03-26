Crossbow
========

Crossbow is a collection of C++ library used in the Tell system. It contains
several libraries, some of them are header only.

Install instructions
--------------------
Crossbow uses CMake and requires a C++ compiler that supports C++11. If nothing
else is written here, the libraries are tested on Linux with gcc and Mac OS X
with clang and libc++.

To use the header only libraries, you can just unpack/clone the crossbow source
directory and add the directory to your include path.

To install the crossbow libraries you should create a build directory and execute
cmake followed by make install (or ninja install). You can specify an install prefix
to change the install location.

**Example:** (to install to /opt/local)
`mkdir build`
`cd build`
`cmake -DCMAKE_INSTALL_PREFIX=/opt/local ..`
`make`
`make install`

If a library has some additional depenedencies, cmake with only build and install them
if it can find the dependencies installed on your system. Please look into the sections
for each library to find out, what kind of dependencies the library has (if any).

string (header only)
--------------------
crossbow::string is a replacememnt for std::string. crossbow::string allocates a
32 byte buffer on the stack when it is constructed. It can store up to 30 (for
char strings) or 15 (for wchar strings) characters on the stack space. If the string
grows bigger than that, it will allocate space on the stack like std::string and use
the 32 byte buffer to safe the pointer, size, and capacity of the string.
If you have a lot of small strings in your system, this string type might give you
additional performance, since it might prevent a lot of calls to malloc/free.

**Dependencies**: This library does not have any dependencies.
