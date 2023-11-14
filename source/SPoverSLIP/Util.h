#pragma once

#include <vector>
#include <cstdint>

class Util
{
public:
	static void hex_dump(const std::vector<uint8_t>& packet);
};
