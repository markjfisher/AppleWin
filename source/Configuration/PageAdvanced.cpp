/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

#include "PageAdvanced.h"
#include "PropertySheet.h"

#include "../Common.h"
#include "../ParallelPrinter.h"
#include "../Registry.h"
#include "../SaveState.h"
#include "../CardManager.h"
#include "../CopyProtectionDongles.h"
#include "../resource/resource.h"
#include <Utilities.h>

CPageAdvanced* CPageAdvanced::ms_this = 0;	// reinit'd in ctor

enum CLONECHOICE {MENUITEM_CLONEMIN, MENUITEM_PRAVETS82=MENUITEM_CLONEMIN, MENUITEM_PRAVETS8M, MENUITEM_PRAVETS8A, MENUITEM_TK30002E, MENUITEM_BASE64A, MENUITEM_CLONEMAX};
const char CPageAdvanced::m_CloneChoices[] =
				"Pravets 82\0"	// Bulgarian
				"Pravets 8M\0"	// Bulgarian
				"Pravets 8A\0"	// Bulgarian
				"TK3000 //e\0"	// Brazilian
				"Base 64A\0"; 	// Taiwanese

const char CPageAdvanced::m_gameIOConnectorChoices[] =
				"Empty\0"
				"SDS DataKey - SpeedStar\0"		/* Protection dongle for Southwestern Data Systems "SpeedStar" Applesoft Compiler */
				"Cortechs Corp - CodeWriter\0"	/* Protection key for Dynatech Microsoftware / Cortechs Corp "CodeWriter" */
				"Robocom Ltd - Robo 500\0"		/* Interface Module for Robocom Ltd's Robo 500 */
				"Robocom Ltd - Robo 1000\0"		/* Interface Module for Robocom Ltd's Robo 1000 */
				"Robocom Ltd - Robo 1500, CAD-2P\0"	/* Interface Module for Robocom Ltd's Robo 1500, Robo Systems CAD-2P */
				"Hayden - Applesoft Compiler\0";	/* Protection key for Hayden Book Company, Inc's Applesoft Compiler (1981) */


INT_PTR CALLBACK CPageAdvanced::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageAdvanced::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageAdvanced::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_NOTIFY:
		{
			// Property Sheet notifications

			switch (((LPPSHNOTIFY)lparam)->hdr.code)
			{
			case PSN_SETACTIVE:
				// About to become the active page
				m_PropertySheetHelper.SetLastPage(m_Page);
				InitOptions(hWnd);
				break;
			case PSN_KILLACTIVE:
				SetWindowLongPtr(hWnd, DWLP_MSGRESULT, FALSE);			// Changes are valid
				break;
			case PSN_APPLY:
				DlgOK(hWnd);
				SetWindowLongPtr(hWnd, DWLP_MSGRESULT, PSNRET_NOERROR);	// Changes are valid
				break;
			case PSN_QUERYCANCEL:
				// Can use this to ask user to confirm cancel
				break;
			case PSN_RESET:
				DlgCANCEL(hWnd);
				break;
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_SAVESTATE_FILENAME:
			break;
		case IDC_SAVESTATE_BROWSE:
			if(m_PropertySheetHelper.SaveStateSelectImage(hWnd, "Select Save State file", true))
				SendDlgItemMessage(hWnd, IDC_SAVESTATE_FILENAME, WM_SETTEXT, 0, (LPARAM)m_PropertySheetHelper.GetSSNewFilename().c_str());
			break;
		case IDC_PRINTER_DUMP_FILENAME_BROWSE:
			{				
				std::string strPrinterDumpLoc = m_PropertySheetHelper.BrowseToFile(hWnd, "Select printer dump file", REGVALUE_PRINTER_FILENAME, "Text files (*.txt)\0*.txt\0" "All Files\0*.*\0");
				SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)strPrinterDumpLoc.c_str());
			}
			break;
		case IDC_SAVESTATE_ON_EXIT:
			break;
		case IDC_SAVESTATE:
			m_PropertySheetHelper.GetConfigNew().m_uSaveLoadStateMsg = WM_USER_SAVESTATE;
			break;
		case IDC_LOADSTATE:
			m_PropertySheetHelper.GetConfigNew().m_uSaveLoadStateMsg = WM_USER_LOADSTATE;
			break;

		//

		case IDC_THE_FREEZES_F8_ROM_FW:
			{
				const UINT uNewState = IsDlgButtonChecked(hWnd, IDC_THE_FREEZES_F8_ROM_FW) ? 1 : 0;
				m_PropertySheetHelper.GetConfigNew().m_bEnableTheFreezesF8Rom = uNewState;
			}
			break;

		case IDC_CLONETYPE:
			if(HIWORD(wparam) == CBN_SELCHANGE)
			{
				const uint32_t NewCloneMenuItem = (uint32_t) SendDlgItemMessage(hWnd, IDC_CLONETYPE, CB_GETCURSEL, 0, 0);
				const eApple2Type NewCloneType = GetCloneType(NewCloneMenuItem);
				m_PropertySheetHelper.GetConfigNew().m_Apple2Type = NewCloneType;
				m_PropertySheetHelper.GetConfigNew().m_CpuType = ProbeMainCpuDefault(NewCloneType);
			}
			break;

		case IDC_COMBO_GAME_IO_CONNECTOR:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				const DONGLETYPE newCopyProtectionDongleMenuItem = (DONGLETYPE)SendDlgItemMessage(hWnd, IDC_COMBO_GAME_IO_CONNECTOR, CB_GETCURSEL, 0, 0);
				SetCopyProtectionDongleType(newCopyProtectionDongleMenuItem);
			}
			break;

		}
		break;
	case WM_INITDIALOG:
		{
			SendDlgItemMessage(hWnd,IDC_SAVESTATE_FILENAME,WM_SETTEXT,0,(LPARAM)Snapshot_GetFilename().c_str());

			CheckDlgButton(hWnd, IDC_SAVESTATE_ON_EXIT, g_bSaveStateOnExit ? BST_CHECKED : BST_UNCHECKED);
                        
			// SP over SLIP configuration values
			auto& listener = GetCommandListener();
			CheckDlgButton(hWnd, IDC_SPOSLIP_ENABLE_LISTENER, listener.get_start_on_init() ? BST_CHECKED : BST_UNCHECKED);
			SendDlgItemMessage(hWnd, IDC_SPOSLIP_ADDRESS, WM_SETTEXT, 0, (LPARAM) listener.get_ip_address().c_str());
			std::string port_string = std::to_string(listener.get_port());
			std::string request_timeout = std::to_string(listener.get_response_timeout());
			SendDlgItemMessage(hWnd, IDC_SPOSLIP_PORT, WM_SETTEXT, 0, (LPARAM) port_string.c_str());
			SendDlgItemMessage(hWnd, IDC_SPOSLIP_RESPONSE_TIMEOUT, WM_SETTEXT, 0, (LPARAM) request_timeout.c_str());

			if (GetCardMgr().IsParallelPrinterCardInstalled())
			{
				ParallelPrinterCard* card = GetCardMgr().GetParallelPrinterCard();

				CheckDlgButton(hWnd, IDC_DUMPTOPRINTER, card->GetDumpToPrinter() ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hWnd, IDC_PRINTER_CONVERT_ENCODING, card->GetConvertEncoding() ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hWnd, IDC_PRINTER_FILTER_UNPRINTABLE, card->GetFilterUnprintable() ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hWnd, IDC_PRINTER_APPEND, card->GetPrinterAppend() ? BST_CHECKED : BST_UNCHECKED);
				SendDlgItemMessage(hWnd, IDC_SPIN_PRINTER_IDLE, UDM_SETRANGE, 0, MAKELONG(999, 0));
				SendDlgItemMessage(hWnd, IDC_SPIN_PRINTER_IDLE, UDM_SETPOS, 0, MAKELONG(card->GetIdleLimit(), 0));
				SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)card->GetFilename().c_str());

				// Need to specify cmd-line switch: -printer-real to enable this control
				EnableWindow(GetDlgItem(hWnd, IDC_DUMPTOPRINTER), card->GetEnableDumpToRealPrinter() ? TRUE : FALSE);
			}
			else
			{
				EnableWindow(GetDlgItem(hWnd, IDC_DUMPTOPRINTER), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_CONVERT_ENCODING), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_FILTER_UNPRINTABLE), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_APPEND), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_SPIN_PRINTER_IDLE), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_DUMP_FILENAME), FALSE);
			}

			InitOptions(hWnd);

			break;
		}
	}

	return FALSE;
}

void CPageAdvanced::DlgOK(HWND hWnd)
{
	// Update save-state filename
	{
		// NB. if SaveStateSelectImage() was called (by pressing the "Save State -> Browse..." button)
		// and a new save-state file was selected ("OK" from the openfilename dialog) then m_bSSNewFilename etc. will have been set
		m_PropertySheetHelper.SaveStateUpdate();
	}

	g_bSaveStateOnExit = IsDlgButtonChecked(hWnd, IDC_SAVESTATE_ON_EXIT) ? true : false;
	REGSAVE(REGVALUE_SAVE_STATE_ON_EXIT, g_bSaveStateOnExit ? 1 : 0);

	// SP over SLIP
	auto& listener = GetCommandListener();
	auto current_ip = listener.get_ip_address();
	auto current_port = listener.get_port();
	auto current_response_timeout = listener.get_response_timeout();

	bool startSPListenerOnStartup = IsDlgButtonChecked(hWnd, IDC_SPOSLIP_ENABLE_LISTENER) ? true : false;
	listener.set_start_on_init(startSPListenerOnStartup);

	std::string listener_ip_address = GetDialogText(hWnd, IDC_SPOSLIP_ADDRESS, 16);
	listener_ip_address = listener.check_and_set_ip_address(listener_ip_address);
	// the listener will check and set the address to a default value if it wasn't a good format, so send it back to the dialog
	SendDlgItemMessage(hWnd, IDC_SPOSLIP_ADDRESS, WM_SETTEXT, 0, (LPARAM)listener_ip_address.c_str());

	int listener_port = GetDialogNumber(hWnd, IDC_SPOSLIP_PORT, 6);
	if (listener_port > 65535 || listener_port <= 0) listener_port = listener.default_port;
	listener.set_port(static_cast<uint16_t>(listener_port));
	std::string port_string = std::to_string(listener_port);
	SendDlgItemMessage(hWnd, IDC_SPOSLIP_PORT, WM_SETTEXT, 0, (LPARAM)port_string.c_str());

	int listener_response_timeout = GetDialogNumber(hWnd, IDC_SPOSLIP_RESPONSE_TIMEOUT, 6);
	if (listener_response_timeout > 65535 || listener_response_timeout <= 0) listener_response_timeout = listener.default_response_timeout;
	listener.set_response_timeout(static_cast<uint16_t>(listener_response_timeout));
	std::string response_timeout_string = std::to_string(listener_response_timeout);
	SendDlgItemMessage(hWnd, IDC_SPOSLIP_RESPONSE_TIMEOUT, WM_SETTEXT, 0, (LPARAM)response_timeout_string.c_str());

	// WRITE TO REGISTRY
	REGSAVE(TEXT(REGVALUE_START_SP_SLIP_LISTENER), startSPListenerOnStartup ? 1 : 0);
	RegSaveString(TEXT(REG_CONFIG), TEXT(REGVALUE_SP_LISTENER_ADDRESS), 1, listener_ip_address);
	REGSAVE(TEXT(REGVALUE_SP_LISTENER_PORT), listener_port);
	REGSAVE(TEXT(REGVALUE_SP_RESPONSE_TIMEOUT), listener_response_timeout);

	// if the user unchecked start, or if they changed port/address, then always stop
	if (!startSPListenerOnStartup || (current_ip != listener_ip_address) || (current_port != listener_port) || (current_response_timeout != listener_response_timeout)) {
		listener.stop();
	}

	// if the user set "start" and we're not listening, start it.
	if (!listener.get_is_listening() && startSPListenerOnStartup)
	{
		listener.Initialize(listener_ip_address, listener_port, listener_response_timeout);
		listener.start();
	}

	// Save the copy protection dongle type
	RegSetConfigGameIOConnectorNewDongleType(GAME_IO_CONNECTOR, GetCopyProtectionDongleType());

	if (GetCardMgr().IsParallelPrinterCardInstalled())
	{
		ParallelPrinterCard* card = GetCardMgr().GetParallelPrinterCard();

		// Update printer dump filename
		{
			std::string filename = GetDialogText(hWnd, IDC_PRINTER_DUMP_FILENAME, MAX_PATH);
			card->SetFilename(filename);
		}

		card->SetDumpToPrinter(IsDlgButtonChecked(hWnd, IDC_DUMPTOPRINTER) ? true : false);
		card->SetConvertEncoding(IsDlgButtonChecked(hWnd, IDC_PRINTER_CONVERT_ENCODING) ? true : false);
		card->SetFilterUnprintable(IsDlgButtonChecked(hWnd, IDC_PRINTER_FILTER_UNPRINTABLE) ? true : false);
		card->SetPrinterAppend(IsDlgButtonChecked(hWnd, IDC_PRINTER_APPEND) ? true : false);

		card->SetIdleLimit((short)SendDlgItemMessage(hWnd, IDC_SPIN_PRINTER_IDLE, UDM_GETPOS, 0, 0));

		// Now save all the above to Registry
		card->SetRegistryConfig();
	}

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void CPageAdvanced::InitOptions(HWND hWnd)
{
	InitFreezeDlgButton(hWnd);
	InitCloneDropdownMenu(hWnd);
	InitGameIOConnectorDropdownMenu(hWnd);
}

// Advanced->Clone: Menu item to eApple2Type
eApple2Type CPageAdvanced::GetCloneType(uint32_t NewMenuItem)
{
	switch (NewMenuItem)
	{
		case MENUITEM_PRAVETS82:	return A2TYPE_PRAVETS82;
		case MENUITEM_PRAVETS8M:	return A2TYPE_PRAVETS8M;
		case MENUITEM_PRAVETS8A:	return A2TYPE_PRAVETS8A;
		case MENUITEM_TK30002E:		return A2TYPE_TK30002E;
		case MENUITEM_BASE64A:		return A2TYPE_BASE64A;
		default:					return A2TYPE_PRAVETS82;
	}
}

int CPageAdvanced::GetCloneMenuItem(void)
{
	const eApple2Type type = m_PropertySheetHelper.GetConfigNew().m_Apple2Type;
	const bool bIsClone = IsClone(type);
	if (!bIsClone)
		return MENUITEM_CLONEMIN;

	int nMenuItem = MENUITEM_CLONEMIN;
	switch (type)
	{
		case A2TYPE_CLONE:	// Set as generic clone type from Config page
			{
				// Need to set a real clone type & CPU in case the user never touches the clone menu
				nMenuItem = MENUITEM_CLONEMIN;
				const eApple2Type NewCloneType = GetCloneType(MENUITEM_CLONEMIN);
				m_PropertySheetHelper.GetConfigNew().m_Apple2Type = GetCloneType(NewCloneType);
				m_PropertySheetHelper.GetConfigNew().m_CpuType = ProbeMainCpuDefault(NewCloneType);
			}
			break;
		case A2TYPE_PRAVETS82:	nMenuItem = MENUITEM_PRAVETS82; break;
		case A2TYPE_PRAVETS8M:	nMenuItem = MENUITEM_PRAVETS8M; break;
		case A2TYPE_PRAVETS8A:	nMenuItem = MENUITEM_PRAVETS8A; break;
		case A2TYPE_TK30002E:	nMenuItem = MENUITEM_TK30002E;  break;
		case A2TYPE_BASE64A:	nMenuItem = MENUITEM_BASE64A;   break;
		default:	// New clone needs adding here?
			_ASSERT(0);
	}

	return nMenuItem;
}

void CPageAdvanced::InitFreezeDlgButton(HWND hWnd)
{
	const bool bIsApple2Plus = IsApple2Plus( m_PropertySheetHelper.GetConfigNew().m_Apple2Type );
	EnableWindow(GetDlgItem(hWnd, IDC_THE_FREEZES_F8_ROM_FW), bIsApple2Plus ? TRUE : FALSE);

	const UINT CheckTheFreezesRom = m_PropertySheetHelper.GetConfigNew().m_bEnableTheFreezesF8Rom ? BST_CHECKED : BST_UNCHECKED;
	CheckDlgButton(hWnd, IDC_THE_FREEZES_F8_ROM_FW, CheckTheFreezesRom);
}

void CPageAdvanced::InitCloneDropdownMenu(HWND hWnd)
{
	// Set clone menu choice (ok even if it's not a clone)
	const int nCurrentChoice = GetCloneMenuItem();
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_CLONETYPE, m_CloneChoices, nCurrentChoice);

	const bool bIsClone = IsClone( m_PropertySheetHelper.GetConfigNew().m_Apple2Type );
	EnableWindow(GetDlgItem(hWnd, IDC_CLONETYPE), bIsClone ? TRUE : FALSE);
}

void CPageAdvanced::InitGameIOConnectorDropdownMenu(HWND hWnd)
{
	// Set copy protection dongle menu choice
	const int nCurrentChoice = GetCopyProtectionDongleType();
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_GAME_IO_CONNECTOR, m_gameIOConnectorChoices, nCurrentChoice);
}
