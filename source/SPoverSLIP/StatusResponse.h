#pragma once

#include <vector>
#include <stdint.h>
#include "Response.h"

class StatusResponse : public Response
{
private:
	std::vector<uint8_t> data_;

public:
	virtual std::vector<uint8_t> serialize() const override;

	const std::vector<uint8_t>& get_data() const { return data_; }
	void add_data(uint8_t d) { data_.push_back(d); }
};
