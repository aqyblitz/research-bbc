#include <iostream>
#include <stdint.h>

int main()
{
    union {
        uint32_t i;
        char c[4];
    } b_int = {0x01020304};

    bool print = b_int.c[0] == 1;

    std::cout << print << std::endl;
}
