#!/bin/bash
cd showcase
chromium --headless --force-device-scale-factor=1 --window-size=1280,720 --screenshot=ericherstu.net.jpg https://ericherstu.net/
chromium --headless --force-device-scale-factor=1 --window-size=1280,720 --screenshot=stonia.net.ee.jpg https://stonia.net.ee/
mogrify -trim -gamma 0.8 -raise 1 -raise 2 -raise 3 *.jpg
