#!/bin/bash
./mdma README.md >README.html
chromium --headless --screenshot=screenshot.jpg README.html
convert screenshot.jpg -gamma 0.8 screenshot.jpg
