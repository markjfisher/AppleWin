#!/bin/bash

cl65 fujinet.s -C fujinet.cfg -o fujinet.bin
if [ $? -ne 0 ] ; then
  echo "Error compiling firmware."
  exit 1
fi

cp fujinet.bin ../../resource/