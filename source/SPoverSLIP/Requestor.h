#pragma once

#include <memory>
#include "Connection.h"
#include "Listener.h"
#include "Request.h"
#include "Response.h"

class Requestor
{
public:
	Requestor() = default;

	// The Request's deserialize function will always return a Response, e.g. StatusRequest -> StatusResponse
	// The request will be mutated 
	static std::unique_ptr<Response> send_request(Request& request, Connection* connection);
	static uint8_t next_request_number();

private:
	static uint8_t request_number_;
};
