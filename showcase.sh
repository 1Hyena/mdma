#!/bin/bash
cd showcase
chromium --headless --screenshot=ericherstu.net.jpg https://ericherstu.net/
mogrify -gamma 0.8 -raise 1 -raise 2 -raise 3 *.jpg
