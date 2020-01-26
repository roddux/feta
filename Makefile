MODULE_NAME := hackmod
EXTRA_CFLAGS += -I$(src)

ifeq ($(KERNELRELEASE),)
# out of kernel

MODULE_SOURCES := fetamod.c ioctl.c

all: $(MODULE_NAME).ko olive

olive: olive_c.c
	gcc olive_c.c -o olive

$(MODULE_NAME).ko: $(MODULE_SOURCES)
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

.PHONY: all clean

else
# in-kernel / kbuild shit
ccflags-y :=  -I$(src)

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := fetamod.o ioctl.o

endif
