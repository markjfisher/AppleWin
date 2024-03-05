add_compile_definitions(DEV_RELAY_SLIP=TRUE)

set(SLIP_PROTOCOL "NET" CACHE STRING "Select the protocol type (NET or COM)")

set_property(CACHE SLIP_PROTOCOL PROPERTY STRINGS "NET" "COM")

if(NOT SLIP_PROTOCOL STREQUAL "NET" AND NOT SLIP_PROTOCOL STREQUAL "COM")
  message(FATAL_ERROR "Invalid value for SLIP_PROTOCOL: ${SLIP_PROTOCOL}. Please choose either NET or COM.")
endif()

# convert to values for C++ code to use as macros
if(SLIP_PROTOCOL STREQUAL "NET")
    add_compile_definitions(SLIP_PROTOCOL_NET=1)
elseif(SLIP_PROTOCOL STREQUAL "COM")
    add_compile_definitions(SLIP_PROTOCOL_COM=1)
endif()
message(STATUS "SLIP_PROTOCOL is ${SLIP_PROTOCOL}")
