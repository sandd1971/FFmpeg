#!/bin/bash

for f in $(find . -name '*.cu'); do
  basename="${f%.*}"
  nvcc -arch compute_30 -code sm_30 -m 64 -ptx -o $basename.ptx $basename.cu || exit 1
  ./compat/cuda/ptx2c.sh ./SMP64/$basename.ptx.c $basename.ptx || exit 1
  rm $basename.ptx || exit 1
done