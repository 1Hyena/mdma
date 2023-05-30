#!/bin/bash
./mdma README.md >framework.html
chromium --headless --screenshot=screenshot.jpg framework.html
magick screenshot.jpg -gamma 0.8 -raise 1 -raise 2 -raise 3 screenshot.jpg
