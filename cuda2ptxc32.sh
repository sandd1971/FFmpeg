#!/bin/bash

for f in $(find . -name '*.cu'); do
  basename="${f%.*}"
  nvcc -arch compute_30 -code sm_30 -m 32 -ptx -o $basename.ptx $basename.cu || exit 1
  sed -i 's/.version 6.5/.version 3.2/g' $basename.ptx || exit 1
  ./ffbuild/bin2c.exe $basename.ptx ./SMP32/$basename.ptx.c || exit 1
  rm $basename.ptx || exit 1
done