#!/usr/bin/env bash

YELLOW="\e[1;32m"
RED="\e[1;31m"
CLEAR="\e[0m"
echop () { echo -e "$YELLOW[+] $1$CLEAR"; }
echoe () { echo -e "$RED[!] $1$CLEAR"; }

dmesg --clear;

echop "Removing fetadrv"
rmmod fetadrv.ko 2>/dev/null;
dmesg; dmesg --clear;

echop "Inserting fetadrv"
insmod fetadrv.ko;
dmesg; dmesg --clear;

echop "Rebinding..."
./rebind.sh;
dmesg; dmesg --clear;

exit

#echop "Runing olive.py"
#./olive.py
#dmesg; dmesg --clear;
