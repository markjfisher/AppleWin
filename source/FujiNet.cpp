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
	bufferLen = 0;
	bufferReadIndex = 0;
}

void FujiNet::Reset(const bool powerCycle)
{
	LogFileOutput("FUJINET Bridge Initialization, reset called\n");
	resetBuffer();

	if (powerCycle)
	{

	}
}

void FujiNet::process()
{
	LogFileOutput("FUJINET processing %d bytes\n", bufferLen);
	uint8_t command = buffer[0];

}

BYTE FujiNet::IOWrite0(WORD programcounter, WORD address, BYTE value, ULONG nCycles)
{
	LogFileOutput("FUJINET IOWrite0: PC: %02x, address: %02x, value: %d\n", programcounter, address, value);
	const uint8_t loc = address & 0x0f;

	// Location:
	// 0 = clear buffer and reset index
	// 1 = write data to buffer
	// 2 = process buffer

	switch (loc) {
	case 0:
		resetBuffer();
		break;
	case 1:
		buffer[bufferLen++] = value;
		break;
	case 2:
		process();
		break;
	}

	return 0;
}

BYTE FujiNet::IORead0(WORD programcounter, WORD address, ULONG nCycles)
{
	LogFileOutput("FUJINET IORead0: PC: %02x, address: %02x\n", programcounter, address);
	const uint8_t loc = address & 0x0f;
	BYTE res = 0;

	// Location:
	// 0 = lo byte of bufferLen
	// 1 = hi byte of bufferLen
	// 2 = next byte

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
	}

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