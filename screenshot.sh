#!/bin/bash
chromium --headless --force-device-scale-factor=1 --window-size=1280,720 --screenshot=screenshot.jpg framework.html
magick screenshot.jpg -trim -gamma 0.8 -raise 1 -raise 2 -raise 3 screenshot.jpg
