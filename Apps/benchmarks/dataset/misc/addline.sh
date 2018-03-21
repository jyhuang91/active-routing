#!/bin/sh

# use example: ./addline.sh "# NodeIDs: 916428 Degree: 456 max_deg_node: 506742" file.txt

line="1i$1"
file=$2

sed -i "$line" $file
