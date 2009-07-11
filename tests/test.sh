#!/bin/bash
export host=${h:-localhost}
echo h=$host duration=10s $1 -x -m $2 >>log
h=$host duration=10s $1 -x -m $2 >> log 2>&1


