#!/usr/bin/env bash
#
# A build script for AppleWin Linux
#

function show_help {
  echo "Usage: $(basename $0) [options] -- [additional args]"
  echo ""
  echo "building options:"
  echo "   -b       # run build"
  echo ""
  echo "flags for building options"
  echo "   -c       # run clean before build"
  echo "   -g       # compile with debug enabled"
  echo "   -G GEN   # Use GEN as the Generator for cmake (e.g. -G \"Unix Makefiles\" )"
  echo ""
  echo "other options:"
  echo "   -h       # this help"
  echo ""
  echo "Additional Args can be accepted to pass values onto sub processes where supported."
  echo "  e.g. ./build.sh -cb -- -DFOO=BAR"
  echo ""
  exit 1
}

if [ $# -eq 0 ] ; then
  show_help
fi

DO_BUILD=0
DO_CLEAN=0
RELEASE_TYPE="Release"
CMAKE_GENERATOR=""

while getopts "bcgG:h" flag
do
  case "$flag" in
    b) DO_BUILD=1 ;;
    c) DO_CLEAN=1 ;;
    g) RELEASE_TYPE="Debug" ;;
    G) CMAKE_GENERATOR=${OPTARG} ;;
    h) show_help ;;
    *) show_help ;;
  esac
done
shift $((OPTIND - 1))

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if [ ! -d build ]; then
  mkdir build
fi

if [ $DO_CLEAN -eq 1 ] ; then
  echo "Removing old build artifacts"
  rm -rf $SCRIPT_DIR/build/*
  rm $SCRIPT_DIR/build/.ninja* 2>/dev/null
fi


if [ $DO_BUILD -eq 1 ] ; then
  echo "Building AppleWinLinux project"
  cd $SCRIPT_DIR/build

  GEN_CMD=""
  if [ -n "$CMAKE_GENERATOR" ] ; then
    GEN_CMD="-G $CMAKE_GENERATOR"
  fi

  cmake "$GEN_CMD" .. -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DBUILD_SA2=on "$@"

  echo "Building for $RELEASE_TYPE"
  cmake "$GEN_CMD" .. -DCMAKE_BUILD_TYPE=$RELEASE_TYPE -DBUILD_SA2=on "$@"
  if [ $? -ne 0 ] ; then
    echo "Error running initial cmake. Aborting"
    exit 1
  fi

  cmake --build .

  if [ $? -ne 0 ]; then
    echo "ERROR: Could not run cmake."
    exit 1
  fi

fi
