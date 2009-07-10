#!/bin/bash
echo h=localhost duration=10s $1 -x -m $2 >>log
h=localhost duration=10s $1 -x -m $2 2>>log 1>>log


