#!/bin/bash
if [ $# -eq 0 ]; then
    echo "Usage: $0 html-file [output-file]"
    exit 1
fi
if [ $# -gt 1 ]; then
    OUTPUT_FILE=$2
else
    OUTPUT_FILE="$1.string"
fi
minify $1 --quiet -o $1.min.html
echo "R\"^~(" > $OUTPUT_FILE
cat $1.min.html >> $OUTPUT_FILE
echo ")^~\"" >> $OUTPUT_FILE
rm $1.min.html
echo "Minified $1 to $OUTPUT_FILE"
