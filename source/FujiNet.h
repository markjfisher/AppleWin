#pragma once

#include "Card.h"

#include <vector>
#include <map>
#include "W5100.h"

/*
* Documentation from
*
*/

class FujiNet : public Card
{
public:
    static const std::string& GetSnapshotCardName();

    enum PacketDestination { HOST, BROADCAST, OTHER };

    FujiNet(UINT slot);
    virtual ~FujiNet();

    virtual void Destroy(void) {}
    virtual void InitializeIO(LPBYTE pCxRomPeripheral);
    virtual void Reset(const bool powerCycle);
    virtual void Update(const ULONG nExecutedCycles);
    virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
    virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

    BYTE IOWrite0(WORD programcounter, WORD address, BYTE value, ULONG nCycles);
    BYTE IORead0(WORD programcounter, WORD address, ULONG nCycles);
    void process(void);
    void device_count(void);
    void dib(uint8_t dest);
    void backupData(BYTE v);
    void addToBuffer(BYTE v);
    BYTE restoreData();

private:
    void resetBuffer();

    BYTE buffer[1024];
    BYTE backup[24];
    unsigned long bufferLen;
    int bufferReadIndex;
    int backupIndex;
    int restoreIndex;
};