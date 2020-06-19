# FETA
Feta is an ATA fuzzer. More specifically, it fuzzes the AHCI controller.

At some point i'll rip the logic from this and incorporate it into a UEFI program for rapid fuzzing. This serves as
a prototype toward that goal.

https://wiki.osdev.org/AHCI

# Rough theory of operation
## Iterate the PCI bus and find an AHCI controller
iterate the PCI bus looking for an AHCI controller
for X in pci_enumerate():
	if X.class_id = 0x1:
		# found an AHCI controller

With the AHCI controller found, we might need to adjust (or set, if UEFI/BIOS hasn't done it) the memory addresses
used for communication. We communicate with the device primarily via memory-mapped IO. SOME communication (PIO stuff)
is done directly through port IO.

The AHCI controller is our access into the ATA pipeline. We hand over our FIS packets and let the HBA do the legwork.
Similar situation for ISA/IDE(?).

The AHCI controller is known as a Host Bus Adapter (HBA).

# The last PCI base address register (BAR[5], header offset 0x24) points to the AHCI base memory, called 
# ABAR (AHCI Base Memory Register).

# All AHCI registers and memories can be located through ABAR.
# The other PCI base address registers act same as a traditional IDE controller

so we found the controller. which basically means we found an offset ?

# HBA memory registers can be divided into two parts: host control and port control.
# host control changes how the whoole controller works
# port control changes how an individual port works
step 2:
host control = ABAR+00

port 0 = ABAR+100
# see struct tagHBA_MEM on osdev link above

# host sends commands to the device using command list
step 2:
    device = ABAR+100 (port 0)
    device.clb and device.clbu make up he pointer to the command list
# "the most important part of AHCI init is correctly setting these pointers and the data structures they point TO"
    host writes command to COMMAND_POINT = 0xdevice.clbu +0xdevice.clv <-- expand on this...

    host writes a command list. 1 to 32 command headers, called slots
    each slot describes an ATA/ATAPI command, including a command FIS

   to send a cmd: host constructs a command header
   sets the bit on the Port Command Issue Register
    AHCI controller sends the command

step 3:
   host receives an FIS back from the device
   FIS is copied into memory, then an interrupt is raised <-- how do we capture this interrupt? (Linux IRQs)
   interrupt is "raised"? a bit is set in the port interrupt status register
    
so it's possible to read/write directly to the PCI bar address from userland, using /sys/bus/pci/devices/00:11 ?
how would we figure out IRQs etc? hmmm. 
UIO has methods, but dunno if they're enough. Linux Userspace IO project.
okay so regardless... the commands.

linux image fun:
boot normal kernel (optimised for sp33d)
copy required files into tmpfs/ram
pivot_root into the ramfs
unbind the ahci module
bind your own ahci module (FETA)

or

boot normal kernel
but init=/bin/start_feta ?


-----
kernel module to trap IO access to the specific regions? then just run fs-noise generator as usual...
mprotect() the PCI range associated with the AHCI device
install a signal handler to catch SIGSEGV events for that range


----
pci driver module? hmmmmmm...
gain sole access to the appropriate memory region: pci_request_region and ioremap (?)

-----

/*
0: boot vm, insert kernel module (feta), rebind (the sole) pci-ahci device to use fetadrv module
1: feta takes posession of the device (fetadrv_init)
2: feta initialises the device (feta_enable_pci_device)
3: userland coponent (olive) tells fuzz handler it's starting (with ID and a new random seed. see: iofuzz net handler)
4: olive issues START_FUZZ ioctl to feta and gives a seed (olive.py, using IOCTL values from olive_c)
5: feta runs fuzz round as instructed
6: feta signals to olive that the fuzz round has completed
7: olive calls out to net handler to say that seed has completed
8: goto 4
*/



