/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

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

/* Description: main
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "CmdLine.h"
#include "Log.h"
#include "Core.h"
#include "Memory.h"
#include "LanguageCard.h"
#include "Keyboard.h"
#include "Joystick.h"
#include "SoundCore.h"
#include "SNESMAX.h"
#include "Interface.h"
#include "Harddisk.h"

CmdLine g_cmdLine;
std::string g_sConfigFile; // INI file to use instead of Registry

bool g_bCapturePrintScreenKey = true;
bool g_bRegisterFileTypes = true;
bool g_bHookSystemKey = true;
bool g_bHookAltTab = false;
bool g_bHookAltGrControl = false;


static LPSTR GetCurrArg(LPSTR lpCmdLine)
{
	if (*lpCmdLine == '\"')
		lpCmdLine++;

	return lpCmdLine;
}

static LPSTR GetNextArg(LPSTR lpCmdLine)
{
	int bInQuotes = 0;

	while (*lpCmdLine)
	{
		if (*lpCmdLine == '\"')
		{
			bInQuotes ^= 1;
			if(!bInQuotes)
			{
				*lpCmdLine++ = 0x00;	// Assume end-quote is end of this arg
				continue;
			}
		}

		if ((*lpCmdLine == ' ') && !bInQuotes)
		{
			*lpCmdLine++ = 0x00;

			while (*lpCmdLine == ' ')	// Skip multiple spaces between args
				lpCmdLine++;
			break;
		}

		lpCmdLine++;
	}

	return lpCmdLine;
}

//---------------------------------------------------------------------------

bool ProcessCmdLine(LPSTR lpCmdLine)
{
	const std::string strCmdLine(lpCmdLine);		// Keep a copy for log ouput
	std::string strUnsupported;

	// If 1st param looks like an abs pathname then assume that an associated filetype has been double-clicked
	// NB. Handled by WM_DDE_INITIATE & WM_DDE_EXECUTE msgs
	if ((lpCmdLine[0] >= '\"' && lpCmdLine[1] >= 'A' && lpCmdLine[1] <= 'Z' && lpCmdLine[2] == ':')	// always in quotes
		|| strncmp("\\\\?\\", lpCmdLine, 4) == 0)
		return true;

	while (*lpCmdLine)
	{
		LPSTR lpNextArg = GetNextArg(lpCmdLine);

		if (((strcmp(lpCmdLine, "-l") == 0) || (strcmp(lpCmdLine, "-log") == 0)) && (g_fh == NULL))
		{
			LogInit();
		}
		else if (strcmp(lpCmdLine, "-noreg") == 0)
		{
			g_bRegisterFileTypes = false;
		}
		else if (strcmp(lpCmdLine, "-conf") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			char buf[MAX_PATH];
			DWORD res = GetFullPathName(lpCmdLine, MAX_PATH, buf, NULL);
			if (res == 0)
				LogFileOutput("Failed to open configuration file: %s\n", lpCmdLine);
			else
				g_sConfigFile = buf;
		}
		else if (strcmp(lpCmdLine, "-d1") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szImageName_drive[SLOT6][DRIVE_1] = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-d2") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szImageName_drive[SLOT6][DRIVE_2] = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-d1-disconnected") == 0)
		{
			g_cmdLine.driveConnected[SLOT6][DRIVE_1] = false;
		}
		else if (strcmp(lpCmdLine, "-d2-disconnected") == 0)
		{
			g_cmdLine.driveConnected[SLOT6][DRIVE_2] = false;
		}
		else if (strcmp(lpCmdLine, "-h1") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szImageName_harddisk[SLOT7][HARDDISK_1] = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-h2") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szImageName_harddisk[SLOT7][HARDDISK_2] = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-s0") == 0)	// Language Card options for Apple II/II+
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (strcmp(lpCmdLine, "saturn") == 0 || strcmp(lpCmdLine, "saturn128") == 0)
				g_cmdLine.uSaturnBanks = Saturn128K::kMaxSaturnBanks;
			else if (strcmp(lpCmdLine, "saturn64") == 0)
				g_cmdLine.uSaturnBanks = Saturn128K::kMaxSaturnBanks / 2;
			else if (strcmp(lpCmdLine, "languagecard") == 0 || strcmp(lpCmdLine, "lc") == 0)
				g_cmdLine.bSlot0LanguageCard = true;
		}
		else if (lpCmdLine[0] == '-' && lpCmdLine[1] == 's' && lpCmdLine[2] >= '1' && lpCmdLine[2] <= '7')
		{
			const UINT slot = lpCmdLine[2] - '0';

			if (lpCmdLine[3] == 0)	// -s[1..7] <card>
			{
				lpCmdLine = GetCurrArg(lpNextArg);
				lpNextArg = GetNextArg(lpNextArg);
				if (strcmp(lpCmdLine, "empty") == 0)
				{
					g_cmdLine.bSlotEmpty[slot] = true;
				}
				else if (strcmp(lpCmdLine, "diskii") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_Disk2;
				}
				else if (strcmp(lpCmdLine, "diskii13") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_Disk2;
					g_cmdLine.slotInfo[slot].isDiskII13 = true;
				}
				else if (strcmp(lpCmdLine, "hdc") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_GenericHDD;
				}
				else if (strcmp(lpCmdLine, "hdc-sp") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_GenericHDD;
					g_cmdLine.slotInfo[slot].useHdcFirmwareMode = HdcSmartPort;
				}
				else if (strcmp(lpCmdLine, "hdc-bm2") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_GenericHDD;
					g_cmdLine.slotInfo[slot].useHdcFirmwareMode = HdcBlockMode2Devices;
				}
				else if (strcmp(lpCmdLine, "hdc-bm4") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_GenericHDD;
					g_cmdLine.slotInfo[slot].useHdcFirmwareMode = HdcBlockMode4Devices;
				}
				else if (strcmp(lpCmdLine, "saturn") == 0 || strcmp(lpCmdLine, "saturn128") == 0)	// Support Saturn128 card in slot 1-7 too (GH#1279)
				{
					g_cmdLine.slotInsert[slot] = CT_Saturn128K;
				}
				else if (strcmp(lpCmdLine, "megaaudio") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_MegaAudio;
					g_cmdLine.supportExtraMBCardTypes = true;
				}
				else if (strcmp(lpCmdLine, "sdmusic") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_SDMusic;
					g_cmdLine.supportExtraMBCardTypes = true;
				}
				else if (strcmp(lpCmdLine, "6522a-bad") == 0)
				{
					g_cmdLine.slotInfo[slot].useBad6522A = true;
				}
				else if (strcmp(lpCmdLine, "6522b-bad") == 0)
				{
					g_cmdLine.slotInfo[slot].useBad6522B = true;
				}
				else if (strcmp(lpCmdLine, "parallel") == 0)
				{
					if (slot == SLOT1)
						g_cmdLine.slotInsert[slot] = CT_GenericPrinter;
					else
						LogFileOutput("Parallel Printer card currently only supported in slot 1\n");
				}
				else if (strcmp(lpCmdLine, "ssc") == 0)
				{
					if (slot == SLOT2)
						g_cmdLine.slotInsert[slot] = CT_SSC;
					else
						LogFileOutput("SSC currently only supported in slot 2\n");
				}
				else if (strcmp(lpCmdLine, "vidhd") == 0)
				{
					if (slot == SLOT3)
						g_cmdLine.slotInsert[slot] = CT_VidHD;
					else
						LogFileOutput("VidHD currently only supported in slot 3\n");
				}
				else if (strncmp(lpCmdLine, "socket", 6) == 0 &&
					(lpCmdLine[6] == '0' || lpCmdLine[6] == '1') &&	// 0=$Cs20(top of card), 1=$Cs40(bottom of card)
					lpCmdLine[7] == '=')
				{
					BYTE socket = lpCmdLine[6] - '0';
					LPSTR socketType = &lpCmdLine[8];
					SSI263Type type = SSI263Unknown;
					if (strcmp(socketType, "empty") == 0)
						type = SSI263Empty;
					else if (strcmp(socketType, "ssi263p") == 0)
						type = SSI263P;
					else if (strcmp(socketType, "ssi263ap") == 0)
						type = SSI263AP;
					g_cmdLine.slotInfo[slot].socketSSI263[socket] = type;
					if (type == SSI263Unknown)
						LogFileOutput("Unsupported SSI263 type: %s\n", socketType);
				}
				else if (strcmp(lpCmdLine, "sc01") == 0)
				{
					g_cmdLine.slotInfo[slot].socketSC01 = SC01;
				}
				else if (strcmp(lpCmdLine, "no-sc01") == 0)
				{
					g_cmdLine.slotInfo[slot].socketSC01 = SSI263Empty;
				}
				else if (strcmp(lpCmdLine, "spoverslip") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_SmartPortOverSlip;
				}
				else if (strcmp(lpCmdLine, "dummysp") == 0)
				{
					g_cmdLine.slotInsert[slot] = CT_DummySmartport;
				}
				else
				{
					LogFileOutput("Unsupported slot-%d card: %s\n", slot, lpCmdLine);
				}
				
			}
			else if (lpCmdLine[3] == 'd' && (lpCmdLine[4] == '1' || lpCmdLine[4] == '2'))	// -s[1..7]d[1|2] <dsk-image>
			{
				const UINT drive = lpCmdLine[4] == '1' ? DRIVE_1 : DRIVE_2;

				if (slot != SLOT5 && slot != SLOT6)
				{
					LogFileOutput("Unsupported arg: %s\n", lpCmdLine);
				}
				else
				{
					lpCmdLine = GetCurrArg(lpNextArg);
					lpNextArg = GetNextArg(lpNextArg);
					g_cmdLine.szImageName_drive[slot][drive] = lpCmdLine;
				}
			}
			else if (lpCmdLine[3] == 'h' && (lpCmdLine[4] >= '1' || lpCmdLine[4] <= '8'))	// -s[1..7]h[1|2|...|8] <dsk-image>
			{
				const UINT drive = lpCmdLine[4] - '1';
				bool badDrive = drive >= NUM_HARDDISKS;

				if (badDrive || (slot != SLOT5 && slot != SLOT7))
				{
					LogFileOutput("Unsupported arg: %s\n", lpCmdLine);
				}
				else
				{
					lpCmdLine = GetCurrArg(lpNextArg);
					lpNextArg = GetNextArg(lpNextArg);
					g_cmdLine.szImageName_harddisk[slot][drive] = lpCmdLine;
				}
			}
			else if (strcmp(lpCmdLine, "-s7-empty-on-exit") == 0)
			{
				g_cmdLine.bSlot7EmptyOnExit = true;
			}
			else
			{
				LogFileOutput("Unsupported arg: %s\n", lpCmdLine);
			}
		}
		else if (strcmp(lpCmdLine, "-aux") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			if (strcmp(lpCmdLine, "empty") == 0)
			{
				g_cmdLine.auxSlotEmpty = true;
			}
			else if (strcmp(lpCmdLine, "std80") == 0)
			{
				g_cmdLine.auxSlotInsert = CT_80Col;
			}
			else if (strcmp(lpCmdLine, "ext80") == 0)
			{
				g_cmdLine.auxSlotInsert = CT_Extended80Col;
			}
			else if (strcmp(lpCmdLine, "rw3") == 0)
			{
				g_cmdLine.auxSlotInsert = CT_RamWorksIII;
				if (!g_cmdLine.uRamWorksExPages)
					g_cmdLine.uRamWorksExPages = kDefaultExMemoryBanksRealRW3;
			}
			else
			{
				LogFileOutput("Unsupported aux slot card: %s\n", lpCmdLine);
			}
		}
		else if (strcmp(lpCmdLine, "-harddisknumblocks") == 0)		// number of blocks to report for ProDOS
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.uHarddiskNumBlocks = atoi(lpCmdLine);
			if (g_cmdLine.uHarddiskNumBlocks > kHarddiskMaxNumBlocks)
			{
				g_cmdLine.uHarddiskNumBlocks = kHarddiskMaxNumBlocks;
			}
			else
			{
				if (g_cmdLine.uHarddiskNumBlocks < 0)
					g_cmdLine.uHarddiskNumBlocks = 0;
			}
		}
		else if (strcmp(lpCmdLine, "-load-state") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szSnapshotName = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-load-state-ignore-hdc-fw") == 0)	// For testing - Use in combination with -load-state
		{
			g_cmdLine.snapshotIgnoreHdcFirmware = true;
		}
		else if (strcmp(lpCmdLine, "-f") == 0 || strcmp(lpCmdLine, "-full-screen") == 0)
		{
			g_cmdLine.setFullScreen = 1;
		}
		else if (strcmp(lpCmdLine, "-no-full-screen") == 0)
		{
			g_cmdLine.setFullScreen = 0;
		}
#define CMD_FS_WIDTH "-fs-width="
		else if (strncmp(lpCmdLine, CMD_FS_WIDTH, sizeof(CMD_FS_WIDTH)-1) == 0)
		{
			if (g_cmdLine.setFullScreen < 0)	// Not yet been specified on cmd line?
				g_cmdLine.setFullScreen = 1;	// Implicity set full-screen. NB. Can be overridden by "-no-full-screen"

			LPSTR lpTmp = lpCmdLine + sizeof(CMD_FS_WIDTH)-1;
			{
				g_cmdLine.userSpecifiedWidth = atoi(lpTmp);
				if (!g_cmdLine.userSpecifiedWidth)
					LogFileOutput("Invalid cmd-line parameter for -fs-width=x switch\n");
			}
		}
#define CMD_FS_HEIGHT "-fs-height="
		else if (strncmp(lpCmdLine, CMD_FS_HEIGHT, sizeof(CMD_FS_HEIGHT)-1) == 0)
		{
			if (g_cmdLine.setFullScreen < 0)	// Not yet been specified on cmd line?
				g_cmdLine.setFullScreen = 1;	// Implicity set full-screen. NB. Can be overridden by "-no-full-screen"

			LPSTR lpTmp = lpCmdLine + sizeof(CMD_FS_HEIGHT)-1;
			if (strcmp(lpTmp, "best") == 0)
			{
				g_cmdLine.bestFullScreenResolution = true;
			}
			else
			{
				g_cmdLine.userSpecifiedHeight = atoi(lpTmp);
				if (!g_cmdLine.userSpecifiedHeight)
					LogFileOutput("Invalid cmd-line parameter for -fs-height=x switch\n");
			}
		}
		else if (strcmp(lpCmdLine, "-no-di") == 0)
		{
			g_bDisableDirectInput = true;
		}
		else if (strcmp(lpCmdLine, "-m") == 0)
		{
			g_bDisableDirectSound = true;
		}
		else if (strcmp(lpCmdLine, "-no-mb") == 0)
		{
			g_bDisableDirectSoundMockingboard = true;
		}
		else if (strcmp(lpCmdLine, "-memclear") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_nMemoryClearType = atoi(lpCmdLine);
			if (g_nMemoryClearType < 0)
				g_nMemoryClearType = 0;
			else
			if (g_nMemoryClearType >= NUM_MIP)
				g_nMemoryClearType = NUM_MIP - 1;
		}
#ifdef RAMWORKS
		else if (strcmp(lpCmdLine, "-r") == 0)		// RamWorks size [1..256]
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.uRamWorksExPages = atoi(lpCmdLine);
			if (g_cmdLine.uRamWorksExPages > kMaxExMemoryBanks)
				g_cmdLine.uRamWorksExPages = kMaxExMemoryBanks;
			else if (g_cmdLine.uRamWorksExPages < 1)
				g_cmdLine.uRamWorksExPages = 1;			// 1x64KB (aux mem)
			g_cmdLine.auxSlotInsert = CT_RamWorksIII;	// Insert RW3 into aux slot
		}
#endif
		else if (strcmp(lpCmdLine, "-f8rom") == 0)		// Use custom 2K ROM at [$F800..$FFFF]
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)	// Stop resource leak if -f8rom is specified twice!
				CloseHandle(g_hCustomRomF8);

			g_hCustomRomF8 = CreateFile(lpCmdLine, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
			if ((g_hCustomRomF8 == INVALID_HANDLE_VALUE) || (GetFileSize(g_hCustomRomF8, NULL) != 0x800))
				g_bCustomRomF8Failed = true;
		}
		else if (strcmp(lpCmdLine, "-rom") == 0)		// Use custom 16K at [$C000..$FFFF] or 12K ROM at [$D000..$FFFF]
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (g_hCustomRom != INVALID_HANDLE_VALUE)	// Stop resource leak if -rom is specified twice!
				CloseHandle(g_hCustomRom);

			g_hCustomRom = CreateFile(lpCmdLine, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
			if ((g_hCustomRom == INVALID_HANDLE_VALUE) || ((GetFileSize(g_hCustomRom, NULL) != 0x4000) && (GetFileSize(g_hCustomRom, NULL) != 0x3000)))
				g_bCustomRomFailed = true;
		}
		else if (strcmp(lpCmdLine, "-videorom") == 0)			// Use 2K (for II/II+). Use 4K,8K or 16K video ROM (for Enhanced //e)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (!GetVideo().ReadVideoRomFile(lpCmdLine))
			{
				std::string msg = "Failed to load video rom (not found or not exactly 2/4/8/16KiB)\n";
				LogFileOutput("%s", msg.c_str());
				GetFrame().FrameMessageBox(msg.c_str(), "AppleWin Error", MB_OK);
			}
			else
			{
				GetVideo().SetVideoRomRockerSwitch(true);	// Use PAL char set
			}
		}
		else if (strcmp(lpCmdLine, "-printscreen") == 0)		// Turn on display of the last filename print screen was saved to
		{
			GetFrame().SetDisplayPrintScreenFileName(true);
		}
		else if (strcmp(lpCmdLine, "-no-printscreen-key") == 0)		// Don't try to capture PrintScreen key GH#469
		{
			g_bCapturePrintScreenKey = false;
		}
		else if (strcmp(lpCmdLine, "-no-printscreen-dlg") == 0)		// Turn off the PrintScreen warning message dialog (if PrintScreen key can't be grabbed)
		{
			GetFrame().SetShowPrintScreenWarningDialog(false);
		}
		else if (strcmp(lpCmdLine, "-no-hook-system-key") == 0)		// Don't hook the System keys (eg. Left-ALT+ESC/SPACE/TAB) GH#556
		{
			g_bHookSystemKey = false;
		}
		else if (strcmp(lpCmdLine, "-hook-alt-tab") == 0)			// GH#556
		{
			g_bHookAltTab = true;
		}
		else if (strcmp(lpCmdLine, "-hook-altgr-control") == 0)		// GH#556
		{
			g_bHookAltGrControl = true;
		}
		else if (strcmp(lpCmdLine, "-altgr-sends-wmchar") == 0)		// GH#625
		{
			KeybSetAltGrSendsWM_CHAR(true);
		}
		else if (strcmp(lpCmdLine, "-capslock=off") == 0)			// GH#1187
		{
			KeybSetCapsLock(false);
		}
		else if (strcmp(lpCmdLine, "-no-hook-alt") == 0)			// GH#583
		{
			JoySetHookAltKeys(false);
		}
		else if (strcmp(lpCmdLine, "-left-alt-control-buttons") == 0)	// GH#743
		{
			JoySetButtonVirtualKey(0, VK_CONTROL);
			JoySetButtonVirtualKey(1, VK_MENU);
		}
		else if (strcmp(lpCmdLine, "-right-alt-control-buttons") == 0)	// GH#743
		{
			JoySetButtonVirtualKey(0, VK_MENU | KF_EXTENDED);
			JoySetButtonVirtualKey(1, VK_CONTROL | KF_EXTENDED);
		}
		else if (strcmp(lpCmdLine, "-swap-buttons") == 0)
		{
			g_cmdLine.bSwapButtons0and1 = true;
		}
		else if (strcmp(lpCmdLine, "-spkr-inc") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			const int nErrorInc = atoi(lpCmdLine);
			SoundCore_SetErrorInc( nErrorInc );
		}
		else if (strcmp(lpCmdLine, "-spkr-max") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			const int nErrorMax = atoi(lpCmdLine);
			SoundCore_SetErrorMax( nErrorMax );
		}
		else if (strcmp(lpCmdLine, "-use-real-printer") == 0)	// Enable control in Advanced config to allow dumping to a real printer
		{
			g_cmdLine.enableDumpToRealPrinter = true;
		}
		else if (strcmp(lpCmdLine, "-speech") == 0)
		{
			g_bEnableSpeech = true;
		}
		else if (strcmp(lpCmdLine, "-multimon") == 0)
		{
			GetFrame().g_bMultiMon = true;
		}
		else if ((strcmp(lpCmdLine, "-dcd") == 0) || (strcmp(lpCmdLine, "-modem") == 0))	// GH#386
		{
			g_cmdLine.supportDCD = true;
		}
		else if (strcmp(lpCmdLine, "-alt-enter=toggle-full-screen") == 0)	// GH#556
		{
			GetFrame().SetAltEnterToggleFullScreen(true);
		}
		else if (strcmp(lpCmdLine, "-alt-enter=open-apple-enter") == 0)		// GH#556
		{
			GetFrame().SetAltEnterToggleFullScreen(false);
		}
		else if (strcmp(lpCmdLine, "-video-mode=idealized") == 0)			// GH#616
		{
			g_cmdLine.newVideoType = VT_COLOR_IDEALIZED;
		}
		else if (strcmp(lpCmdLine, "-video-mode=rgb-videocard") == 0)
		{
			g_cmdLine.newVideoType = VT_COLOR_VIDEOCARD_RGB;
		}
		else if (strcmp(lpCmdLine, "-video-mode=composite-monitor") == 0)	// GH#763
		{
			g_cmdLine.newVideoType = VT_COLOR_MONITOR_NTSC;
		}
		else if (strcmp(lpCmdLine, "-video-style=vertical-blend") == 0)		// GH#616
		{
			g_cmdLine.newVideoStyleEnableMask = VS_COLOR_VERTICAL_BLEND;
		}
		else if (strcmp(lpCmdLine, "-video-style=no-vertical-blend") == 0)	// GH#616
		{
			g_cmdLine.newVideoStyleDisableMask = VS_COLOR_VERTICAL_BLEND;
		}
		else if (strcmp(lpCmdLine, "-rgb-card-invert-bit7") == 0)	// GH#633
		{
			RGB_SetInvertBit7(true);
		}
		else if (strcmp(lpCmdLine, "-mac-lc-card-dlgr") == 0)	// GH#1258
		{
			RGB_SetMacLCCardDLGR(true);
		}
		else if (strcmp(lpCmdLine, "-screenshot-and-exit") == 0)	// GH#616: For testing - Use in combination with -load-state
		{
			g_cmdLine.szScreenshotFilename = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
		}
		else if (strcmp(lpCmdLine, "-clock-multiplier") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.clockMultiplier = atof(lpCmdLine);
		}
		else if (strcmp(lpCmdLine, "-model") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (strcmp(lpCmdLine, "apple2") == 0)
				g_cmdLine.model = A2TYPE_APPLE2;
			else if (strcmp(lpCmdLine, "apple2p") == 0)
				g_cmdLine.model = A2TYPE_APPLE2PLUS;
			else if (strcmp(lpCmdLine, "apple2jp") == 0)
				g_cmdLine.model = A2TYPE_APPLE2JPLUS;
			else if (strcmp(lpCmdLine, "apple2e") == 0)
				g_cmdLine.model = A2TYPE_APPLE2E;
			else if (strcmp(lpCmdLine, "apple2ee") == 0)
				g_cmdLine.model = A2TYPE_APPLE2EENHANCED;
			else
				LogFileOutput("-model: unsupported type: %s\n", lpCmdLine);
		}
		else if (_stricmp(lpCmdLine, "-50hz") == 0)	// (case-insensitive)
		{
			g_cmdLine.newVideoRefreshRate = VR_50HZ;
		}
		else if (_stricmp(lpCmdLine, "-60hz") == 0)	// (case-insensitive)
		{
			g_cmdLine.newVideoRefreshRate = VR_60HZ;
		}
		else if (strcmp(lpCmdLine, "-rgb-card-type") == 0)
		{
			// RGB video card valid types are: "apple", "sl7", "eve", "feline"
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (strcmp(lpCmdLine, "apple") == 0)	// Just an alias for SL7
				g_cmdLine.rgbCard = RGB_Videocard_e::Apple;
			else if (strcmp(lpCmdLine, "sl7") == 0)
				g_cmdLine.rgbCard = RGB_Videocard_e::Video7_SL7;
			else if (strcmp(lpCmdLine, "eve") == 0)
				g_cmdLine.rgbCard = RGB_Videocard_e::LeChatMauve_EVE;
			else if (strcmp(lpCmdLine, "feline") == 0)
				g_cmdLine.rgbCard = RGB_Videocard_e::LeChatMauve_Feline;
			else
				LogFileOutput("-rgb-card-type: unsupported type: %s\n", lpCmdLine);
		}
		else if (strcmp(lpCmdLine, "-rgb-card-foreground") == 0)
		{
			// Default hardware-defined Text foreground color, for Video-7's RGB-SL7 card only
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.rgbCardForegroundColor = atoi(lpCmdLine);
		}
		else if (strcmp(lpCmdLine, "-rgb-card-background") == 0)
		{
			// Default hardware-defined Text background color, for Video-7's RGB-SL7 card only
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.rgbCardBackgroundColor = atoi(lpCmdLine);
		}
		else if (strcmp(lpCmdLine, "-power-on") == 0)
		{
			g_cmdLine.bBoot = true;
		}
		else if (strcmp(lpCmdLine, "-current-dir") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.strCurrentDir = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-no-nsc") == 0)
		{
			g_cmdLine.bRemoveNoSlotClock = true;
		}
		else if (strcmp(lpCmdLine, "-snes-max-alt-joy1") == 0)
		{
			g_cmdLine.snesMaxAltControllerType[0] = true;
		}
		else if (strcmp(lpCmdLine, "-snes-max-alt-joy2") == 0)
		{
			g_cmdLine.snesMaxAltControllerType[1] = true;
		}
		else if (strcmp(lpCmdLine, "-snes-max-user-joy1") == 0 || strcmp(lpCmdLine, "-snes-max-user-joy2") == 0)
		{
			const unsigned int joyNum = (strcmp(lpCmdLine, "-snes-max-user-joy1") == 0) ? 0 : 1;

			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			std::string errorMsg;
			if (!SNESMAXCard::ParseControllerMappingFile(joyNum, lpCmdLine, errorMsg))
			{
				LogFileOutput("%s", errorMsg.c_str());
				GetFrame().FrameMessageBox(errorMsg.c_str(), "AppleWin Error", MB_OK);
			}
		}
		else if (strcmp(lpCmdLine, "-wav-speaker") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.wavFileSpeaker = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-wav-mockingboard") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.wavFileMockingboard = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-mb-audit") == 0)	// enable selection of additional sound cards, eg. for mb-audit
		{
			g_cmdLine.supportExtraMBCardTypes = true;
		}
		else if (strcmp(lpCmdLine, "-no-disk2-stepper-defer") == 0)	// a debug switch added at 1.30.11 / GH#1110 (likely to be removed in a future version)
		{
			g_cmdLine.noDisk2StepperDefer = true;
		}
		else if (strcmp(lpCmdLine, "-hdc-firmware-v1") == 0)	// a debug switch added at 1.30.18 / GH#1277 (likely to be removed in a future version)
		{
			g_cmdLine.useHdcFirmwareV1 = true;
		}
		else if (strcmp(lpCmdLine, "-hdc-firmware-v2") == 0)
		{
			g_cmdLine.useHdcFirmwareV2 = true;
		}
		else if (strcmp(lpCmdLine, "-bootsector") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			char  sPath[ MAX_PATH];
			DWORD res = GetFullPathName(lpCmdLine, MAX_PATH, sPath, NULL);
			if (!res)
			{
				LogFileOutput( "ERROR: Couldn't get full path for custom boot sector file: %s\n", lpCmdLine );
			}

			FILE *pFile = fopen( sPath, "rb");
			if (pFile)
			{
				fseek( pFile, 0, SEEK_END );
				size_t nSize = ftell( pFile );
				fseek( pFile, 0, SEEK_SET );
				fclose( pFile );

				if (nSize > 0)
				{
					g_cmdLine.sBootSectorFileName = sPath;
					g_cmdLine.nBootSectorFileSize = nSize;
				}
				else
				{
					g_cmdLine.nBootSectorFileSize = 0;
					LogFileOutput( "INFO: Custom Boot Sector size = %zu\n", nSize );
				}
			}
			else
			{
				LogFileOutput( "ERROR: Couldn't open custom boot sector file: %s\n", lpCmdLine );
			}
		}
		else if (strcmp(lpCmdLine, "-alt-cpu-emu") == 0)	// debug
		{
			g_cmdLine.useAltCpuEmulation = true;
		}
		else	// unsupported
		{
			LogFileOutput("Unsupported arg: %s\n", lpCmdLine);
			strUnsupported += lpCmdLine;
			strUnsupported += "\n";
		}

		lpCmdLine = lpNextArg;
	}

	LogFileOutput("CmdLine: %s\n",  strCmdLine.c_str());

	bool ok = true;

	if (!strUnsupported.empty())
	{
		std::string msg("Unsupported commands:\n\n");
		msg += strUnsupported;
		msg += "\n";
		msg += "Continue running AppleWin?";
		int res = GetFrame().FrameMessageBox(
				msg.c_str(),
				"AppleWin Command Line",
				MB_ICONSTOP | MB_SETFOREGROUND | MB_YESNO);
		ok = (res != IDNO);
	}

	return ok;
}
