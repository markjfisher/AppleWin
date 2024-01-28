#pragma once

#include "Disk.h"
#include "Harddisk.h"


void LoadConfiguration(bool loadImages);
void InsertFloppyDisks(const UINT slot, LPCSTR szImageName_drive[NUM_DRIVES], bool driveConnected[NUM_DRIVES], bool& bBoot);
void InsertHardDisks(const UINT slot, LPCSTR szImageName_harddisk[NUM_HARDDISKS], bool& bBoot);
void GetAppleWindowTitle();

void CtrlReset();
void ResetMachineState();
std::string GetDialogText(HWND hWnd, int control_id, size_t max_length);
int GetDialogNumber(HWND hWnd, int control_id, size_t max_length);