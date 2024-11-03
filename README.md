# DLL Injection for Touhou Project 7

In order to learn the basics of hacking games and have some fun I wrote a DLL injection invincibility hack for Touhou Project 7.

### Building
Can be built using `cmake` and the `mingw-w64-gcc` toolchain on either Linux or Windows.

### dllinject.exe
Searches the running process list for `th07.exe` and injects `hax.dll`

### hax.dll
Reimplements a function responsible for player death
