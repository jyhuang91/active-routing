#!/bin/sh

declare -a graphs
graphs=(web-Google) # can add more snap graphs

cd misc
make
cd ..

mkdir -p snap
cd snap

echo "Download graphs ..."

for graph in ${graphs[@]};
do
  wget https://snap.stanford.edu/data/$graph.txt.gz
  gunzip $graph.txt.gz
  ../misc/parseGraph $graph.txt >> temp
done

file="./temp"

echo "Start processing graphs ..."

i=0
while read -r line; do
  graph=${graphs[$i]}.txt
  i=$(( i + 1 ))
  ../misc/addline.sh "$line" $graph
  echo "$graph processed"
done < $file

echo "Graph processing finished"

rm temp

cd ..
