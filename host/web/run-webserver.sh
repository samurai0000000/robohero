#!/bin/sh

python3 -m http.server 3000 --directory ./dist --bind 0.0.0.0
