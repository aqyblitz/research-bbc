#include <stdint.h>
#include <string.h>

#ifndef CRCGEN_H
#define CRCGEN_H

#define POLYNOMIAL 0x04C11DB7
#define INITIAL_REMAINDER 0xFFFFFFFF
#define FINAL_XOR_VALUE 0xFFFFFFFF
typedef uint32_t crc;

class crcgen
{
    public:
        crcgen();
        ~crcgen();
        crc crc_compute(unsigned char const message[], int nBytes, uint32_t init_remainder=INITIAL_REMAINDER);
    private:
        int crcTable[256];
        uint32_t reflect(uint32_t data, uint8_t nBits);
};

#endif // CRCGEN_H
