#pragma once
#include "Response.h"

class WriteResponse : public Response
{
public:
  explicit WriteResponse(uint8_t request_sequence_number, uint8_t status);
  std::vector<uint8_t> serialize() const override;
};
