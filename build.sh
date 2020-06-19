#!/usr/bin/env bash
set -Eeuo pipefail

YELLOW="\e[1;32m"
RED="\e[1;31m"
CLEAR="\e[0m"
echop () { echo -e "$YELLOW[+] $1$CLEAR"; }
echoe () { echo -e "$RED[!] $1$CLEAR"; }

#clear
echop "Removing fetadrv module (if it's loaded)"
rmmod fetadrv.ko 2>/dev/null || true

#echo "[+] Cleaning build directory"
#make clean &>/dev/null

echop "Building feta"
makelog=`mktemp`
r=0; make &>$makelog || r=$?
if [ ! $r -eq 0 ]; then
	echoe "Build failed! Check output below:"
	cat $makelog; rm $makelog
	exit 1
fi

echop "Clearing dmesg output"
dmesg --clear

echop "Inserting fetadrv module"
r=0; insmod fetadrv.ko || r=$?
if [ ! $r -eq 0 ]; then
	echoe "Could not insert module!"
	exit 1
fi

echop "Rebinding AHCI device to use fetadrv module"
./rebind.sh

# sync

#echop "Running olive.py"
#./olive.py

echop "fetadrv output from dmesg:"
dmesg
