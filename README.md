# Wave Function Collapse in C++
This is a C++ port of https://github.com/mxgmn/WaveFunctionCollapse.

All sample images come from https://github.com/mxgmn/WaveFunctionCollapse

# License
This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.

The sample images are from https://github.com/mxgmn/WaveFunctionCollapse, so they are NOT covered by the above license.

# How to get started

    git clone git@github.com:emilk/wfc.git
    cd wfc
    ./main.cpp

`./main.cpp` starts with a shell script that downloads dependencies, build, links, and finally runs the program.

This works on Mac and Linux.

# Requirements
C++14. Nothing more, really.

All required third-party libraries are included or downloaded by running `./main.cpp`. These are:

* https://github.com/emilk/configuru (`.cfg` loading)
* https://github.com/emilk/emilib (small helpers for looping and string formating)
* https://github.com/emilk/loguru (logging and asserts)
* https://github.com/nothings/stb (image read/write)
* http://www.jonolick.com/home/gif-writer (write gif files)

# Performance
The sample configuration finishes **25% faster** than the original version (50 vs 40 seconds).

Tested on a Linux VM, speed may be better on an installed distribution.

# Limitations
This port supports everything in https://github.com/mxgmn/WaveFunctionCollapse (as of October 2016),
though with slightly different input ([.cfg files](https://github.com/emilk/Configuru) over .xml, for instance).

The code is not optimized nor well-documented. It could also do with some further cleanup.
