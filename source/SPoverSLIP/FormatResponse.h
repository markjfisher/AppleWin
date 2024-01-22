#pragma once
#include "Response.h"

class FormatResponse : public Response
{
public:
  explicit FormatResponse(uint8_t request_sequence_number, uint8_t status);
  std::vector<uint8_t> serialize() const override;
};
