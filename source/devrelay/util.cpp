#include "util.h"

void hexDump(const void* data, size_t size) {
    const unsigned char* byte = reinterpret_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
        // Print offset at the start of each line
        if (i % 16 == 0) {
            std::cout << std::setfill('0') << std::setw(4) << std::hex << i << ": ";
        }

        // Print byte in hex
        std::cout << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte[i] << " ";

        // Break line after 16 bytes or at the end
        if ((i + 1) % 16 == 0 || i == size - 1) {
            std::cout << std::endl;
        }
    }
}