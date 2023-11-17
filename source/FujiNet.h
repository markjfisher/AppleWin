// ReSharper disable CppInconsistentNaming
#pragma once

#include "Card.h"

#include <vector>
#include <memory>
#include <map>
#include "W5100.h"
#include "SPoverSLIP/Listener.h"

/*
* Documentation from
*
*/

enum
{
	SP_CMD_STATUS       = 0,
	SP_CMD_READBLOCK    = 1,
	SP_CMD_WRITEBLOCK   = 2,
	SP_CMD_FORMAT       = 3,
	SP_CMD_CONTROL      = 4,
	SP_CMD_INIT         = 5,
	SP_CMD_OPEN         = 6,
	SP_CMD_CLOSE        = 7,
	SP_CMD_READ         = 8,
	SP_CMD_WRITE        = 9
};


class FujiNet final : public Card
{
public:
    static const std::string& GetSnapshotCardName();

    explicit FujiNet(const UINT slot);
    ~FujiNet();

    void Destroy() override;
    void InitializeIO(LPBYTE pCxRomPeripheral) override;
    void Reset(const bool powerCycle) override;
    void Update(const ULONG nExecutedCycles) override;
    void SaveSnapshot(YamlSaveHelper& yamlSaveHelper) override;
    bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version) override;

    BYTE IOWrite0(WORD programCounter, WORD address, BYTE value, ULONG nCycles);
    static void deviceCount(WORD spPayloadLoc);
    void dib(BYTE unit_number, WORD sp_payload_loc) const;
    void status(BYTE unitNumber, WORD spPayloadLoc, WORD paramsLoc) const;
    void processSPoverSLIP();

    // SP over SLIP
    void createListener();

private:
    // SP over SLIP
    std::unique_ptr<Listener> listener_;
};