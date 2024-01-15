#!/bin/bash

cl65 -t none spoverslip.s -o spoverslip.bin
if [ $? -ne 0 ] ; then
  echo "Error compiling firmware."
  exit 1
fi

cp spoverslip.bin ../../resource/