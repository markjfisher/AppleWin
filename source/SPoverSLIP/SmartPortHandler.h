#pragma once

//#include <memory>
//#include "StatusRequest.h"
//#include "StatusResponse.h"

// Only need the defines for the Request creation

#define SP_STATUS        0
#define SP_READ_BLOCK    1
#define SP_WRITE_BLOCK   2
#define SP_FORMAT        3
#define SP_CONTROL       4
#define SP_INIT          5
#define SP_OPEN          6
#define SP_CLOSE         7
#define SP_READ          8
#define SP_WRITE         9
#define SP_RESET         10

//class SmartPortHandler {
//public:
//  virtual ~SmartPortHandler() = default;
//
//  virtual std::unique_ptr<Response> status(StatusRequest* request) = 0;
//  // virtual std::unique_ptr<Response> read_block(ReadBlockRequest* request) = 0;
//  // virtual std::unique_ptr<Response> write_block(WriteBlockRequest* request) = 0;
//  // virtual std::unique_ptr<Response> format(FormatRequest* request) = 0;
//  // virtual std::unique_ptr<Response> control(ControlRequest* request) = 0;
//  // virtual std::unique_ptr<Response> init(InitRequest* request) = 0;
//  // virtual std::unique_ptr<Response> open(OpenRequest* request) = 0;
//  // virtual std::unique_ptr<Response> close(CloseRequest* request) = 0;
//  // virtual std::unique_ptr<Response> read(ReadRequest* request) = 0;
//  // virtual std::unique_ptr<Response> write(WriteRequest* request) = 0;
//  // virtual std::unique_ptr<Response> reset(ResetRequest* request) = 0;
//};
