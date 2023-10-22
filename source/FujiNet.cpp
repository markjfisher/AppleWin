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
	memset(buffer, 0, W5100_MEM_SIZE);
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

BYTE FujiNet::IOWrite0(WORD programcounter, WORD address, BYTE value, ULONG nCycles)
{
	LogFileOutput("FUJINET IOWrite: PC: %02x, address: %02x, value: %d\n", programcounter, address, value);
	BYTE res = 1;

	return res;
}

BYTE FujiNet::IORead0(WORD programcounter, WORD address, ULONG nCycles)
{
	LogFileOutput("FUJINET IORead: PC: %02x, address: %02x\n", programcounter, address);

	return 0;
}

BYTE FujiNet::IOWriteX(WORD programcounter, WORD address, BYTE value, ULONG nCycles)
{
	BYTE res = 1;
	LogFileOutput("FUJINET IOWrite: PC: %02x, address: %02x, value: %d\n", programcounter, address, value);

	return res;
}

BYTE FujiNet::IOReadX(WORD programcounter, WORD address, ULONG nCycles)
{
	LogFileOutput("FUJINET IORead: PC: %02x, address: %02x\n", programcounter, address);

	BYTE lo = address & 0xff;
	switch (lo) {
	case 1: return 0x20;
		break;
	case 3: return 0x00;
		break;
	case 5: return 0x03;
		break;
	case 7: return 0x3C; // or $00 for SP?
		break;
	}

	return 0x69;
}

BYTE __stdcall c0Handler(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
	UINT uSlot = ((address & 0xf0) >> 4) - 8;
	LogFileOutput("FUJINET c0Handler PC: %02x, address: %02x, write: %u, value: %u, cycles: %lu, slot: %u\n", programcounter, address, write, value, nCycles, uSlot);

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

BYTE __stdcall cXHandler(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
	UINT uSlot = (address >> 8) & 0x0f;
	LogFileOutput("FUJINET cXHandler PC: %02x, address: %02x, write: %u, value: %u, cycles: %lu, slot: %u\n", programcounter, address, write, value, nCycles, uSlot);

	if (uSlot < 8) {
		FujiNet* pCard = (FujiNet*)MemGetSlotParameters(uSlot);
		if (write) {
			return pCard->IOWriteX(programcounter, address, value, nCycles);
		}
		else {
			return pCard->IOReadX(programcounter, address, nCycles);
		}
	}
	return 0;
}

void FujiNet::InitializeIO(LPBYTE pCxRomPeripheral)
{
	LogFileOutput("FUJINET InitialiseIO\n");
	RegisterIoHandler(m_slot, c0Handler, c0Handler, cXHandler, cXHandler, this, nullptr);

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