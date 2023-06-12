#!/bin/bash
cd src
make clean
make
cd ..
./mdma --github README.md >framework.html
