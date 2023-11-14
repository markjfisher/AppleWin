#pragma once

#include <vector>
#include <stdint.h>
#include "Response.h"

class StatusResponse : public Response
{
private:
	std::vector<uint8_t> status_values_;

public:
	virtual std::vector<uint8_t> serialize() const override;

	// status_values
	const std::vector<uint8_t>& get_status_values() const { return status_values_; }
	void set_status_values(const std::vector<uint8_t>& status_values) { status_values_ = status_values; }
	void add_status_value(uint8_t status_value) { status_values_.push_back(status_value); }
};
