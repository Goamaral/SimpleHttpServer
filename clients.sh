#!/bin/bash
max=100
ip=localhost
port=8080
sleep_time=1

for (( i=1; i<=max; ++i )) do 
    echo "Request $i of $max"
    curl -s "$ip:$port" > /dev/null
    sleep $sleep_time
done
