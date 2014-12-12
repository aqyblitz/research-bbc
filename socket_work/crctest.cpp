#include <stdio.h>
#include <string>

#include "crcgen.h"

#define CHECK_VALUE 0xCBF43926

int main(void)
{
	unsigned char test[] = "123456789";
    crcgen gen = crcgen();
	/*
	 * Print the check value for the selected CRC algorithm.
	 */
	printf("The check value for the CRC-32 standard is 0x%X\n", CHECK_VALUE);

	/*
	 * Compute the CRC of the test message, more efficiently.
	 */
	printf("The computed CRC of \"123456789\" is 0x%X\n", gen.crc_compute(test, sizeof(test)-1));
    // What the hell? Why -1?

    return 0;
}   /* main() */
