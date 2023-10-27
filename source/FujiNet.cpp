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

#include <StdAfx.h>
#include <string>
#include <fstream>
#include <streambuf>

#include "YamlHelper.h"
#include "FujiNet.h"
#include "Interface.h"
#include "W5100.h"
#include "../Registry.h"

#include "StrFormat.h"
#include "Memory.h"
#include "Log.h"
#include "../resource/resource.h"

#include <iostream>
#include <urlmon.h>
#pragma comment(lib,"urlmon.lib")

const std::string& FujiNet::GetSnapshotCardName()
{
	static const std::string name("FujiNet");
	LogFileOutput("FUJINET Returning name FujiNet\n");
	return name;
}

FujiNet::FujiNet(UINT slot) : Card(CT_FujiNet, slot)
{
	LogFileOutput("FUJINET ctor, slot: %d\n", slot);
	Reset(true);
}

FujiNet::~FujiNet()
{
	LogFileOutput("FUJINET dtor\n");
}

void FujiNet::resetBuffer()
{
	memset(buffer, 0, 1024);
	memset(backup, 0, 24);
	bufferLen = 0;
	bufferReadIndex = 0;
	backupIndex = 0;
	restoreIndex = 0;
}

void FujiNet::Reset(const bool powerCycle)
{
	LogFileOutput("FUJINET Bridge Initialization, reset called\n");
	resetBuffer();

	if (powerCycle)
	{

	}
}

void FujiNet::device_count()
{
	// write "6" back to the A2 when it asks for a value.
	LogFileOutput("FUJINET device count, len: 1, buff: 6\n");
	bufferLen = 1;
	buffer[0] = 6;
}

void FujiNet::dib(uint8_t dest)
{
	LogFileOutput("FUJINET dib, len: 7, buff: NETWORK@5\n");
	bufferLen = 7+5; // payload seems to be sent back to payload+5, but we copy initial 5 bytes back too.
	strcpy((char *) buffer+5, "NETWORK");
	buffer[4] = 7; // not sure this is used, not in network_library anyway.
}

void FujiNet::process()
{
	LogFileOutput("FUJINET processing %d bytes\n", bufferLen);

	// Buffer has following data:
	// 00 : command
	// 01 : command parameter count
	// 02 : destination (the device number of the FujiNet target, e.g. "NETWORK" may be 2, "PRINTER" 3, etc)
	// 03 : payload lo (used only by firmware)
	// 04 : payload hi (used only by firmware)
	// 05 : param 1 (used by control/status, read(l), write (l))
	// 06 : param 2 (used by read(h), write (h))
	// 07+: payload data if required

	// Payload data contains:
	
	// CONTROL COMMAND (READ FROM PAYLOAD, NO WRITE BACK FOR FIRMWARE) [4 + deviceSpec.size()]
	// 00 : Length (lo)
	// 01 : Length (hi)
	// 02 : Mode (e.g. R/W = $0C)
	// 03 : Translation (0 = None, etc)
	// 04 .. 04 + (Len - 2) = nul terminated Device Spec

	// OPEN/CLOSE (NO PAYLOAD USE) [0]
	// N/A

	// READ (WRITE BACK TO FIRMWARE) [0]
	// The length is in Buffer[5,6]

	// WRITE (READ FROM PAYLOAD FOR SENDING TO FN) [length specified as below]
	// The length is in Buffer[5,6]
	// Data in Buffer[7+]

	// STATUS (WRITES TO PAYLOAD) [0]
	// Length -> Payload[0,1]

	// To start, get enough working fake without calling actual FN.
	// STATUS 0, DEST  = 0                          -> do device count
	// STATUS 0, DEST != 0, PARAM1 = statuscode = 3 -> DIB, use DEST to fetch the device name
	// othewise normal STATUS

	// everything else is other commands

	uint8_t command = buffer[0];
	uint8_t dest = buffer[2];
	uint8_t p1 = buffer[5];

	if (command == 0 && dest == 0) {
		device_count();
	}
	else if (command == 0 && dest != 0 && p1 == 3) {
		dib(dest);
	}

}

void FujiNet::backupData(BYTE v)
{
	if (backupIndex < 24) {
		backup[backupIndex++] = v;
	}
}

// Treat backup like FIFO, not stack
BYTE FujiNet::restoreData()
{
	if (restoreIndex < 24) {
		return backup[restoreIndex++];
	}
	return 0;
}

void FujiNet::addToBuffer(BYTE v)
{
	if (bufferLen < 1024) {
		buffer[bufferLen++] = v;
	}
}

BYTE FujiNet::IOWrite0(WORD programcounter, WORD address, BYTE value, ULONG nCycles)
{
	LogFileOutput("FUJINET IOWrite0: PC: %02x, address: %02x, value: %d\n", programcounter, address, value);
	const uint8_t loc = address & 0x0f;

	// Location:
	// 0x00 - 0x0D = write data to buffer (to allow the Y index in firmware to move with the bytes being copied, as there's only a few bytes to copy)
	// 0x0E        = store given value in backup array (zp location backup)
	// 0x0F        = special command, choose from value:
	//                0 = reset buffer + indexes
	//                1 = process buffer

	switch (loc) {
	case 0xE:
		backupData(value);
		break;
	case 0xF:
		switch (value) {
		case 0:
			resetBuffer();
			break;
		case 1:
			process();
			break;
		}
		break;
	default:
		addToBuffer(value);
		break;
	}

	return 0;
}

BYTE FujiNet::IORead0(WORD programcounter, WORD address, ULONG nCycles)
{
	const uint8_t loc = address & 0x0f;
	BYTE res = 0;

	// Location:
	// 0 = lo byte of bufferLen
	// 1 = hi byte of bufferLen
	// 2 = read next byte from buffer
	// 3 = read from backup data

	switch (loc) {
	case 0:
		res = (BYTE)(bufferLen & 0x00ff);
		break;
	case 1:
		res = (BYTE)((bufferLen >> 8) && 0xff);
		break;
	case 2:
		if (bufferReadIndex < bufferLen) {
			res = buffer[bufferReadIndex++];
		}
		break;
	case 3:
		res = restoreData();
		break;
	}

	LogFileOutput("FUJINET IORead0: [%04x] = %02x (PC: %04x)\n", address, res, programcounter);
	return res;
}

BYTE __stdcall c0Handler(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
	UINT uSlot = ((address & 0xf0) >> 4) - 8;

	if (uSlot < 8) {
		FujiNet* pCard = (FujiNet*)MemGetSlotParameters(uSlot);
		if (write) {
			return pCard->IOWrite0(programcounter, address, value, nCycles);
		}
		else {
			return pCard->IORead0(programcounter, address, nCycles);
		}
	}
	return 0;
}

void FujiNet::InitializeIO(LPBYTE pCxRomPeripheral)
{
	LogFileOutput("FUJINET InitialiseIO\n");

	const DWORD HARDDISK_FW_SIZE = APPLE_SLOT_SIZE;

	// Install firmware into chosen slot
	BYTE* pData = GetFrame().GetResource(IDR_FUJINET_FW, "FIRMWARE", HARDDISK_FW_SIZE);
	if (pData == NULL)
		return;

	// set 2 locations in firmware that need to know the slot number for reading/writing to the card
	BYTE loc1_ne = pData[0xFD]; 	// location of where to put $nE, used for backup of zp values
	BYTE loc2_n0 = pData[0xFE]; 	// location of where to put $n0, used for "(slot),y"
	BYTE n0 = (((m_slot & 0xff) | 0x08) << 4); // (slot + 8) * 16, giving low byte of $C0n0
	pData[loc1_ne] = n0 + 0x0E;
	pData[loc2_n0] = n0;

	memcpy(pCxRomPeripheral + m_slot * APPLE_SLOT_SIZE, pData, HARDDISK_FW_SIZE);

	RegisterIoHandler(m_slot, c0Handler, c0Handler, nullptr, nullptr, this, nullptr);
}


void FujiNet::Update(const ULONG nExecutedCycles)
{
	// LogFileOutput("FUJINET Update\n");
}


void FujiNet::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	LogFileOutput("FUJINET SaveSnapshot\n");
}

bool FujiNet::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	LogFileOutput("FUJINET LoadSnapshot\n");
	return true;
}