#include "StdAfx.h"

#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip>

#include "Util.h"

void Util::hex_dump(const std::vector<uint8_t>& data)
{
	std::vector<uint8_t>::size_type offset = 0;
	int bytes_per_line = 16;

	while (offset < data.size())
	{
		std::cout << std::hex << offset << ": ";

		for (int i = 0; i < bytes_per_line && offset + i < data.size(); i++)
		{
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)data[offset + i] << " ";
		}

		std::cout << std::endl;

		offset += bytes_per_line;
	}
	std::cout << std::dec;
}
