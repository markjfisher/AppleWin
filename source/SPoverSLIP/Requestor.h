#pragma once

#include <memory>
#include "Connection.h"
#include "Request.h"
#include "Response.h"

class Requestor
{
public:
	Requestor();

	// The Request's deserialize function will always return a Response, e.g. StatusRequest -> StatusResponse
	static std::unique_ptr<Response> send_request(const Request& request, Connection* connection);
	static uint8_t next_request_number();

private:
	static uint8_t request_number_;
};
