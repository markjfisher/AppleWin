/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2022, Andrea Odetti

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// ReSharper disable CppInconsistentNaming

#include <StdAfx.h>
#include <string>
#include <memory>

#include "YamlHelper.h"
#include "SmartPortOverSlip.h"

#include "Core.h"
#include "Interface.h"
#include "../Registry.h"

#include "Memory.h"
#include "CPU.h"
#include "Log.h"
#include "../resource/resource.h"
#include "SPoverSLIP/CloseRequest.h"
#include "SPoverSLIP/CloseResponse.h"
#include "SPoverSLIP/ControlRequest.h"
#include "SPoverSLIP/ControlResponse.h"
#include "SPoverSLIP/FormatRequest.h"
#include "SPoverSLIP/FormatResponse.h"
#include "SPoverSLIP/InitRequest.h"
#include "SPoverSLIP/InitResponse.h"
#include "SPoverSLIP/OpenRequest.h"
#include "SPoverSLIP/OpenResponse.h"
#include "SPoverSLIP/ReadBlockRequest.h"
#include "SPoverSLIP/ReadBlockResponse.h"
#include "SPoverSLIP/ReadRequest.h"
#include "SPoverSLIP/ReadResponse.h"
#include "SPoverSLIP/ResetRequest.h"
#include "SPoverSLIP/ResetResponse.h"
#include "SPoverSLIP/Requestor.h"
#include "SPoverSLIP/StatusRequest.h"
#include "SPoverSLIP/StatusResponse.h"
#include "SPoverSLIP/WriteBlockRequest.h"
#include "SPoverSLIP/WriteBlockResponse.h"
#include "SPoverSLIP/WriteRequest.h"
#include "SPoverSLIP/WriteResponse.h"

int SmartPortOverSlip::active_instances = 0;

const std::string& SmartPortOverSlip::GetSnapshotCardName()
{
	static const std::string name("SmartPortOverSlip");
	LogFileOutput("SmartPortOverSlip Returning name SmartPortOverSlip\n");
	return name;
}

SmartPortOverSlip::SmartPortOverSlip(const UINT slot) : Card(CT_SmartPortOverSlip, slot)
{
	if (active_instances > 0) {
		throw std::runtime_error("There is an existing slot active for SP over SLIP. You can only have 1 of these cards active.");
	}
	active_instances++;

	LogFileOutput("SmartPortOverSlip ctor, slot: %d\n", slot);
	// create_listener();
	SmartPortOverSlip::Reset(true);
}

SmartPortOverSlip::~SmartPortOverSlip()
{
	active_instances--;
	LogFileOutput("SmartPortOverSlip destructor\n");
	//if (listener_ != nullptr)
	//{
	//	listener_->stop();
	//}
	//listener_.reset();
}

void SmartPortOverSlip::Reset(const bool powerCycle)
{
	LogFileOutput("SmartPortOverSlip Bridge Initialization, reset called\n");
	if (powerCycle)
	{
		// send RESET to all devices
                const auto connections = GetSPoverSLIPListener().get_all_connections();
		for (const auto& id_and_connection : connections) {
			reset(id_and_connection.first, id_and_connection.second);
		}
	}
}

void SmartPortOverSlip::handle_write()
{
	// stack pointer location holds the data we need to service this request
	WORD rts_location = static_cast<WORD>(mem[regs.sp + 1]) + static_cast<WORD>(mem[regs.sp + 2] << 8);
	const BYTE command = mem[rts_location + 1];
	const WORD cmd_list_loc = mem[rts_location + 2] + (mem[rts_location + 3] << 8);
	// BYTE paramCount = mem[cmdListLoc]; // parameter count not used
	const BYTE unit_number = mem[cmd_list_loc + 1];
	const WORD sp_payload_loc = static_cast<WORD>(mem[cmd_list_loc + 2]) + static_cast<WORD>(mem[cmd_list_loc + 3] << 8);
	const WORD params_loc = cmd_list_loc + 4;

	// LogFileOutput("SmartPortOverSlip processing SP command: 0x%02x, unit: "
	//	"0x%02x, cmdList: 0x%04x, spPayLoad: 0x%04x, p1: 0x%02x\n", 
	//	command, unit_number, cmd_list_loc, sp_payload_loc, mem[params_loc]);

	// Fix the stack so the RTS in the firmware returns to the instruction after the data
	rts_location += 3;
	mem[regs.sp + 1] = rts_location & 0xff;
	mem[regs.sp + 2] = (rts_location >> 8) & 0xff;

	// Deal with DIB, doesn't need connection details.
	if (unit_number == 0 && mem[params_loc] == 0)
	{
		device_count(sp_payload_loc);
		return;
	}

	const auto id_and_connection = GetSPoverSLIPListener().find_connection_with_device(unit_number);
	if (id_and_connection.second == nullptr)
	{
		regs.a = 1;		// TODO: what value should we error with?
		regs.x = 0;
		regs.y = 0;
		unset_processor_status(AF_ZERO);
		return;
	}

	const auto lower_device_id = id_and_connection.first;
	const auto connection = id_and_connection.second.get();

	// convert the unit_number given to the id of the target connection, as we may have more than one connection
	const auto target_unit_id = static_cast<uint8_t>(unit_number - lower_device_id + 1);

	switch (command) {
	case SP_CMD_STATUS:
		status(target_unit_id, connection, sp_payload_loc, mem[params_loc]);
		break;
	case SP_CMD_READBLOCK:
		read_block(target_unit_id, connection, sp_payload_loc, params_loc);
		break;
	case SP_CMD_WRITEBLOCK:
		write_block(target_unit_id, connection, sp_payload_loc, params_loc);
		break;
	case SP_CMD_FORMAT:
		format(target_unit_id, connection);
		break;
	case SP_CMD_CONTROL:
		control(target_unit_id, connection, sp_payload_loc, mem[params_loc]);
		break;
	case SP_CMD_INIT:
		init(target_unit_id, connection);
		break;
	case SP_CMD_OPEN:
		open(target_unit_id, connection);
		break;
	case SP_CMD_CLOSE:
		close(target_unit_id, connection);
		break;
	case SP_CMD_READ:
		read(target_unit_id, connection, sp_payload_loc, params_loc);
		break;
	case SP_CMD_WRITE:
		write(target_unit_id, connection, sp_payload_loc, params_loc);
		break;
	case SP_CMD_RESET:
		reset(target_unit_id, connection);
		break;
	default:
		break;
	}

}

BYTE SmartPortOverSlip::io_write0(WORD programCounter, const WORD address, const BYTE value, ULONG nCycles)
{
	const uint8_t loc = address & 0x0f;
	// Only do something if $65 is sent to address $02
	if (value == 0x65 && loc == 0x02)
	{
		handle_write();
	}
	return 0;
}

void SmartPortOverSlip::device_count(const WORD sp_payload_loc)
{
	// Fill the status information directly into SP payload memory.
	// The count is from sum of all devices across all Connections.
	const BYTE deviceCount = Listener::get_total_device_count();
	mem[sp_payload_loc] = deviceCount;
	mem[sp_payload_loc + 1] = 1 << 6; // no interrupt
	mem[sp_payload_loc + 2] = 0x4D;   // 0x4D46 == MF for vendor ID
	mem[sp_payload_loc + 3] = 0x46;
	mem[sp_payload_loc + 4] = 0x0A;   // version 1.00 Alpha = $100A
	mem[sp_payload_loc + 5] = 0x10;
	regs.a = 0;
	regs.x = 6;
	regs.y = 0;
	set_processor_status(AF_ZERO);
}

void SmartPortOverSlip::read_block(const BYTE unit_number, Connection* connection, const WORD sp_payload_loc, const WORD params_loc)
{
	ReadBlockRequest request(Requestor::next_request_number(), unit_number);
	// Assume that (cmd_list_loc + 4 == params_loc) holds 3 bytes for the block number. If it's in the payload, this is wrong and will have to be fixed.
	request.set_block_number_from_ptr(mem, params_loc);
	auto response = Requestor::send_request(request, connection);

	handle_response<ReadBlockResponse>(std::move(response), [this, sp_payload_loc](const ReadBlockResponse* rbr) {
		memcpy(mem + sp_payload_loc, rbr->get_block_data().data(), 512);
		regs.a = 0;
		regs.x = 0;
		regs.y = 2; // 512 bytes
	});
}

void SmartPortOverSlip::write_block(const BYTE unit_number, Connection* connection, const WORD sp_payload_loc, const WORD params_loc)
{
	WriteBlockRequest request(Requestor::next_request_number(), unit_number);
	// Assume that (cmd_list_loc + 4 == params_loc) holds 3 bytes for the block number. The payload contains the data to write
	request.set_block_number_from_ptr(mem, params_loc);
	request.set_block_data_from_ptr(mem, sp_payload_loc);

	auto response = Requestor::send_request(request, connection);
	handle_simple_response<WriteBlockResponse>(std::move(response));
}

void SmartPortOverSlip::read(const BYTE unit_number, Connection* connection, const WORD sp_payload_loc, const WORD params_loc)
{
	ReadRequest request(Requestor::next_request_number(), unit_number);
	request.set_byte_count_from_ptr(mem, params_loc);
	request.set_address_from_ptr(mem, params_loc + 2); // move along by byte_count size. would be better to get its size rather than hard code it here.
	auto response = Requestor::send_request(request, connection);

	handle_response<ReadResponse>(std::move(response), [sp_payload_loc](const ReadResponse* rr) {
		const auto response_size = rr->get_data().size();
		memcpy(mem + sp_payload_loc, rr->get_data().data(), response_size);
		regs.a = 0;
		regs.x = response_size & 0xff;
		regs.y = (response_size >> 8) & 0xff;
	});
}

void SmartPortOverSlip::write(const BYTE unit_number, Connection* connection, const WORD sp_payload_loc, const WORD params_loc)
{
	WriteRequest request(Requestor::next_request_number(), unit_number);
	request.set_byte_count_from_ptr(mem, params_loc);
	request.set_address_from_ptr(mem, params_loc + 2); // move along by byte_count size. would be better to get its size rather than hard code it here.
	const auto byte_count = request.get_byte_count();
	const auto write_length = byte_count[0] + (byte_count[1] << 8);
	request.set_data_from_ptr(mem, sp_payload_loc, write_length);

	auto response = Requestor::send_request(request, connection);
	handle_simple_response<WriteResponse>(std::move(response));
}

void SmartPortOverSlip::status(const BYTE unit_number, Connection* connection, const WORD sp_payload_loc, const BYTE status_code)
{
	// see https://www.1000bit.it/support/manuali/apple/technotes/smpt/tn.smpt.2.html
	const StatusRequest request(Requestor::next_request_number(), unit_number, status_code);
	auto response = Requestor::send_request(request, connection);
	handle_response<StatusResponse>(std::move(response), [sp_payload_loc](const StatusResponse* sr) {
		const auto response_size = sr->get_data().size();
		memcpy(mem + sp_payload_loc, sr->get_data().data(), response_size);
		regs.a = 0;
		regs.x = response_size & 0xff;
		regs.y = (response_size >> 8) & 0xff;
	});
}

void SmartPortOverSlip::control(const BYTE unit_number, Connection* connection, const WORD sp_payload_loc, const BYTE control_code)
{
	const auto length = mem[sp_payload_loc] + (mem[sp_payload_loc + 1] << 8) + 2;
	uint8_t* start_ptr = &mem[sp_payload_loc];
	std::vector<uint8_t> payload(start_ptr, start_ptr + length);

	const ControlRequest request(Requestor::next_request_number(), unit_number, control_code, payload);
	auto response = Requestor::send_request(request, connection);
	handle_simple_response<ControlResponse>(std::move(response));
}

void SmartPortOverSlip::init(const BYTE unit_number, Connection* connection)
{
	const InitRequest request(Requestor::next_request_number(), unit_number);
	auto response = Requestor::send_request(request, connection);
	handle_simple_response<InitResponse>(std::move(response));
}

void SmartPortOverSlip::open(const BYTE unit_number, Connection* connection)
{
	const OpenRequest request(Requestor::next_request_number(), unit_number);
	auto response = Requestor::send_request(request, connection);
	handle_simple_response<OpenResponse>(std::move(response));
}

void SmartPortOverSlip::close(const BYTE unit_number, Connection* connection)
{
	const CloseRequest request(Requestor::next_request_number(), unit_number);
	auto response = Requestor::send_request(request, connection);
	handle_simple_response<CloseResponse>(std::move(response));
}

void SmartPortOverSlip::reset(const BYTE unit_number, Connection* connection)
{
	const ResetRequest request(Requestor::next_request_number(), unit_number);
	auto response = Requestor::send_request(request, connection);
	handle_simple_response<ResetResponse>(std::move(response));
}

void SmartPortOverSlip::format(const BYTE unit_number, Connection* connection)
{
	const FormatRequest request(Requestor::next_request_number(), unit_number);
	auto response = Requestor::send_request(request, connection);
	handle_simple_response<FormatResponse>(std::move(response));
}


BYTE __stdcall c0Handler(const WORD programCounter, const WORD address, const BYTE write, const BYTE value, const ULONG nCycles)
{
	const UINT uSlot = ((address & 0xf0) >> 4) - 8;

	if (uSlot < 8) {
		auto* pCard = static_cast<SmartPortOverSlip*>(MemGetSlotParameters(uSlot));
		if (write) {
			return pCard->io_write0(programCounter, address, value, nCycles);
		}
	}
	return 0;
}

void SmartPortOverSlip::InitializeIO(LPBYTE pCxRomPeripheral)
{
	LogFileOutput("SmartPortOverSlip InitialiseIO\n");

	// Load firmware into chosen slot
	const BYTE* pData = GetFrame().GetResource(IDR_SPOVERSLIP_FW, "FIRMWARE", APPLE_SLOT_SIZE);
	if (pData == nullptr)
		return;

	// Copy the data into the destination memory
	std::memcpy(pCxRomPeripheral + m_slot * APPLE_SLOT_SIZE, pData, APPLE_SLOT_SIZE);

	// Set location in firmware that need to know the slot number for reading/writing to the card
	const BYTE locCN1 = pData[0xF9]; 	// location of where to put $n2
	const BYTE locCN2 = pData[0xFA]; 	// location of where to put $n2
	const BYTE cn = ((m_slot & 0xff) | 0xC0); // slot(n) || 0xC0 = 0xCn

	const BYTE locN2 = pData[0xFB]; 	// location of where to put $n2
	const BYTE n2 = static_cast<BYTE>(((m_slot & 0xff) | 0x08) << 4) + 0x02; // (slot + 8) * 16 + 2, giving low byte of $C0n2

	// Modify the destination memory to hold the slot information needed by the firmware.
	// The pData memory is R/O, and we get an access violation writing to it, so alter the destination instead.
	const LPBYTE pDest = pCxRomPeripheral + m_slot * APPLE_SLOT_SIZE;
	pDest[locCN1] = cn;
	pDest[locCN2] = cn;
	pDest[locN2] = n2;

	RegisterIoHandler(m_slot, nullptr, c0Handler, nullptr, nullptr, this, nullptr);
}

void SmartPortOverSlip::Update(const ULONG nExecutedCycles)
{
	// LogFileOutput("SmartPortOverSlip Update\n");
}


void SmartPortOverSlip::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	LogFileOutput("SmartPortOverSlip SaveSnapshot\n");
}

bool SmartPortOverSlip::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	LogFileOutput("SmartPortOverSlip LoadSnapshot\n");
	return true;
}

//void SmartPortOverSlip::create_listener()
//{
//	// grab the configured IP and Port TODO
//
//	//listener_ = std::make_unique<Listener>("0.0.0.0", 1985);
//	//listener_->start();
//	//LogFileOutput("SmartPortOverSlip Created SP over SLIP listener on 0.0.0.0:1985\n");
//}

void SmartPortOverSlip::Destroy()
{
	// Stop the listener and its connections
	//if (listener_ != nullptr)
	//{
	//	listener_->stop();
	//}
}
