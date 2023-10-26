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
	// 02 : destination
	// 03 : payload lo
	// 04 : payload hi
	// 05 : param 1
	// 06 : param 2
	// 07+: payload data if required

	// Payload data contains:
	
	// CONTROL COMMAND
	// 00 : Length (lo)
	// 01 : Length (hi)
	// 02 : Mode (e.g. R/W = $0C)
	// 03 : Translation (0 = None, etc)
	// 04 .. 04 + (Len - 2) = nul terminated Device Spec

	// OPEN/CLOSE
	// N/A

	// READ/WRITE
	// DATA for Write, or to be sent back for Read.
	// The length is in Buffer[5,6]

	// STATUS
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
	backup[backupIndex++] = v;
}

// Treat backup like FIFO, not stack
BYTE FujiNet::restoreData()
{
	return backup[restoreIndex++];
}

BYTE FujiNet::IOWrite0(WORD programcounter, WORD address, BYTE value, ULONG nCycles)
{
	LogFileOutput("FUJINET IOWrite0: PC: %02x, address: %02x, value: %d\n", programcounter, address, value);
	const uint8_t loc = address & 0x0f;

	// Location:
	// 0x00 - 0x0C = write data to buffer (to allow the Y index in firmware to move with the bytes being copied, as there's only a few bytes to copy)
	// 0x0D        = clear buffer and reset index
	// 0x0E        = save data (zp location backup)
	// 0x0F        = process buffer

	if (loc < 0xD) {
		buffer[bufferLen++] = value;
	}
	else if (loc == 0xD) {
		resetBuffer();
	}
	else if (loc == 0xE) {
		backupData(value);
	}
	else if (loc == 0xF) {
		process();
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
	// 2 = next byte
	// 3 = restore backup data

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

	// write the (slot number + 8) * 16 into the firmware code at location 13. This ensures it can work in any slot given
	pData[13] = (BYTE) (((m_slot & 0xff) | 0x08) << 4);

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