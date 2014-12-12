#include <stdio.h>
#include <string>
#include <netinet/in.h>
#include <cstdlib>

#include "crcgen.h"

#define CHECK_VALUE 0xCBF43926

int is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } b_int = {0x01020304};

    return b_int.c[0] == 1;
}

int main(void)
{
    //int32_t msg_info[] = {htonl(sizeof(int)), htonl(0), htonl(4), htonl(6)};
    //printf("%d", sizeof(msg_info));
    //unsigned char* test = reinterpret_cast<unsigned char*>(msg_info);
	unsigned char test[] = "123456789";
    crcgen gen = crcgen();
	/*
	 * Print the check value for the selected CRC algorithm.
	 */
	printf("The check value for the CRC-32 standard is 0x%X\n", CHECK_VALUE);

	/*
	 * Compute the CRC of the test message, more efficiently.
	 */
    //printf("%d",sizeof(test)-1);
    crc val = gen.crc_compute(test, sizeof(test)-1); // basically 4 chars?
    //crc val = gen.crc_compute(test, sizeof(msg_info));
	printf("The computed CRC of \"123456789\" is 0x%X\n", val);
	//int32_t msg_info_t[] = {htonl(sizeof(int)), htonl(0), htonl(4), htonl(6), val};
	//unsigned char* test_t = reinterpret_cast<unsigned char*>(msg_info_t);
    //printf("%d", sizeof(msg_info_t));
    unsigned char bytes[4];
    //if(!is_big_endian())
    //    val = ntohl(val);

    unsigned char check[sizeof(test)+4-1];
    for(int i=0; i<sizeof(test)-1; i++)
    {
        check[i] = test[i];
        printf("Byte test is 0x%X\n",check[i]);
    }
    for(int i=0; i<4; i++)
    {
        check[sizeof(test)-1+i] = (val >> (8*i)) & 0xFF;//(val >> ((i-1) << 3)) & 0xFF;
        printf("Byte test is 0x%X\n",check[sizeof(test)-1+i]);
    }
    //printf("%d",sizeof(check));
    //printf("Byte test is 0x%X\n",reinterpret_cast<uint32_t>(bytes));
	printf("The checked CRC of \"123456789\" is 0x%X\n", gen.crc_compute(check, sizeof(check)-1));

    // What the hell? Why -1?

    return 0;
}   /* main() */
