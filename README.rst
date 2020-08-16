libe2c
-----------

libe2c is a generic synchronous BFT state-machine replication library that is best case optimal. This code is based on the code from [libhotstuff](https://github.com/hot-stuff/libhotstuff).

Paper
=====

- Full paper: Coming Soon

Playing with the code!
=======================

NOTE: This is an academic implementation. The authors do not provide any guarantees whatsoever of any kind. The
sections below may be incomplete and subject to changes as we make more progress.

::

    # install from the repo
    git clone https://github.com/adithyabhatkajake/libe2c.git
    cd libe2c/
    git submodule update --init --recursive

    # Package Requirements
    #
    # CMake >= 3.9 (cmake)
    # C++14 (g++)
    # libuv >= 1.10.0 (libuv1-dev)
    # openssl >= 1.1.0 (libssl-dev)
    #
    # on Ubuntu: sudo apt-get install libssl-dev libuv1-dev cmake make
    # on Arch Linux: sudo pacman -S libssl-dev libuv1-dev cmake make (TEST)

    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED=ON -DE2C_PROTOCOL_LOG=ON
    make

    # Scripts to run a demo with 4 nodes
    # Script to start a client


    # Fault tolerance:
    # Script to demo Fault tolerance
