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

# Start server
serverpid=""
if pgrep uperf ; then
    echo "uperf server already running; please stop it and try again"
    exit
else
    echo "Starting server - $uperf -s $csocket"
    $uperf -s $csocket &
    serverpid=$!
fi

echo h=$host duration=10s $uperf $csocket -m $profile >>log
h=$host duration=10s $uperf $csocket -m $profile >> log 2>&1
exitstatus=$?

# kill server
if [[ serverpid != "" ]] ; then
   echo "Killing $serverpid"
   kill -9 $serverpid
fi
exit $exitstatus
