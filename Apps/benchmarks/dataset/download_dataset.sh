#!/bin/sh

mkdir snap

cd snap
wget https://snap.stanford.edu/data/web-Google.txt.gz
gunzip web-Google.txt.gz
cd ..
