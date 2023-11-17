#pragma once

#include <vector>
#include <stdint.h>
#include "Request.h"
#include "Response.h"

class StatusRequest : public Request
{
public:
	StatusRequest(uint8_t request_sequence_number, uint8_t sp_unit, uint8_t status_code);
	virtual std::vector<uint8_t> serialize() const override;
	virtual std::unique_ptr<Response> deserialize(const std::vector<uint8_t>& data) const;

	uint8_t get_status_code() const { return status_code_; }
	void set_status_code(uint8_t status_code) { status_code_ = status_code; }

private:
	uint8_t status_code_;
};
