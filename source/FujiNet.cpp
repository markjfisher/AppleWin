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
#include "FujiNet.h"
#include "Interface.h"
#include "../Registry.h"

#include "Memory.h"
#include "CPU.h"
#include "Log.h"
#include "../resource/resource.h"
#include "SPoverSLIP/ControlRequest.h"
#include "SPoverSLIP/ControlResponse.h"
#include "SPoverSLIP/Requestor.h"
#include "SPoverSLIP/StatusRequest.h"
#include "SPoverSLIP/StatusResponse.h"

const std::string& FujiNet::GetSnapshotCardName()
{
	static const std::string name("FujiNet");
	LogFileOutput("FujiNet Returning name FujiNet\n");
	return name;
}

FujiNet::FujiNet(const UINT slot) : Card(CT_FujiNet, slot)
{
	LogFileOutput("FujiNet ctor, slot: %d\n", slot);
	Reset(true);
	create_listener();
}

FujiNet::~FujiNet()
{
	LogFileOutput("FujiNet destructor\n");
	if (listener_ != nullptr)
	{
		listener_->stop();
	}
	listener_.reset();
}

void FujiNet::Reset(const bool powerCycle)
{
	LogFileOutput("FujiNet Bridge Initialization, reset called\n");
	if (powerCycle)
	{
		// may have to send RESET here to all devices
	}
}

void FujiNet::process_sp_over_slip() const
{
	// stack pointer location holds the data we need to service this request
	WORD rts_location = mem[regs.sp + 1] + (mem[regs.sp + 2] << 8);
	const BYTE command = mem[rts_location + 1];
	const WORD cmd_list_loc = mem[rts_location + 2] + (mem[rts_location + 3] << 8);
	// BYTE paramCount = mem[cmdListLoc];
	const BYTE unit_number = mem[cmd_list_loc + 1];
	const WORD sp_payload_loc = mem[cmd_list_loc + 2] + (mem[cmd_list_loc + 3] << 8);
	const WORD params_loc = cmd_list_loc + 4;

	LogFileOutput("FujiNet processing SP command: 0x%02x, unit: 0x%02x, cmdList: 0x%04x, spPayLoad: 0x%04x, p1: 0x%02x\n", command, unit_number, cmd_list_loc, sp_payload_loc, mem[params_loc]);

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

	const auto id_and_connection = listener_->find_connection_with_device(unit_number);
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
	case SP_CMD_CONTROL:
		control(target_unit_id, connection, sp_payload_loc, mem[params_loc]);
		break;
	default:
		break;
	}

}

BYTE FujiNet::io_write0(WORD programCounter, WORD address, BYTE value, ULONG nCycles) const
{
	const uint8_t loc = address & 0x0f;
	// Only do something if $65 is sent to address $02
	if (value == 0x65 && loc == 0x02)
	{
		process_sp_over_slip();
	}
	return 0;
}

void FujiNet::device_count(WORD sp_payload_loc)
{
	// Fill the status information directly into SP payload memory.
	// The count is from sum of all devices across all Connections.
	// TODO: to remove the initial "here are my devices" code, this should change to querying its connections for a count of devices, i.e. do a "0"/"0" request.
	// TODO: doing this dynamically here ensures we catch any changes to a device, or connections that are lost.
	const BYTE deviceCount = Listener::get_total_device_count();
	mem[sp_payload_loc] = deviceCount;
	mem[sp_payload_loc + 1] = 1 << 6;	// no interrupt
	mem[sp_payload_loc + 2] = 0x4D;   // 0x4D46 == MF for vendor ID
	mem[sp_payload_loc + 3] = 0x46;
	mem[sp_payload_loc + 4] = 0x0A;   // version 1.00 Alpha = $100A
	mem[sp_payload_loc + 5] = 0x10;
	regs.a = 0;
	regs.x = 6;
	regs.y = 0;
	set_processor_status(AF_ZERO);
}

// https://www.1000bit.it/support/manuali/apple/technotes/smpt/tn.smpt.2.html
void FujiNet::status(BYTE unit_number, Connection* connection, WORD sp_payload_loc, BYTE status_code)
{
	// convert the unit_number given to the id of the target connection, as we may have more than one connection
	const auto target_unit_id = static_cast<uint8_t>(unit_number);

	StatusRequest request(Requestor::next_request_number(), target_unit_id, status_code);
	const auto response = Requestor::send_request(request, connection);
	const auto status_response = dynamic_cast<StatusResponse*>(response.get());
	if (status_response != nullptr)
	{
		if (status_response->get_status() == 0)
		{
			const auto response_size = status_response->get_data().size();
			// copy the response data into the SP payload memory
			memcpy(mem + sp_payload_loc, status_response->get_data().data(), response_size);
			regs.a = 0;
			regs.x = response_size & 0xff;
			regs.y = (response_size >> 8) & 0xff;
			set_processor_status(AF_ZERO);
		} else
		{
			// An error in the response
			regs.a = status_response->get_status();
			regs.x = 0;
			regs.y = 0;
			unset_processor_status(AF_ZERO);
		}
	}
	else
	{
		// An error trying to do the request, as there was no response
		regs.a = 1;		// TODO: what error should we return?
		regs.x = 0;
		regs.y = 0;
		unset_processor_status(AF_ZERO);
	}
}

void FujiNet::control(BYTE unit_number, Connection* connection, WORD sp_payload_loc, BYTE control_code)
{
	// CONTROL COMMAND (READ FROM PAYLOAD, NO WRITE BACK FOR FIRMWARE) [total bytes in Payload = 2 + Length given in 0/1]
	// 00 : Length (lo)
	// 01 : Length (hi) (Length = device spec size + 1 for nul + 2 for the mode/translation bytes. does not include 2 bytes for length data itself)
	// 02 : Mode (e.g. R/W = $0C)
	// 03 : Translation (0 = None, etc)
	// 04 .. 04 + (Len - 2) = nul terminated Device Spec

	const auto length = mem[sp_payload_loc] + (mem[sp_payload_loc + 1] << 8) + 2;
	uint8_t* start_ptr = &mem[sp_payload_loc];
	std::vector<uint8_t> payload(start_ptr, start_ptr + length);

	ControlRequest request(Requestor::next_request_number(), unit_number, control_code, payload);
	const auto response = Requestor::send_request(request, connection);
	const auto control_response = dynamic_cast<ControlResponse*>(response.get());

	if (control_response != nullptr)
	{
		const BYTE status = control_response->get_status();
		// There's only a status in a control response. Where does it go?
		regs.a = status;
		regs.x = 0;
		regs.y = 0;
		// set/unset ZERO flag according to status value
		update_processor_status(status == 0, AF_ZERO);

	}
	else
	{
		// An error trying to do the request, as there was no response
		regs.a = 1;		// TODO: what error should we return?
		regs.x = 0;
		regs.y = 0;
		unset_processor_status(AF_ZERO);
	}
}

BYTE __stdcall c0Handler(WORD programCounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
	const UINT uSlot = ((address & 0xf0) >> 4) - 8;

	if (uSlot < 8) {
		const auto* pCard = static_cast<FujiNet*>(MemGetSlotParameters(uSlot));
		if (write) {
			return pCard->io_write0(programCounter, address, value, nCycles);
		}
	}
	return 0;
}

void FujiNet::InitializeIO(LPBYTE pCxRomPeripheral)
{
	LogFileOutput("FujiNet InitialiseIO\n");

	// Load firmware into chosen slot
	const BYTE* pData = GetFrame().GetResource(IDR_FUJINET_FW, "FIRMWARE", APPLE_SLOT_SIZE);
	if (pData == nullptr)
		return;

	// Copy the data into the destination memory
	std::memcpy(pCxRomPeripheral + m_slot * APPLE_SLOT_SIZE, pData, APPLE_SLOT_SIZE);

	// Set location in firmware that need to know the slot number for reading/writing to the card
	const BYTE locCN1 = pData[0xF9]; 	// location of where to put $n2
	const BYTE locCN2 = pData[0xFA]; 	// location of where to put $n2
	const BYTE cn = ((m_slot & 0xff) | 0xC0); // slot(n) || 0xC0 = 0xCn

	const BYTE locN2 = pData[0xFB]; 	// location of where to put $n2
	const BYTE n2 = (((m_slot & 0xff) | 0x08) << 4) + 0x02; // (slot + 8) * 16 + 2, giving low byte of $C0n2

	// Modify the destination memory to hold the slot information needed by the firmware.
	// The pData memory is R/O, and we get an access violation writing to it, so alter the destination instead.
	const LPBYTE pDest = pCxRomPeripheral + m_slot * APPLE_SLOT_SIZE;
	pDest[locCN1] = cn;
	pDest[locCN2] = cn;
	pDest[locN2] = n2;

	RegisterIoHandler(m_slot, nullptr, c0Handler, nullptr, nullptr, this, nullptr);
}

void FujiNet::Update(const ULONG nExecutedCycles)
{
	// LogFileOutput("FujiNet Update\n");
}


void FujiNet::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	LogFileOutput("FujiNet SaveSnapshot\n");
}

bool FujiNet::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	LogFileOutput("FujiNet LoadSnapshot\n");
	return true;
}

void FujiNet::create_listener()
{
	listener_ = std::make_unique<Listener>("0.0.0.0", 1985);
	listener_->start();
	LogFileOutput("FujiNet Created SP over SLIP listener on 0.0.0.0:1985\n");
}

void FujiNet::Destroy()
{
	// Stop the listener and its connections
	if (listener_ != nullptr)
	{
		listener_->stop();
	}
}
