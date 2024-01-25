#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "Request.h"
#include "Response.h"

class ReadBlockRequest : public Request
{
public:
  ReadBlockRequest(uint8_t request_sequence_number, uint8_t sp_unit);
  std::vector<uint8_t> serialize() const override;
  std::unique_ptr<Response> deserialize(const std::vector<uint8_t> &data) const override;
  const std::array<uint8_t, 3> &get_block_number() const;
  void set_block_number_from_ptr(const uint8_t *ptr, size_t offset);
  void set_block_number_from_bytes(uint8_t l, uint8_t m, uint8_t h);

private:
  std::array<uint8_t, 3> block_number_;
};
