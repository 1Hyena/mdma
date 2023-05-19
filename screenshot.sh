#!/bin/bash
./mdma README.md >README.html
chromium --headless --screenshot=screenshot.png README.html
magick screenshot.png -gamma 0.8 -alpha set -virtual-pixel transparent -channel A -blur 0x8 -level 50%,100% +channel screenshot.png
