#pragma once

#include <vector>
#include <stdint.h>
#include "Packet.h"
#include "Request.h"

class Response : public Packet
{
public:
	virtual std::vector<uint8_t> serialize() const override = 0;

	uint8_t get_status() const { return status_; }
	void set_status(uint8_t status) { status_ = status; }

private:
	uint8_t status_ = 0;
};
