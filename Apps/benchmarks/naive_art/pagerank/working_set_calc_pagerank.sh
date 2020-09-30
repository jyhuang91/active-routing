#!/bin/bash
echo -e "\n"
CMD=`ps -ax | grep "./pagerank" | grep -v "grep\|home"`
PID=`echo $CMD | awk '{print $1}'`
WSS=`cat /proc/${PID}/status | grep -i rss`
TOT_MEM=`cat /proc/${PID}/status | grep -i vmsize`
echo $CMD
echo Working set size: ${WSS} 
echo Total memory requirement: ${TOT_MEM}
echo -e "\n"
