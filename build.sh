#!/bin/sh -eu

make ${MAKE_OPTS:-} -C kernel kernel.elf
make ${MAKE_OPTS:-} -C kernel hypervisor.elf


for MK in $(ls libs/*/Makefile)
do
  LIB_DIR=$(dirname $MK)
  LIB=$(basename $LIB_DIR)
  make ${MAKE_OPTS:-} -C $LIB_DIR $LIB
done


for MK in $(ls apps/*/Makefile)
do
  APP_DIR=$(dirname $MK)
  APP=$(basename $APP_DIR)
  make ${MAKE_OPTS:-} -C $APP_DIR $APP
done

DISK_IMG=./disk.img MIKANOS_DIR=$PWD $MIKAN_HOME/osbook/devenv/make_mikanos_image.sh

if [ "${1:-}" = "run" ]
then
  $MIKAN_HOME/osbook/devenv/run_image.sh ./disk.img
fi
