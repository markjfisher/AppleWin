#pragma once

#include <stdint.h>

class SPoSLIP
{
private:
	uint8_t request_sequence_number_;

public:
	uint8_t get_request_sequence_number() const { return request_sequence_number_; }

	void set_request_sequence_number(uint8_t request_sequence_number)
	{
		request_sequence_number_ = request_sequence_number;
	}
};
