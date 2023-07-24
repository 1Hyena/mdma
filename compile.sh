#!/bin/bash
cd src
make clean
make
cd ..
./mdma --preview 0 README.md >framework.html
