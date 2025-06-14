/*AppleWin : An Apple //e emulator for Windows

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

#include <climits>
#include <regex>
#include <stdexcept>
#include <string>

#include "Utilities.h"
#include "Core.h"
#include "CardManager.h"
#include "CPU.h"
#include "Joystick.h"
#include "Log.h"
#include "ParallelPrinter.h"
#include "Registry.h"
#include "Riff.h"
#include "SaveState.h"
#include "Speaker.h"
#include "Memory.h"
#include "Pravets.h"
#include "Keyboard.h"
#include "Interface.h"
#include "SoundCore.h"
#include "CopyProtectionDongles.h"

#include "Configuration/IPropertySheet.h"
#include "Configuration/PropertySheetHelper.h"
#include "Tfe/PCapBackend.h"

#ifdef USE_SPEECH_API
#include "Speech.h"
#endif

// Backwards compatibility with AppleWin <1.24.0
static void LoadConfigOldJoystick_v1(const UINT uJoyNum)
{
	uint32_t dwOldJoyType;
	if (!REGLOAD(uJoyNum==0 ? REGVALUE_OLD_JOYSTICK0_EMU_TYPE1 : REGVALUE_OLD_JOYSTICK1_EMU_TYPE1, &dwOldJoyType))
		return;	// EG. Old AppleWin never installed

	UINT uNewJoyType;
	switch (dwOldJoyType)
	{
	case 0:		// Disabled
	default:
		uNewJoyType = J0C_DISABLED;
		break;
	case 1:		// PC Joystick
		uNewJoyType = J0C_JOYSTICK1;
		break;
	case 2:		// Keyboard (standard)
		uNewJoyType = J0C_KEYBD_NUMPAD;
		GetPropertySheet().SetJoystickCenteringControl(JOYSTICK_MODE_FLOATING);
		break;
	case 3:		// Keyboard (centering)
		uNewJoyType = J0C_KEYBD_NUMPAD;
		GetPropertySheet().SetJoystickCenteringControl(JOYSTICK_MODE_CENTERING);
		break;
	case 4:		// Mouse
		uNewJoyType = J0C_MOUSE;
		break;
	}

	JoySetJoyType(uJoyNum, uNewJoyType);
}

// Reads configuration from the registry entries
//
// NB. loadImages=false if loading a save-state from cmd-line afterwards
// - Registry images may have been deleted from disk, so avoid the MessageBox
void LoadConfiguration(bool loadImages)
{
	uint32_t dwComputerType = 0;
	eApple2Type apple2Type = A2TYPE_APPLE2EENHANCED;

	///////////////////////////////////////////////////////////////
	// SmartPort over SLIP
	auto& listener = GetCommandListener();
	uint32_t dwRegStartListener = 0;
	bool bStartListener = listener.default_start_listener;

	char tcAddress[16];
	strncpy(tcAddress, listener.default_listener_address.data(), 15);
	tcAddress[15] = '\0'; // ensure it's null terminated in worst case 111.111.111.111

	uint32_t dwPort = static_cast<uint32_t>(listener.default_port);
	uint32_t dwResponseTimeout = static_cast<uint32_t>(listener.default_response_timeout);

	if (REGLOAD(REGVALUE_START_SP_SLIP_LISTENER, &dwRegStartListener))
	{
		bStartListener = dwRegStartListener ? true : false;
		RegLoadString(REG_CONFIG, REGVALUE_SP_LISTENER_ADDRESS, 1, tcAddress, 16, listener.default_listener_address.c_str());
		REGLOAD_DEFAULT(REGVALUE_SP_LISTENER_PORT, &dwPort, listener.default_port);
		REGLOAD_DEFAULT(REGVALUE_SP_RESPONSE_TIMEOUT, &dwResponseTimeout, listener.default_response_timeout);
	}
	else
	{
		// Save defaults to registry if non exist, this cleans up some editing issues if there's no values already saved
		REGSAVE(REGVALUE_START_SP_SLIP_LISTENER, 1);
		RegSaveString(REG_CONFIG, REGVALUE_SP_LISTENER_ADDRESS, 1, listener.default_listener_address);
		REGSAVE(REGVALUE_SP_LISTENER_PORT, listener.default_port);
		REGSAVE(REGVALUE_SP_RESPONSE_TIMEOUT, listener.default_response_timeout);
	}

	std::string listener_address(tcAddress);
	listener_address = listener.check_and_set_ip_address(listener_address);

	// check the input number wasn't too small or too large as uint32_t is 4 bytes, port is only 2.
	// Could be running as root (the horror) and get a port <1024
	if (dwPort > 65535 || dwPort == 0) {
		dwPort = listener.default_port;
	}
	uint16_t port = static_cast<uint16_t>(dwPort);

	// check the response timeout is sane, should be 1 or greater.
	if (dwResponseTimeout > 65535 || dwResponseTimeout == 0) {
		dwResponseTimeout = listener.default_response_timeout;
	}
	uint16_t response_timeout = static_cast<uint16_t>(dwResponseTimeout);

	if (bStartListener)
	{
		listener.Initialize(listener_address, port, response_timeout);
		listener.start();
	}

	// Store the values so that if we open the preferences, we can fetch it to set the current state of the checkbox
	listener.set_start_on_init(bStartListener);
	listener.set_port(port);
	listener.set_response_timeout(response_timeout);
	///////////////////////////////////////////////////////////////


	if (REGLOAD(REGVALUE_APPLE2_TYPE, &dwComputerType))
	{
		const uint32_t dwLoadedComputerType = dwComputerType;

		if ( (dwComputerType >= A2TYPE_MAX) ||
			 (dwComputerType >= A2TYPE_UNDEFINED && dwComputerType < A2TYPE_CLONE) ||
			 (dwComputerType >= A2TYPE_CLONE_A2_MAX && dwComputerType < A2TYPE_CLONE_A2E) )
			dwComputerType = A2TYPE_APPLE2EENHANCED;

		// Remap the bad Pravets models (before AppleWin v1.26)
		if (dwComputerType == A2TYPE_BAD_PRAVETS82) dwComputerType = A2TYPE_PRAVETS82;
		if (dwComputerType == A2TYPE_BAD_PRAVETS8M) dwComputerType = A2TYPE_PRAVETS8M;

		// Remap the bad Pravets models (at AppleWin v1.26) - GH#415
		if (dwComputerType == A2TYPE_CLONE) dwComputerType = A2TYPE_PRAVETS82;

		if (dwLoadedComputerType != dwComputerType)
		{
			std::string strText = StrFormat("Unsupported Apple2Type(%d). Changing to %d", dwLoadedComputerType, dwComputerType);

			LogFileOutput("%s\n", strText.c_str());

			GetFrame().FrameMessageBox(strText.c_str(), "Load Configuration", MB_ICONSTOP | MB_SETFOREGROUND);

			GetPropertySheet().ConfigSaveApple2Type((eApple2Type)dwComputerType);
		}

		apple2Type = (eApple2Type) dwComputerType;
	}
	else if (REGLOAD(REGVALUE_OLD_APPLE2_TYPE, &dwComputerType))	// Support older AppleWin registry entries
	{
		switch (dwComputerType)
		{
			// NB. No A2TYPE_APPLE2E (this is correct)
		case 0:		apple2Type = A2TYPE_APPLE2; break;
		case 1:		apple2Type = A2TYPE_APPLE2PLUS; break;
		case 2:		apple2Type = A2TYPE_APPLE2EENHANCED; break;
		default:	apple2Type = A2TYPE_APPLE2EENHANCED; break;
		}
	}

	SetApple2Type(apple2Type);

	//

	uint32_t dwMainCpuType;
	REGLOAD_DEFAULT(REGVALUE_CPU_TYPE, &dwMainCpuType, CPU_65C02);
	if (dwMainCpuType != CPU_6502 && dwMainCpuType != CPU_65C02)
		dwMainCpuType = CPU_65C02;
	SetMainCpu((eCpuType)dwMainCpuType);

	//

	uint32_t dwJoyType;
	if (REGLOAD(REGVALUE_JOYSTICK0_EMU_TYPE, &dwJoyType))
		JoySetJoyType(JN_JOYSTICK0, dwJoyType);
	else if (REGLOAD(REGVALUE_OLD_JOYSTICK0_EMU_TYPE2, &dwJoyType))	// GH#434
		JoySetJoyType(JN_JOYSTICK0, dwJoyType);
	else
		LoadConfigOldJoystick_v1(JN_JOYSTICK0);

	if (REGLOAD(REGVALUE_JOYSTICK1_EMU_TYPE, &dwJoyType))
		JoySetJoyType(JN_JOYSTICK1, dwJoyType);
	else if (REGLOAD(REGVALUE_OLD_JOYSTICK1_EMU_TYPE2, &dwJoyType))	// GH#434
		JoySetJoyType(JN_JOYSTICK1, dwJoyType);
	else
		LoadConfigOldJoystick_v1(JN_JOYSTICK1);

	uint32_t copyProtectionDongleType;
	std::string regSection = RegGetConfigSlotSection(GAME_IO_CONNECTOR);
	if (RegLoadValue(regSection.c_str(), REGVALUE_GAME_IO_TYPE, TRUE, &copyProtectionDongleType))
		SetCopyProtectionDongleType((DONGLETYPE)copyProtectionDongleType);
	else
		SetCopyProtectionDongleType(DT_EMPTY);

	uint32_t dwSoundType;
	REGLOAD_DEFAULT(REGVALUE_SOUND_EMULATION, &dwSoundType, REG_SOUNDTYPE_WAVE);
	switch (dwSoundType)
	{
	case REG_SOUNDTYPE_NONE:
	case REG_SOUNDTYPE_DIRECT:	// Not supported from 1.26
	case REG_SOUNDTYPE_SMART:	// Not supported from 1.26
	default:
		soundtype = SOUND_NONE;
		break;
	case REG_SOUNDTYPE_WAVE:
		soundtype = SOUND_WAVE;
		break;
	}

	REGLOAD_DEFAULT(REGVALUE_EMULATION_SPEED, &g_dwSpeed, SPEED_NORMAL);
	GetVideo().Config_Load_Video();
	SetCurrentCLK6502();	// Pre: g_dwSpeed && Config_Load_Video()->SetVideoRefreshRate()

	//

	uint32_t dwTmp = 0;

	if(REGLOAD(REGVALUE_FS_SHOW_SUBUNIT_STATUS, &dwTmp))
		GetFrame().SetFullScreenShowSubunitStatus(dwTmp ? true : false);

	if (REGLOAD(REGVALUE_SHOW_DISKII_STATUS, &dwTmp))
		GetFrame().SetWindowedModeShowDiskiiStatus(dwTmp ? true : false);

	if(REGLOAD(REGVALUE_THE_FREEZES_F8_ROM, &dwTmp))
		GetPropertySheet().SetTheFreezesF8Rom(dwTmp);

	if(REGLOAD(REGVALUE_SAVE_STATE_ON_EXIT, &dwTmp))
		g_bSaveStateOnExit = dwTmp ? true : false;

	if(REGLOAD(REGVALUE_PDL_XTRIM, &dwTmp))
		JoySetTrim((short)dwTmp, true);
	if(REGLOAD(REGVALUE_PDL_YTRIM, &dwTmp))
		JoySetTrim((short)dwTmp, false);

	if(REGLOAD(REGVALUE_SCROLLLOCK_TOGGLE, &dwTmp))
		GetPropertySheet().SetScrollLockToggle(dwTmp);

	if(REGLOAD(REGVALUE_CURSOR_CONTROL, &dwTmp))
		GetPropertySheet().SetJoystickCursorControl(dwTmp);
	if(REGLOAD(REGVALUE_AUTOFIRE, &dwTmp))
		GetPropertySheet().SetAutofire(dwTmp);
	if(REGLOAD(REGVALUE_SWAP_BUTTONS_0_AND_1, &dwTmp))
		GetPropertySheet().SetButtonsSwapState(dwTmp ? true : false);
	if(REGLOAD(REGVALUE_CENTERING_CONTROL, &dwTmp))
		GetPropertySheet().SetJoystickCenteringControl(dwTmp);

	if(REGLOAD(REGVALUE_MOUSE_CROSSHAIR, &dwTmp))
		GetPropertySheet().SetMouseShowCrosshair(dwTmp);
	if(REGLOAD(REGVALUE_MOUSE_RESTRICT_TO_WINDOW, &dwTmp))
		GetPropertySheet().SetMouseRestrictToWindow(dwTmp);

	//

	char szFilename[MAX_PATH];

	//

	for (UINT slot = SLOT0; slot <= SLOT7; slot++)
	{
		std::string regSection = RegGetConfigSlotSection(slot);

		if (RegLoadValue(regSection.c_str(), REGVALUE_CARD_TYPE, TRUE, &dwTmp))
		{
			GetCardMgr().Insert(slot, (SS_CARDTYPE)dwTmp, false);
		}
		else	// legacy (AppleWin 1.30.3 or earlier)
		{
			if (slot == SLOT3)
			{
				RegLoadString(REG_CONFIG, REGVALUE_UTHERNET_INTERFACE, 1, szFilename, MAX_PATH, "");
				// copy it to the new location
				PCapBackend::SetRegistryInterface(slot, szFilename);

				uint32_t tfeEnabled;
				REGLOAD_DEFAULT(REGVALUE_UTHERNET_ACTIVE, &tfeEnabled, 0);
				GetCardMgr().Insert(SLOT3, tfeEnabled ? CT_Uthernet : CT_Empty);
			}
			else if (slot == SLOT4 && REGLOAD(REGVALUE_SLOT4, &dwTmp))
				GetCardMgr().Insert(SLOT4, (SS_CARDTYPE)dwTmp);
			else if (slot == SLOT5 && REGLOAD(REGVALUE_SLOT5, &dwTmp))
				GetCardMgr().Insert(SLOT5, (SS_CARDTYPE)dwTmp);
			else if (slot == SLOT7 && REGLOAD(REGVALUE_HDD_ENABLED, &dwTmp) && dwTmp == 1)	// GH#1015
				GetCardMgr().Insert(SLOT7, CT_GenericHDD);
		}
	}

	// Aux slot

	{
		std::string regSection = RegGetConfigSlotSection(SLOT_AUX);

		if (RegLoadValue(regSection.c_str(), REGVALUE_CARD_TYPE, TRUE, &dwTmp))
		{
			SS_CARDTYPE type = (SS_CARDTYPE)dwTmp;
			const bool noUpdateRegistry = false;
			GetCardMgr().InsertAux(type, noUpdateRegistry);
			SetExpansionMemType(type, noUpdateRegistry);

			RegLoadValue(regSection.c_str(), REGVALUE_AUX_NUM_BANKS, TRUE, &dwTmp, kDefaultExMemoryBanksRealRW3);
			SetRamWorksMemorySize(dwTmp, noUpdateRegistry);
		}
	}

	if(REGLOAD(REGVALUE_SPKR_VOLUME, &dwTmp))
		SpkrSetVolume(dwTmp, GetPropertySheet().GetVolumeMax());

	if(REGLOAD(REGVALUE_MB_VOLUME, &dwTmp))
		GetCardMgr().GetMockingboardCardMgr().SetVolume(dwTmp, GetPropertySheet().GetVolumeMax());

	// Load save-state pathname *before* inserting any harddisk/disk images (for both init & reinit cases)
	// NB. inserting harddisk/disk can change snapshot pathname
	RegLoadString(REG_CONFIG, REGVALUE_SAVESTATE_FILENAME, 1, szFilename, MAX_PATH, "");	// Can be pathname or just filename
	Snapshot_SetFilename(szFilename);	// If not in Registry than default will be used (ie. g_sCurrentDir + default filename)

	//

	RegLoadString(REG_PREFS, REGVALUE_PREF_HDV_START_DIR, 1, szFilename, MAX_PATH, "");
	if (szFilename[0] == '\0')
		GetCurrentDirectory(sizeof(szFilename), szFilename);
	SetCurrentImageDir(szFilename);

	for (UINT slot = SLOT1; slot <= SLOT7; slot++)
	{
		if (loadImages && GetCardMgr().QuerySlot(slot) == CT_GenericHDD)
		{
			for (UINT i = 0; i < NUM_HARDDISKS; i++)
				dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot)).LoadLastDiskImage(i);
		}
	}

	//

	// Current/Starting Dir is the "root" of where the user keeps their disk images
	RegLoadString(REG_PREFS, REGVALUE_PREF_START_DIR, 1, szFilename, MAX_PATH, "");
	if (szFilename[0] == '\0')
		GetCurrentDirectory(sizeof(szFilename), szFilename);
	SetCurrentImageDir(szFilename);

	if (loadImages)
		GetCardMgr().GetDisk2CardMgr().LoadLastDiskImage();

	// Do this after populating the slots with Disk II controller(s)
	uint32_t dwEnhanceDisk;
	REGLOAD_DEFAULT(REGVALUE_ENHANCE_DISK_SPEED, &dwEnhanceDisk, 1);
	GetCardMgr().GetDisk2CardMgr().SetEnhanceDisk(dwEnhanceDisk ? true : false);

	//

	if (GetCardMgr().IsParallelPrinterCardInstalled())
		GetCardMgr().GetParallelPrinterCard()->GetRegistryConfig();

	//

	if (REGLOAD(REGVALUE_WINDOW_SCALE, &dwTmp))
		GetFrame().SetViewportScale(dwTmp);

	if (REGLOAD(REGVALUE_CONFIRM_REBOOT, &dwTmp))
		GetFrame().g_bConfirmReboot = dwTmp;
}

static std::string GetFullPath(LPCSTR szFileName)
{
	std::string strPathName;

	if (szFileName[0] == PATH_SEPARATOR || szFileName[1] == ':')
	{
		// Abs pathname
		strPathName = szFileName;
	}
	else
	{
		// Rel pathname (GH#663)
		strPathName = g_sStartDir;
		strPathName.append(szFileName);
	}

	return strPathName;
}

static void SetCurrentDir(const std::string & pathname)
{
	// Due to the order HDDs/disks are inserted, then s7 insertions take priority over s6 & s5; and d2 takes priority over d1:
	// . if -s6[dN] and -hN are specified, then g_sCurrentDir will be set to the HDD image's path
	// . if -s5[dN] and -s6[dN] are specified, then g_sCurrentDir will be set to the s6 image's path
	// . if -[sN]d1 and -[sN]d2 are specified, then g_sCurrentDir will be set to the d2 image's path
	// This is purely dependent on the current order of InsertFloppyDisks() & InsertHardDisks() - ie. very brittle!
	// . better to use -current-dir to be explicit
	std::size_t found = pathname.find_last_of(PATH_SEPARATOR);
	std::string path = pathname.substr(0, found);
	SetCurrentImageDir(path);
}

static bool DoDiskInsert(const UINT slot, const int nDrive, LPCSTR szFileName)
{
	Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot));

	if (szFileName[0] == '\0')
	{
		disk2Card.EjectDisk(nDrive);
		return true;
	}

	std::string strPathName = GetFullPath(szFileName);
	if (strPathName.empty()) return false;

	ImageError_e Error = disk2Card.InsertDisk(nDrive, strPathName, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
	bool res = (Error == eIMAGE_ERROR_NONE);
	if (res)
		SetCurrentDir(strPathName);
	return res;
}

static bool DoHardDiskInsert(const UINT slot, const int nDrive, LPCSTR szFileName)
{
	_ASSERT(GetCardMgr().QuerySlot(slot) == CT_GenericHDD);
	if (GetCardMgr().QuerySlot(slot) != CT_GenericHDD)
		return false;

	if (szFileName[0] == '\0')
	{
		dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot)).Unplug(nDrive);
		return true;
	}

	std::string strPathName = GetFullPath(szFileName);
	if (strPathName.empty()) return false;

	BOOL bRes = dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot)).Insert(nDrive, strPathName);
	bool res = (bRes == TRUE);
	if (res)
		SetCurrentDir(strPathName);
	return res;
}

void InsertFloppyDisks(const UINT slot, LPCSTR szImageName_drive[NUM_DRIVES], bool driveConnected[NUM_DRIVES], bool& bBoot)
{
	_ASSERT(slot == 5 || slot == 6);

	bool bRes = true;

	if (!driveConnected[DRIVE_1])
	{
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot)).UnplugDrive(DRIVE_1);
	}
	else if (szImageName_drive[DRIVE_1])
	{
		bRes = DoDiskInsert(slot, DRIVE_1, szImageName_drive[DRIVE_1]);
		LogFileOutput("Init: S%d, DoDiskInsert(D1), res=%d\n", slot, bRes);
		GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);	// floppy activity LEDs and floppy buttons
		bBoot = true;
	}

	if (!driveConnected[DRIVE_2])
	{
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot)).UnplugDrive(DRIVE_2);
	}
	else if (szImageName_drive[DRIVE_2])
	{
		bRes &= DoDiskInsert(slot, DRIVE_2, szImageName_drive[DRIVE_2]);
		LogFileOutput("Init: S%d, DoDiskInsert(D2), res=%d\n", slot, bRes);
	}

	if (!bRes)
		GetFrame().FrameMessageBox("Failed to insert floppy disk(s) - see log file", "Warning", MB_ICONASTERISK | MB_OK);
}

void InsertHardDisks(const UINT slot, LPCSTR szImageName_harddisk[NUM_HARDDISKS], bool& bBoot)
{
	_ASSERT(slot == 5 || slot == 7);

	// If no HDDs then just return (and don't insert an HDC into this slot)
	bool res = true;
	for (UINT i = 0; i < NUM_HARDDISKS; i++)
		res &= szImageName_harddisk[i] == NULL;
	if (res)
		return;

	if (GetCardMgr().QuerySlot(slot) != CT_GenericHDD)
		GetCardMgr().Insert(slot, CT_GenericHDD);	// Enable the Harddisk controller card

	res = true;
	for (UINT i = 0; i < NUM_HARDDISKS; i++)
	{
		if (szImageName_harddisk[i])
		{
			res &= DoHardDiskInsert(slot, i, szImageName_harddisk[i]);
			LogFileOutput("Init: DoHardDiskInsert(HDD-%d), res=%d\n", i, res);

			if (i == HARDDISK_1)
			{
				GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_DISK_STATUS);	// harddisk activity LED
				bBoot = true;
			}
		}
	}

	if (!res)
		GetFrame().FrameMessageBox("Failed to insert harddisk(s) - see log file", "Warning", MB_ICONASTERISK | MB_OK);
}

void GetAppleWindowTitle()
{
	switch (g_Apple2Type)
	{
	default:
	case A2TYPE_APPLE2:			 g_pAppTitle = TITLE_APPLE_2; break;
	case A2TYPE_APPLE2PLUS:		 g_pAppTitle = TITLE_APPLE_2_PLUS; break;
	case A2TYPE_APPLE2JPLUS:	 g_pAppTitle = TITLE_APPLE_2_JPLUS; break;
	case A2TYPE_APPLE2E:		 g_pAppTitle = TITLE_APPLE_2E; break;
	case A2TYPE_APPLE2EENHANCED: g_pAppTitle = TITLE_APPLE_2E_ENHANCED; break;
	case A2TYPE_PRAVETS82:		 g_pAppTitle = TITLE_PRAVETS_82; break;
	case A2TYPE_PRAVETS8M:		 g_pAppTitle = TITLE_PRAVETS_8M; break;
	case A2TYPE_PRAVETS8A:		 g_pAppTitle = TITLE_PRAVETS_8A; break;
	case A2TYPE_TK30002E:		 g_pAppTitle = TITLE_TK3000_2E; break;
	case A2TYPE_BASE64A:		 g_pAppTitle = TITLE_BASE64A; break;
	}

#if _DEBUG
	g_pAppTitle += " *DEBUG* ";
#endif

	if (g_nAppMode == MODE_LOGO)
		return;

	g_pAppTitle += " - ";

	if (GetVideo().IsVideoStyle(VS_HALF_SCANLINES))
		g_pAppTitle += " 50% ";

	g_pAppTitle += GetVideo().VideoGetAppWindowTitle();

	if (GetCardMgr().GetDisk2CardMgr().IsAnyFirmware13Sector())
		g_pAppTitle += " (S6-13) ";

	if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
		g_pAppTitle += " (custom rom)";
	else if (GetPropertySheet().GetTheFreezesF8Rom() && IsApple2PlusOrClone(GetApple2Type()))
		g_pAppTitle += " (The Freeze's non-autostart F8 rom)";

	switch (g_nAppMode)
	{
	case MODE_PAUSED: g_pAppTitle += std::string(" [") + TITLE_PAUSED + "]"; break;
	case MODE_STEPPING: g_pAppTitle += std::string(" [") + TITLE_STEPPING + "]"; break;
	}
}

//===========================================================================

// CtrlReset() vs ResetMachineState():
// . CPU:
//		Ctrl+Reset : 6502.sp=-3    / CpuReset()
//		Power cycle: 6502.sp=0x1ff / CpuInitialize()
// . Disk][:
//		Ctrl+Reset : if motor-on, then motor-off but continue to spin for 1s
//		Power cycle: motor-off & immediately stop spinning

// todo: consolidate CtrlReset() and ResetMachineState()
void ResetMachineState()
{
	LogFileOutput("Apple II power-cycle\n");

	GetCardMgr().Reset(true);
	g_bFullSpeed = 0;	// Might've hit reset in middle of InternalCpuExecute() - so beep may get (partially) muted

	MemReset();	// calls CpuInitialize(), CNoSlotClock.Reset()
	GetPravets().Reset();
	if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).Boot();
	GetVideo().VideoResetState();
	KeybReset();
	JoyReset();
	SpkrReset();
	SetActiveCpu(GetMainCpu());
#ifdef USE_SPEECH_API
	g_Speech.Reset();
#endif

	SoundCore_SetFade(FADE_NONE);
	LogFileTimeUntilFirstKeyReadReset();
	// GetSPoverSLIPListener().stop(); // We may not need to stop the listener. The card is reset, so that will send device resets, but SP over SLIP layer can stay alive
}


//===========================================================================

/*
 * In comments, UTAII is an abbreviation for a reference to "Understanding the Apple II" by James Sather
 */

 // todo: consolidate CtrlReset() and ResetMachineState()
void CtrlReset()
{
	if (IsAppleIIeOrAbove(GetApple2Type()))
	{
		// NB. RamWorks III manual (v1.41, pg 45):
		// "The bank select register is initialized to zero on a power-up, but not after a reset."

		// For A][ & A][+, reset doesn't reset the LC switches (UTAII:5-29)
		// TODO: What about Saturn cards? Presumably the same as the A][ & A][+ slot0 LC?
		MemResetPaging();

		// For A][ & A][+, reset doesn't reset the video mode (UTAII:4-4)
		GetVideo().VideoResetState();	// Switch Alternate char set off
	}

	if (IsAppleIIeOrAbove(GetApple2Type()) || IsCopamBase64A(GetApple2Type()))
	{
		// For A][ & A][+, reset doesn't reset the annunciators (UTAIIe:I-5)
		// Base 64A: on RESET does reset to ROM page 0 (GH#807)
		MemAnnunciatorReset();
	}

	GetPravets().Reset();
	GetCardMgr().Reset(false);
	KeybReset();
#ifdef USE_SPEECH_API
	g_Speech.Reset();
#endif

	CpuReset();
	GetFrame().g_bFreshReset = true;
}

#ifdef WIN32
// This isn't required and will not compile in Linux port
std::string GetDialogText(HWND hWnd, int control_id, size_t max_length) {
	if (max_length > USHRT_MAX) {
        max_length = USHRT_MAX;
    }

	std::vector<char> buffer(max_length, 0);
    *(USHORT*)buffer.data() = static_cast<USHORT>(max_length);

    UINT nLineLength = SendDlgItemMessage(hWnd, control_id, EM_LINELENGTH, 0, 0);

    SendDlgItemMessage(hWnd, control_id, EM_GETLINE, 0, (LPARAM)buffer.data());

    nLineLength = nLineLength > max_length - 1 ? max_length - 1 : nLineLength;
    buffer[nLineLength] = 0x00;

    return std::string(buffer.data());
}

int GetDialogNumber(HWND hWnd, int control_id, size_t max_length) {
    std::string str = GetDialogText(hWnd, control_id, max_length);

    try {
        return std::stoi(str);
    } catch (const std::invalid_argument&) {
        return 0;
    }
}
#endif
