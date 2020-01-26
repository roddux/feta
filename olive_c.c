#include <linux/ioctl.h>
#include <stdint.h>
/*
This file only exists to translate the below values into constants, for use in olive.py
*/

#define START_FUZZ _IO('q',  2)
#define SET_SEED _IOW('q', 3, uint64_t *)

void main() {
	printf("START_FUZZ: %#08x\n", START_FUZZ);
	printf("SET_SEED: %#08x\n", SET_SEED);
}
