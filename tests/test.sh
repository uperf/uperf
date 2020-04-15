#!/usr/bin/env bash
export host=${h:-localhost}
export uperf=${UPERF:-$1}
echo h=$host duration=10s $uperf -m $2 >>log
h=$host duration=10s $uperf -m $2 >> log 2>&1


