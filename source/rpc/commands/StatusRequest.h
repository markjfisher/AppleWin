#pragma once

#include "Request.h"
#include "Response.h"
#include <cstdint>
#include <vector>

class StatusRequest : public Request
{
public:
	StatusRequest(uint8_t request_sequence_number, uint8_t device_id, uint8_t status_code);
	virtual std::vector<uint8_t> serialize() const override;
	std::unique_ptr<Response> deserialize(const std::vector<uint8_t> &data) const override;

	uint8_t get_status_code() const { return status_code_; }

private:
	uint8_t status_code_;
};
