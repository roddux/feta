#!/usr/bin/env bash
set euo pipefail

# I can't be arsed to insert fetadrv before the actual ahci driver loads, so instead
# we wait until the system is in a stable, booted state -- then we rebind the driver.

# Feta should be running using IDE/TFTP/etc as a main hard disk, with only one AHCI PCI
# device configured on the system.

echo "[+] Checking that there's only one AHCI device"
no_ahcis=`lspci|grep SATA|wc -l` # <- TODO: liable to break
if [ ! $no_ahcis -eq 1 ]; then
	echo "[!] Error: found more than one AHCI PCI device. Do NOT run this on live systems!"
	exit 1
fi

echo "[+] Grabbing id of the AHCI PCI device"
ahci=`lspci | awk '/SATA/{print "0000:"$1}'`
echo "[+] Got PCI device id '$ahci'"

if [ -d /sys/bus/pci/drivers/fetadrv/$ahci/ ]; then
	echo "[+] PCI device '$ahci' is already associated with driver 'fetadrv'.";
	exit 0
fi

if [ ! -d /sys/bus/pci/drivers/ahci/$ahci/ ]; then
	echo "[>] Warn: 'ahci' driver is not associated with PCI device '$ahci'.";
	UNBIND=NO
fi

if [ -z "$UNBIND" ]; then 
	echo "[+] Unbinding PCI device '$ahci' from driver 'ahci'"
	echo -n $ahci > /sys/bus/pci/drivers/ahci/unbind
fi
 
echo "[+] Binding PCI device '$ahci' to driver 'fetadrv'"
echo -n $ahci > /sys/bus/pci/drivers/fetadrv/bind
