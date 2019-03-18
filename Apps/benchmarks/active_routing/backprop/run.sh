#!/bin/bash
./backprop 2097152 8
#PID=`ps -ax | grep "./backprop" | grep -v grep | awk '{print $1}'`
#WSS=`cat /proc/$PID/status | grep -i rss`
#TOT_MEM=`cat /proc/$PID/status | grep -i vmsize`
#echo Working set size: ${WSS} 
#echo Total memory requirement: ${TOT_MEM}
