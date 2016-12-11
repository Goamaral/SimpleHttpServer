#!/bin/bash
max=10000
ip=localhost
port=8080
sleep_time=0

for (( i=1; i<=max; ++i )) do
    echo "Request $i of $max"
    curl -s "$ip:$port/index.html.gz" > /dev/null
    sleep $sleep_time
done
