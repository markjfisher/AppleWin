
#include "Request.h"

Request::Request(const uint8_t request_sequence_number, const uint8_t command_number, const uint8_t sp_unit) : Packet(request_sequence_number), command_number_(command_number), sp_unit_(sp_unit) {}

uint8_t Request::get_command_number() const { return command_number_; }

uint8_t Request::get_sp_unit() const { return sp_unit_; }
