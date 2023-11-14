#pragma once

#include <vector>
#include <stdint.h>
#include "SPoSLIP.h"

class Packet : public SPoSLIP
{
public:
	virtual std::vector<uint8_t> serialize() const = 0;
};
