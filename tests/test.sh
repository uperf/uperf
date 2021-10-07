#!/usr/bin/env bash
export host=${h:-localhost}
export uperf=${UPERF:-$1}

profile=$2
csocket=

if [[ "$profile" == *".vsock.xml" ]]; then
    csocket="-S vsock"
    if [[ "$host" == "localhost" ]]; then
        host="1"
    fi
fi

echo h=$host duration=10s $uperf $csocket -m $profile >>log
h=$host duration=10s $uperf $csocket -m $profile >> log 2>&1


