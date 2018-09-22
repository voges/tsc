# TSC

TNT Sequence Compressor

---

TSC was tested on the following systems:

| Operating system                                          | Compiler(s)                                                              |
| --------------------------------------------------------- | ------------------------------------------------------------------------ |
| openSUSE Leap 15.0                                        | gcc version 7.3.1 20180323 \[gcc-7-branch revision 258812\] (SUSE Linux) |

## Quick start on Linux

Clone the TSC repository with

    git clone https://github.com/voges/tsc.git

Build the executable from the command line with the following commands; alternatively use the CMake GUI.

    cd tsc
    mkdir build
    cd build
    cmake ..
    make

This generates an executable named ``tsc`` in the ``build`` folder.

## Who do I talk to?

Jan Voges <[voges@tnt.uni-hannover.de](mailto:voges@tnt.uni-hannover.de)>
