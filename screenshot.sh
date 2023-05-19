#!/bin/bash
./mdma README.md >README.html
chromium --headless --screenshot=screenshot.jpg README.html
magick screenshot.jpg -gamma 0.8 -raise 4 -raise 8 -raise 12 screenshot.jpg
