#include "Request.h"

Request::Request(const uint8_t request_sequence_number, const uint8_t command_number, const uint8_t device_id) : Command(request_sequence_number), command_number_(command_number), device_id_(device_id) {}

uint8_t Request::get_command_number() const { return command_number_; }

uint8_t Request::get_device_id() const { return device_id_; }
