/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2021, Tom Charlesworth, Michael Pohoreski

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

#include "StdAfx.h"
#include "Card.h"

#include "Tfe/tfe.h"
#include "Mockingboard.h"
#include "ParallelPrinter.h"
#include "z80emu.h"


void DummyCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	switch (QueryType())
	{
	case CT_GenericPrinter:
		PrintLoadRom(pCxRomPeripheral, m_slot);
		break;
	case CT_Uthernet:
		tfe_InitializeIO(pCxRomPeripheral, m_slot);
		break;
	case CT_GenericClock:
		break; // nothing to do
	case CT_MockingboardC:
	case CT_Phasor:
		// only in slot 4
		if (m_slot == SLOT4)
		{
			MB_InitializeIO(pCxRomPeripheral, SLOT4, SLOT5);
		}
		break;
	case CT_Z80:
		Z80_InitializeIO(pCxRomPeripheral, m_slot);
		break;
	}
}