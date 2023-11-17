// ReSharper disable CppInconsistentNaming
#pragma once

#include "Card.h"

#include <memory>

#include "CPU.h"
#include "SPoverSLIP/Listener.h"

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
    ~FujiNet() override;

    void Destroy() override;
    void InitializeIO(LPBYTE pCxRomPeripheral) override;
    void Reset(const bool powerCycle) override;
    void Update(const ULONG nExecutedCycles) override;
    void SaveSnapshot(YamlSaveHelper& yamlSaveHelper) override;
    bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version) override;

	BYTE io_write0(WORD programCounter, WORD address, BYTE value, ULONG nCycles) const;
	static void device_count(WORD sp_payload_loc);
    void process_sp_over_slip() const;
    static void status(BYTE unit_number, Connection* connection, WORD sp_payload_loc, BYTE status_code);
    static void control(BYTE unit_number, Connection* connection, WORD sp_payload_loc, BYTE control_code);

    static void set_processor_status(const uint8_t flags) { regs.ps |= flags; }
    static void unset_processor_status(const uint8_t flags) { regs.ps &= (0xFF - flags); }
    // if condition is true then set the flags given, else remove them.
    static void update_processor_status(const bool condition, const uint8_t flags) { condition ? set_processor_status(flags) : unset_processor_status(flags); }

	// SP over SLIP
    void create_listener();

private:
    // SP over SLIP
    std::unique_ptr<Listener> listener_;
};