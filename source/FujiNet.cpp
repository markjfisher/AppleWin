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
	createListener();
}

FujiNet::~FujiNet()
{
	LogFileOutput("FujiNet destructor\n");
	if (listener_ != nullptr)
	{
		LogFileOutput("FujiNet dtor, stopping listener\n");
		listener_->stop();
		LogFileOutput("FujiNet dtor, ... stopped\n");
	}
	listener_.reset();
}

void FujiNet::Reset(const bool powerCycle)
{
	LogFileOutput("FujiNet Bridge Initialization, reset called\n");
	if (powerCycle)
	{
		// may have to send RESET here
	}
}

void FujiNet::processSPoverSLIP()
{
	// stack pointer location holds the data we need to service this request
	WORD rtsLocation = mem[regs.sp + 1] + (mem[regs.sp + 2] << 8);
	const BYTE command = mem[rtsLocation + 1];
	const WORD cmdListLoc = mem[rtsLocation + 2] + (mem[rtsLocation + 3] << 8);
	// BYTE paramCount = mem[cmdListLoc];
	const BYTE unitNumber = mem[cmdListLoc + 1];
	const WORD spPayloadLoc = mem[cmdListLoc + 2] + (mem[cmdListLoc + 3] << 8);
	const WORD paramsLoc = cmdListLoc + 4;

	LogFileOutput("FujiNet processing SP command: 0x%02x, unit: 0x%02x, cmdList: 0x%04x, spPayLoad: 0x%04x, p1: 0x%02x\n", command, unitNumber, cmdListLoc, spPayloadLoc, mem[paramsLoc]);

	// Fix the stack so the RTS in the firmware returns to the instruction after the data
	rtsLocation += 3;
	mem[regs.sp + 1] = rtsLocation & 0xff;
	mem[regs.sp + 2] = (rtsLocation >> 8) & 0xff;

	switch (command) {
	case SP_CMD_STATUS:
		status(unitNumber, spPayloadLoc, paramsLoc);
		break;
	default:
		break;
	}

}

BYTE FujiNet::IOWrite0(WORD programCounter, WORD address, BYTE value, ULONG nCycles)
{
	//LogFileOutput("FujiNet IOWrite0: PC: %02x, address: %02x, value: %d\n", programCounter, address, value);
	const uint8_t loc = address & 0x0f;
	// Only do something if $65 is sent to address $02
	if (value == 0x65 && loc == 0x02)
	{
		processSPoverSLIP();
	}
	return 0;
}

void FujiNet::deviceCount(WORD spPayloadLoc)
{
	// Fill the status information directly into SP payload memory.
	// The count is from sum of all devices across all Connections.
	// TODO: to remove the initial "here are my devices" code, this should change to querying its connections for a count of devices, i.e. do a "0"/"0" request.
	// TODO: doing this dynamically here ensures we catch any changes to a device, or connections that are lost.
	const BYTE deviceCount = Listener::get_total_device_count();
	mem[spPayloadLoc] = deviceCount;
	mem[spPayloadLoc + 1] = 1 << 6;	// no interrupt
	mem[spPayloadLoc + 2] = 0x4D;   // 0x4D46 == MF for vendor ID
	mem[spPayloadLoc + 3] = 0x46;
	mem[spPayloadLoc + 4] = 0x0A;   // version 1.00 Alpha = $100A
	mem[spPayloadLoc + 5] = 0x10;
	regs.a = 0;
	regs.x = 6;
	regs.y = 0;
}

void FujiNet::dib(BYTE unit_number, WORD sp_payload_loc) const
{
	// send a request for the DIB through the connection, if this unit number is connected
	const auto id_and_connection = listener_->find_connection_with_device(unit_number);
	if (id_and_connection.second == nullptr)
	{
		regs.a = 1;		// TODO: what value should we error with?
		regs.x = 0;
		regs.y = 0;
		return;
	}

	const auto lower_device_id = id_and_connection.first;
	const auto connection = id_and_connection.second;

	// convert the unit_number given to the id of the target connection, as we may have more than one connection
	const auto target_unit_id = static_cast<uint8_t>(unit_number - lower_device_id + 1);

	StatusRequest request(Requestor::next_request_number(), target_unit_id, 3);	// DIB request
	const auto response = Requestor::send_request(request, connection.get());
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
		} else
		{
			// An error in the response
			regs.a = status_response->get_status();
			regs.x = 0;
			regs.y = 0;
		}
	}
}

// https://www.1000bit.it/support/manuali/apple/technotes/smpt/tn.smpt.2.html
void FujiNet::status(const BYTE unitNumber, const WORD spPayloadLoc, const WORD paramsLoc) const
{
	const BYTE statusCode = mem[paramsLoc];

	if (unitNumber == 0 && statusCode == 0)
	{
		deviceCount(spPayloadLoc);
	}
	else if (unitNumber != 0 && statusCode == 3)
	{
		dib(unitNumber, spPayloadLoc);
	}
	else
	{
		// regular status for this device that needs to go to a connected device
	}
}

BYTE __stdcall c0Handler(WORD programCounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
	const UINT uSlot = ((address & 0xf0) >> 4) - 8;

	if (uSlot < 8) {
		auto* pCard = static_cast<FujiNet*>(MemGetSlotParameters(uSlot));
		if (write) {
			return pCard->IOWrite0(programCounter, address, value, nCycles);
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

void FujiNet::createListener()
{
	listener_ = std::make_unique<Listener>("0.0.0.0", 1985);
	listener_->start();
	LogFileOutput("FujiNet Created SP over SLIP listener on 0.0.0.0:1985\n");
}

void FujiNet::Destroy()
{
	if (listener_ != nullptr)
	{
		listener_->stop();
	}
}
