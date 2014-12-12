#include "crcgen.h"


#define WIDTH  (8 * sizeof(crc))
#define TOPBIT (1 << (WIDTH - 1))

#define REFLECT_DATA(X)       ((uint8_t) reflect((uint32_t) (X), (uint8_t) 8))
#define REFLECT_REMAINDER(X)  ((crc) reflect((uint32_t) (X), (uint8_t) WIDTH))

crcgen::crcgen()
{
    bzero(crcTable, sizeof(crcTable));

    crc remainder;
    int dividend;
    unsigned char bit;

    for (dividend = 0; dividend < 256; ++dividend)
    {
        remainder = dividend << (WIDTH - 8);

        for (bit = 8; bit > 0; --bit)
        {
            if (remainder & TOPBIT)
            {
                remainder = (remainder << 1) ^ POLYNOMIAL;
            }
            else
            {
                remainder = (remainder << 1);
            }
        }
        crcTable[dividend] = remainder;
    }
}

crcgen::~crcgen()
{

}

/*
* Check this against 12345689: 0xCBF43926
*/
crc crcgen::crc_compute(unsigned char const message[], int nBytes)
{
    //int n_blocks = nBytes/8;

    crc remainder = INITIAL_REMAINDER;
    unsigned char data;
	int byte;

    for (byte = 0; byte < nBytes; ++byte)
    {
        data = REFLECT_DATA(message[byte]) ^ (remainder >> (WIDTH - 8));
  		remainder = crcTable[data] ^ (remainder << 8);
    }

    return (REFLECT_REMAINDER(remainder) ^ FINAL_XOR_VALUE);

} /* crc_compute() */

/*
 * This makes some assumption about the data (namely that it doesn't exceed 16*8=128 bits. So keep that in mind.
 */
uint32_t crcgen::reflect(uint32_t data, uint8_t nBits)
{
    uint32_t reflection = 0x00000000;
    uint8_t bit;
    /*
     * Reflect the data about the center bit.
     */
    for (bit = 0; bit < nBits; ++bit)
    {
        /*
         * If the LSB bit is set, set the reflection of it.
         */
        if (data & 0x01)
        {
            reflection |= (1 << ((nBits - 1) - bit));
        }

        data = (data >> 1);
    }

    return (reflection);

}   /* reflect() */
