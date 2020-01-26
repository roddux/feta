# FETA
Feta is a low-level, memory-bashing ATA fuzzer. Issues ATA commands.

idea is to boot straight into FETA as an init parameter. Nothing else to worry about. no processes, etc...

feta would enumerate the PCI interfaces available and select what is hopefully the only one (VBox setup depending)
- don't necessarily need to rice it with a custom kernel for this... could just supply init=/bin/fuzzer
- fuzzing using /dev/mem instead of writing a kernel module could be MUCH easier (?)
  - python fuzzing anyone?

---
feta:

mem = open("/dev/mem",rw)
write_mem(data,offset):
    fseek(mem,offset)
    fwrite(mem,data)

ata_select_drive()
ata_read()
ata_write()
ata_detect_drive()
ata_probe() <- 

https://wiki.osdev.org/AHCI
AHCI is the future. IDE is legacy... don't waste time trying to implement IDE shiz.

step 1:
iterate the PCI bus looking for an AHCI controller
for X in pci_enumerate():
	if X.class_id = 0x1:
		# found an AHCI controller

# AHCI controller through system memory and memory mapped registers. Encapsulates SATA and provides it via a 
# PCI interface to the host.

# an ahci controller (on the pc) is known as a Host Bus Adapter (HBA).

# no need to fuck around with taskfiles

# ahci controllers can support up to 32 "ports" <-- what are ports?
# each port can attach a SATA device; like a disk, port multiplier, etc...

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

