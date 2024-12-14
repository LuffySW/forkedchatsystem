#!/bin/bash

for i in {1..15}
do
    ./client "Client_$i" batch &
    sleep 0.1
done
wait