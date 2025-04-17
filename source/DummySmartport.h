// ReSharper disable CppInconsistentNaming
#pragma once

#include "Card.h"

#include <memory>

#include "CPU.h"

class DummySmartport : public Card
{
public:
	static const std::string& GetSnapshotCardName();

	explicit DummySmartport(UINT slot);
	~DummySmartport() override;

	void Destroy() override;
	void InitializeIO(LPBYTE pCxRomPeripheral) override;
	void Reset(bool powerCycle) override;
	void Update(ULONG nExecutedCycles) override;
	void SaveSnapshot(YamlSaveHelper& yamlSaveHelper) override;
	bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version) override;

	BYTE io_write0(WORD programCounter, WORD address, BYTE value, ULONG nCycles);

	static void device_count(WORD sp_payload_loc);
	void handle_smartport_call();
	void handle_prodos_call();

	static void set_processor_status(const uint8_t flags) { regs.ps |= flags; }
	static void unset_processor_status(const uint8_t flags) { regs.ps &= (0xFF - flags); }

}; 