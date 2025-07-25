#!/bin/bash

find map/chunk_* -type d -name water | while read dir; do
  count=$(find "$dir" -type f | wc -l)
  echo "$dir: $count files"
done | sort -t ':' -k2 -n
