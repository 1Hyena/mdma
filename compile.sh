#!/bin/bash
cd src
make clean
make
cd ..
./mdma README.md >framework.html
