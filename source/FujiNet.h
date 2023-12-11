// ReSharper disable CppInconsistentNaming
#pragma once

#include "Card.h"

#include <memory>

#include "CPU.h"
#include "SPoverSLIP/Listener.h"
#include "SPoverSLIP/Response.h"

class ControlResponse;
class InitResponse;

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
	SP_CMD_WRITE        = 9,
	SP_CMD_RESET        = 10
};


class FujiNet : public Card
{
public:
    static const std::string& GetSnapshotCardName();

    explicit FujiNet(UINT slot);
    ~FujiNet() override;

    void Destroy() override;
    void InitializeIO(LPBYTE pCxRomPeripheral) override;
    void Reset(bool powerCycle) override;
    void Update(ULONG nExecutedCycles) override;
    void SaveSnapshot(YamlSaveHelper& yamlSaveHelper) override;
    bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version) override;

	BYTE io_write0(WORD programCounter, WORD address, BYTE value, ULONG nCycles);
	static void device_count(WORD sp_payload_loc);
    void process_sp_over_slip();
    void status(BYTE unit_number, Connection* connection, WORD sp_payload_loc, BYTE status_code);
    void control(BYTE unit_number, Connection* connection, WORD sp_payload_loc, BYTE control_code);
    void init(BYTE unit_number, Connection* connection);
	void open(BYTE unit_number, Connection* connection);
	void close(BYTE unit_number, Connection* connection);
	void format(BYTE unit_number, Connection* connection);
	void reset(BYTE unit_number, Connection* connection);
    void read_block(BYTE unit_number, Connection* connection, WORD sp_payload_loc, WORD params_loc);
    void write_block(BYTE unit_number, Connection* connection, WORD sp_payload_loc, WORD params_loc);
    void read(BYTE unit_number, Connection* connection, WORD sp_payload_loc, WORD params_loc);
    void write(BYTE unit_number, Connection* connection, WORD sp_payload_loc, WORD params_loc);

	static void set_processor_status(const uint8_t flags) { regs.ps |= flags; }
    static void unset_processor_status(const uint8_t flags) { regs.ps &= (0xFF - flags); }
    // if condition is true then set the flags given, else remove them.
    static void update_processor_status(const bool condition, const uint8_t flags) { condition ? set_processor_status(flags) : unset_processor_status(flags); }

	// SP over SLIP
    void create_listener();

	template <typename T>
	void handle_simple_response(const std::unique_ptr<Response> response) {
		static_assert(std::is_base_of<Response, T>::value, "T must be a subclass of Response");
		auto specific_response = dynamic_cast<T*>(response.get());

		if (specific_response != nullptr)
		{
			const BYTE status = specific_response->get_status();
			regs.a = status;
			regs.x = 0;
			regs.y = 0;
			update_processor_status(status == 0, AF_ZERO);
		}
		else
		{
			regs.a = 1;		// TODO: what error should we return?
			regs.x = 0;
			regs.y = 0;
			unset_processor_status(AF_ZERO);
		}
	}

    template <typename T, typename Func>
    void handle_response(const std::unique_ptr<Response> response, Func call_back) {
        auto specific_response = dynamic_cast<T*>(response.get());

        if (specific_response != nullptr)
        {
            if (specific_response->get_status() == 0)
            {
                call_back(specific_response);
                set_processor_status(AF_ZERO);
            }
            else
            {
                // An error in the response
                regs.a = specific_response->get_status();
                regs.x = 0;
                regs.y = 0;
                unset_processor_status(AF_ZERO);
            }
        }
        else
        {
            // An error trying to do the request, as there was no response
            regs.a = 1;		// TODO: what error should we return?
            regs.x = 0;
            regs.y = 0;
            unset_processor_status(AF_ZERO);
        }
    }
private:
    // SP over SLIP
    std::unique_ptr<Listener> listener_;
};