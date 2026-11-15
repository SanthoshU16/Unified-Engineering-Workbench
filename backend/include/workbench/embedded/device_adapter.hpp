#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>

namespace workbench {
namespace embedded {

enum class DeviceState {
    DISCONNECTED,
    DISCOVERED,
    CONNECTED,
    STREAMING,
    ERROR_TIMEOUT,
    RECOVERING
};

inline std::string to_string(DeviceState state) {
    switch (state) {
        case DeviceState::DISCONNECTED:  return "DISCONNECTED";
        case DeviceState::DISCOVERED:    return "DISCOVERED";
        case DeviceState::CONNECTED:     return "CONNECTED";
        case DeviceState::STREAMING:     return "STREAMING";
        case DeviceState::ERROR_TIMEOUT: return "ERROR_TIMEOUT";
        case DeviceState::RECOVERING:    return "RECOVERING";
        default:                         return "UNKNOWN";
    }
}

// Abstract interface for all host-connected workbench hardware adapters
class IDeviceAdapter {
public:
    virtual ~IDeviceAdapter() = default;

    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string device_type() const = 0; // "MCU", "FPGA", "JTAG_ADAPTER"
    virtual DeviceState state() const = 0;
    virtual bool is_connected() const = 0;

    // Operations
    virtual bool discover() = 0;
    virtual std::string get_capability_report() = 0;
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual std::string poll_telemetry() = 0;
    virtual bool inject_failure(const std::string& failure_type = "TIMEOUT") = 0;
    virtual bool recover() = 0;

    // Safety and electrical metadata
    virtual std::string voltage_domain() const = 0;
    virtual std::string host_interface() const = 0;
    virtual std::string debug_role() const = 0;
};

// Microcontroller Board Adapter (e.g. STM32F401 ARM Cortex-M4)
class McuAdapter : public IDeviceAdapter {
public:
    McuAdapter();
    std::string id() const override { return "mcu_stm32f4"; }
    std::string name() const override { return "STM32F401 Discovery MCU"; }
    std::string device_type() const override { return "MCU"; }
    DeviceState state() const override { return state_; }
    bool is_connected() const override { return state_ == DeviceState::CONNECTED || state_ == DeviceState::STREAMING; }

    bool discover() override;
    std::string get_capability_report() override;
    bool connect() override;
    bool disconnect() override;
    std::string poll_telemetry() override;
    bool inject_failure(const std::string& failure_type = "TIMEOUT") override;
    virtual bool recover() override;

    std::string voltage_domain() const override { return "3.3V VDD logic (5.0V USB VBUS power)"; }
    std::string host_interface() const override { return "USB-CDC Virtual COM Port (115200 8N1)"; }
    std::string debug_role() const override { return "Target Compute Node (receives firmware, executes control loop)"; }

private:
    DeviceState state_{DeviceState::DISCONNECTED};
    uint64_t packet_seq_{0};
    uint32_t uptime_sec_{0};
    bool failure_injected_{false};
    double simulated_temp_{42.5};
};

// FPGA Board Adapter (e.g. Xilinx Artix-7 XC7A35T)
class FpgaAdapter : public IDeviceAdapter {
public:
    FpgaAdapter();
    std::string id() const override { return "fpga_artix7"; }
    std::string name() const override { return "Xilinx Artix-7 XC7A35T FPGA"; }
    std::string device_type() const override { return "FPGA"; }
    DeviceState state() const override { return state_; }
    bool is_connected() const override { return state_ == DeviceState::CONNECTED || state_ == DeviceState::STREAMING; }

    bool discover() override;
    std::string get_capability_report() override;
    bool connect() override;
    bool disconnect() override;
    std::string poll_telemetry() override;
    bool inject_failure(const std::string& failure_type = "TIMEOUT") override;
    virtual bool recover() override;

    std::string voltage_domain() const override { return "Multi-rail: 1.0V VCCINT, 1.8V VCCAUX, 3.3V VCCO I/O Banks"; }
    std::string host_interface() const override { return "FTDI FT2232H High-Speed USB FIFO / UART"; }
    std::string debug_role() const override { return "Target Hardware Accelerator (programmable gate array fabric)"; }

private:
    DeviceState state_{DeviceState::DISCONNECTED};
    uint64_t packet_seq_{0};
    uint32_t uptime_sec_{0};
    bool failure_injected_{false};
    double simulated_temp_{53.2};
};

// Dedicated JTAG Debug Adapter (e.g. SEGGER J-Link / FTDI MPSSE)
class JtagAdapter : public IDeviceAdapter {
public:
    JtagAdapter();
    std::string id() const override { return "jtag_probe"; }
    std::string name() const override { return "J-Link / MPSSE Dedicated JTAG Adapter"; }
    std::string device_type() const override { return "JTAG_ADAPTER"; }
    DeviceState state() const override { return state_; }
    bool is_connected() const override { return state_ == DeviceState::CONNECTED || state_ == DeviceState::STREAMING; }

    bool discover() override;
    std::string get_capability_report() override;
    bool connect() override;
    bool disconnect() override;
    std::string poll_telemetry() override;
    bool inject_failure(const std::string& failure_type = "TIMEOUT") override;
    virtual bool recover() override;

    std::string voltage_domain() const override { return "Target VREF Sense (1.2V - 3.3V Level-Shifted Buffers)"; }
    std::string host_interface() const override { return "USB 2.0 High-Speed Bulk Debug Endpoint"; }
    std::string debug_role() const override {
        return "Dedicated Hardware Debug Host (masters TCK, TMS, TDI; samples TDO; controls target TAP state machine; NOT a compute node)";
    }

private:
    DeviceState state_{DeviceState::DISCONNECTED};
    uint64_t scan_cycles_{0};
    bool failure_injected_{false};
    double detected_vref_{3.30};
};

// Manager coordinating all hardware adapters in the workbench
class DeviceManager {
public:
    DeviceManager();

    std::vector<IDeviceAdapter*> get_all_devices();
    IDeviceAdapter* get_device(const std::string& id);

    std::string serialize_all_status_json();

private:
    std::unique_ptr<McuAdapter> mcu_;
    std::unique_ptr<FpgaAdapter> fpga_;
    std::unique_ptr<JtagAdapter> jtag_;
};

} // namespace embedded
} // namespace workbench

