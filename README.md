# Soletta Machine Learning

[![Coverity Scan Build Status](https://scan.coverity.com/projects/5845/badge.svg)](https://scan.coverity.com/projects/5845)
[![Build Status](https://semaphoreci.com/api/v1/projects/bf8a2e2d-7645-4092-911d-26756acfbbce/527006/shields_badge.svg)](https://semaphoreci.com/solettaproject/soletta-machine-learning)

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
