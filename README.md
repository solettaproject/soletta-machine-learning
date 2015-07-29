# Soletta Machine Learning

[![Coverity Scan Build Status](https://scan.coverity.com/projects/5845/badge.svg)](https://scan.coverity.com/projects/5845)

**Soletta Machine Learning** is an open source machine learning library
for development of IoT devices.
It provides APIs to handle with client side AI and an easy to use flow-based
Soletta module.

Initially supporting neural networks and fuzzy logic learning,
using well established open source libraries, it could be easily
extended to support others.

# Building

## Dependencies

This project depends on:

 * fuzzylite:
    * Desc: A fuzzy logic control library written in C++.
    * [Site](http://www.fuzzylite.com/)
    * [Source](https://github.com/fuzzylite/fuzzylite.git)

 * FANN (Fast Artificial Neural Network Library):
    * Desc: An open source neural network library.
    * [Site](http://leenissen.dk/fann/wp/)
    * [Source](https://github.com/libfann/fann)

 * Soletta:
    * Desc: Soletta Project is a framework for making IoT devices.
    * [Site](https://solettaproject.org/)
    * [Source](https://github.com/solettaproject/soletta)

Some distros (like Fedora, Ubuntu), has packaged FANN and fuzzylite.

## Linux

Build and install all dependencies
At the moment, it's required to do a minor fix on fuzzylite/fuzzylite.pc.in:
Replace

    Cflags: -I${includedir}/fl

by

    Cflags: -I${includedir}

Alternativelly it's possible to provide a cmake with extra cflags:

    $ cmake -DCMAKE_C_FLAGS="-I/path/to/proper/header/dir/"


After dependencies setup and installation, build machine-learning running:
Using both engines:

    $ cmake -DFUZZY_ENGINE=ON -DANN_ENGINE=ON .

or

    $ cmake .
    $ make

Using only the neural networks engine:

    $ cmake -DFUZZY_ENGINE=OFF .
    $ make


Using only the Fuzzy engine:

    $ cmake -DANN_ENGINE=OFF .
    $ make

## Docs
To build docs run:

    $ make doc

## Galileo

 * Install proper toolchain to build for galileo board.
 * Edit soletta_module/i586-poky-linux-uclibc.cmake and update
   CMAKE_FIND_ROOT_PATH variable to point the toolchain's sysdir with any
   necessary dependencies to build sml.
 * Create temporary directory to install sml and its dependencies. It is
   called in this instruction {TMP_DESTDIR_PATH}
 * To use soletta_module/i586-poky-linux-uclibc.cmake to build fann and
   fuzzylite and install them to the toolchain's ROOT_PATH. Run:

        $ cmake .. \
          -DCMAKE_TOOLCHAIN_FILE={PATH_TO_SML}soletta_module/i586-poky-linux-uclibc.cmake \
          -DCMAKE_INSTALL_PREFIX:PATH=/usr
        $ make && make install DESTDIR={TMP_DESTDIR_PATH}

 * Copy all files in {TMP_DESTDIR_PATH} to toolchain's sysdir.
 * Use same command to build sml and to install it to {TMP_DESTDIR_PATH}
 * Copy all files in {TMP_DESTDIR_PATH} to root of image to be used in galileo
   board.
