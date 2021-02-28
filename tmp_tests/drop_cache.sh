#!/bin/bash

sync
sleep 0.5
echo 3 > /proc/sys/vm/drop_caches
sleep 0.5
free -h
echo
