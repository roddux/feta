#!/usr/bin/env python3
"""
Olive, the controller for feta.

This file will act as the local machine handler, working with the networked API endpoints to register the
start of fuzzing runs, using which seeds, etc.

IOCTL values START_FUZZ etc are determined by compiling and running olive_c
"""
from fcntl import ioctl
from ctypes import c_ulonglong as uint64_t

START_FUZZ=0x007102
SET_SEED=0x40087103

print("[>] Olive - Feta controller")

print("[+] Opening control device /dev/feta")
fname = open("/dev/feta","r")

print("[+] Setting seed")
ioctl(fname, SET_SEED, uint64_t(0xffffffffffffffff))

print("[+] Starting fuzz")
ioctl(fname, START_FUZZ)
