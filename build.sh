#!/bin/bash

qmake && make clean && make -j && strip -s ./bin/pcscnfc
