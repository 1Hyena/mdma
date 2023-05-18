#!/bin/bash
./mdma README.md >README.html
chromium --headless --screenshot=screenshot.jpg README.html
rm ./README.html
