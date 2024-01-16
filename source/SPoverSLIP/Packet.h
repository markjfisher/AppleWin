#pragma once

#include "SPoSLIP.h"
#include <stdint.h>
#include <vector>

class Packet : public SPoSLIP
{
public:
  explicit Packet(const uint8_t request_sequence_number) : SPoSLIP(request_sequence_number) {}
  virtual std::vector<uint8_t> serialize() const = 0;
};
