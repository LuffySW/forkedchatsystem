#!/bin/bash

for i in {1..29}
do
    ./clientChat "Client$i" batch &
    sleep 0.1
done
wait