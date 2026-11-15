#include "workbench/embedded/device_adapter.hpp"
#include <sstream>
#include <iomanip>

namespace workbench {
namespace embedded {

// --- McuAdapter Implementation ---

McuAdapter::McuAdapter() = default;

bool McuAdapter::discover() {
    if (state_ != DeviceState::DISCONNECTED && state_ != DeviceState::DISCOVERED) return false;
    state_ = DeviceState::DISCOVERED;
    return true;
}

std::string McuAdapter::get_capability_report() {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"device_id\": \"" << id() << "\",\n"
       << "  \"name\": \"" << name() << "\",\n"
       << "  \"device_type\": \"MCU\",\n"
       << "  \"architecture\": \"ARM Cortex-M4F 32-bit RISC\",\n"
       << "  \"clock_speed_mhz\": 84.0,\n"
       << "  \"flash_memory_kb\": 512,\n"
       << "  \"sram_kb\": 96,\n"
       << "  \"peripherals\": [\"USART1 (115200 8N1)\", \"SPI1\", \"I2C1\", \"12-bit ADC\", \"Timer2/3/4\"],\n"
       << "  \"debug_interface\": \"SWD (Serial Wire Debug: SWCLK, SWDIO, nRST)\",\n"
       << "  \"voltage_domain\": \"" << voltage_domain() << "\",\n"
       << "  \"host_interface\": \"" << host_interface() << "\",\n"
       << "  \"debug_role\": \"" << debug_role() << "\"\n"
       << "}";
    return ss.str();
}

bool McuAdapter::connect() {
    if (state_ != DeviceState::DISCOVERED || failure_injected_) return false;
    state_ = DeviceState::CONNECTED;
    packet_seq_ = 0;
    uptime_sec_ = 0;
    return true;
}

bool McuAdapter::disconnect() {
    state_ = state_ == DeviceState::DISCONNECTED ? DeviceState::DISCONNECTED : DeviceState::DISCOVERED;
    failure_injected_ = false;
    return true;
}

std::string McuAdapter::poll_telemetry() {
    if (failure_injected_) {
        state_ = DeviceState::ERROR_TIMEOUT;
        return "{\"status\": \"ERROR_TIMEOUT\", \"error\": \"UART RX timeout (deadline 250ms exceeded without ACK). Recoverable failure active.\"}";
    }

    if (state_ != DeviceState::CONNECTED && state_ != DeviceState::STREAMING) {
        return "{\"status\": \"" + to_string(state_) + "\", \"error\": \"Device is not connected.\"}";
    }

    state_ = DeviceState::STREAMING;
    packet_seq_++;
    uptime_sec_ += 1;
    simulated_temp_ += 0.05 * ((packet_seq_ % 5 == 0) ? -1.0 : 1.0);

    std::ostringstream ss;
    ss << std::setprecision(3);
    ss << "{\n"
       << "  \"status\": \"STREAMING\",\n"
       << "  \"device_id\": \"" << id() << "\",\n"
       << "  \"packet_seq\": " << packet_seq_ << ",\n"
       << "  \"uptime_sec\": " << uptime_sec_ << ",\n"
       << "  \"core_temperature_C\": " << simulated_temp_ << ",\n"
       << "  \"supply_voltage_V\": 3.298,\n"
       << "  \"cpu_load_percent\": 18.4,\n"
       << "  \"heartbeat\": \"PULSE_OK\"\n"
       << "}";
    return ss.str();
}

bool McuAdapter::inject_failure(const std::string& failure_type) {
    if (failure_type != "TIMEOUT" || (state_ != DeviceState::CONNECTED && state_ != DeviceState::STREAMING)) return false;
    failure_injected_ = true;
    state_ = DeviceState::ERROR_TIMEOUT;
    return true;
}

bool McuAdapter::recover() {
    // Automated recovery: flush buffers, re-handshake UART
    if (state_ != DeviceState::ERROR_TIMEOUT || !failure_injected_) return false;
    failure_injected_ = false;
    state_ = DeviceState::CONNECTED;
    return true;
}

// --- FpgaAdapter Implementation ---

FpgaAdapter::FpgaAdapter() = default;

bool FpgaAdapter::discover() {
    if (state_ != DeviceState::DISCONNECTED && state_ != DeviceState::DISCOVERED) return false;
    state_ = DeviceState::DISCOVERED;
    return true;
}

std::string FpgaAdapter::get_capability_report() {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"device_id\": \"" << id() << "\",\n"
       << "  \"name\": \"" << name() << "\",\n"
       << "  \"device_type\": \"FPGA\",\n"
       << "  \"part_number\": \"XC7A35T-1CSG324C\",\n"
       << "  \"logic_cells\": 33280,\n"
       << "  \"dsp_slices\": 90,\n"
       << "  \"block_ram_kb\": 1800,\n"
       << "  \"clock_domains\": [\"SysClk 100MHz\", \"DDRClk 200MHz\", \"UserClk 50MHz\"],\n"
       << "  \"config_modes\": [\"Master SPI Flash (Quad SPI)\", \"Slave JTAG (Boundary-Scan)\"],\n"
       << "  \"voltage_domain\": \"" << voltage_domain() << "\",\n"
       << "  \"host_interface\": \"" << host_interface() << "\",\n"
       << "  \"debug_role\": \"" << debug_role() << "\"\n"
       << "}";
    return ss.str();
}

bool FpgaAdapter::connect() {
    if (state_ != DeviceState::DISCOVERED || failure_injected_) return false;
    state_ = DeviceState::CONNECTED;
    packet_seq_ = 0;
    uptime_sec_ = 0;
    return true;
}

bool FpgaAdapter::disconnect() {
    state_ = state_ == DeviceState::DISCONNECTED ? DeviceState::DISCONNECTED : DeviceState::DISCOVERED;
    failure_injected_ = false;
    return true;
}

std::string FpgaAdapter::poll_telemetry() {
    if (failure_injected_) {
        state_ = DeviceState::ERROR_TIMEOUT;
        return "{\"status\": \"ERROR_TIMEOUT\", \"error\": \"FTDI FIFO buffer overflow/timeout: Handshake stall detected. Recoverable failure active.\"}";
    }

    if (state_ != DeviceState::CONNECTED && state_ != DeviceState::STREAMING) {
        return "{\"status\": \"" + to_string(state_) + "\", \"error\": \"Device is not connected.\"}";
    }

    state_ = DeviceState::STREAMING;
    packet_seq_++;
    uptime_sec_ += 1;
    simulated_temp_ += 0.08 * ((packet_seq_ % 7 == 0) ? -1.0 : 1.0);

    std::ostringstream ss;
    ss << std::setprecision(4);
    ss << "{\n"
       << "  \"status\": \"STREAMING\",\n"
       << "  \"device_id\": \"" << id() << "\",\n"
       << "  \"packet_seq\": " << packet_seq_ << ",\n"
       << "  \"uptime_sec\": " << uptime_sec_ << ",\n"
       << "  \"xadc_die_temp_C\": " << simulated_temp_ << ",\n"
       << "  \"vccint_1v0_V\": 1.002,\n"
       << "  \"vccaux_1v8_V\": 1.805,\n"
       << "  \"vcco_3v3_V\": 3.310,\n"
       << "  \"pll_status\": \"LOCKED\",\n"
       << "  \"crc_errors\": 0\n"
       << "}";
    return ss.str();
}

bool FpgaAdapter::inject_failure(const std::string& failure_type) {
    if (failure_type != "TIMEOUT" || (state_ != DeviceState::CONNECTED && state_ != DeviceState::STREAMING)) return false;
    failure_injected_ = true;
    state_ = DeviceState::ERROR_TIMEOUT;
    return true;
}

bool FpgaAdapter::recover() {
    if (state_ != DeviceState::ERROR_TIMEOUT || !failure_injected_) return false;
    failure_injected_ = false;
    state_ = DeviceState::CONNECTED;
    return true;
}

// --- JtagAdapter Implementation ---

JtagAdapter::JtagAdapter() = default;

bool JtagAdapter::discover() {
    if (state_ != DeviceState::DISCONNECTED && state_ != DeviceState::DISCOVERED) return false;
    state_ = DeviceState::DISCOVERED;
    return true;
}

std::string JtagAdapter::get_capability_report() {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"device_id\": \"" << id() << "\",\n"
       << "  \"name\": \"" << name() << "\",\n"
       << "  \"device_type\": \"JTAG_ADAPTER\",\n"
       << "  \"role_clarification\": \"HARDWARE DEBUG INTERFACE (masters TAP bus controller; NOT a compute node)\",\n"
       << "  \"supported_protocols\": [\"IEEE 1149.1 JTAG (4-wire/5-wire)\", \"ARM 2-wire Serial Wire Debug (SWD)\"],\n"
       << "  \"target_voltage_sensing\": \"1.2V to 3.3V automatic bidirectional level-shifting buffers\",\n"
       << "  \"max_tck_frequency_mhz\": 15.0,\n"
       << "  \"signal_ownership\": {\n"
       << "    \"TCK\": \"Driven by Adapter (Master)\",\n"
       << "    \"TMS_SWDIO\": \"Driven by Adapter / Bidirectional in SWD\",\n"
       << "    \"TDI\": \"Driven by Adapter (Master)\",\n"
       << "    \"TDO_SWO\": \"Sampled by Adapter from Target (Slave Output)\",\n"
       << "    \"nSRST\": \"Open-drain active-low reset driven by Adapter\",\n"
       << "    \"VTREF\": \"Input power-sense supplied by Target Board\"\n"
       << "  },\n"
       << "  \"voltage_domain\": \"" << voltage_domain() << "\",\n"
       << "  \"host_interface\": \"" << host_interface() << "\"\n"
       << "}";
    return ss.str();
}

bool JtagAdapter::connect() {
    if (state_ != DeviceState::DISCOVERED || failure_injected_) return false;
    state_ = DeviceState::CONNECTED;
    scan_cycles_ = 0;
    return true;
}

bool JtagAdapter::disconnect() {
    state_ = state_ == DeviceState::DISCONNECTED ? DeviceState::DISCONNECTED : DeviceState::DISCOVERED;
    failure_injected_ = false;
    return true;
}

std::string JtagAdapter::poll_telemetry() {
    if (failure_injected_) {
        state_ = DeviceState::ERROR_TIMEOUT;
        return "{\"status\": \"ERROR_TIMEOUT\", \"error\": \"JTAG TAP State Machine desync / VREF brownout detected. Recoverable failure active.\"}";
    }

    if (state_ != DeviceState::CONNECTED && state_ != DeviceState::STREAMING) {
        return "{\"status\": \"" + to_string(state_) + "\", \"error\": \"Device is not connected.\"}";
    }

    state_ = DeviceState::STREAMING;
    scan_cycles_ += 1000;

    std::ostringstream ss;
    ss << std::setprecision(4);
    ss << "{\n"
       << "  \"status\": \"STREAMING\",\n"
       << "  \"device_id\": \"" << id() << "\",\n"
       << "  \"scan_cycles\": " << scan_cycles_ << ",\n"
       << "  \"target_vref_detected_V\": " << detected_vref_ << ",\n"
       << "  \"tck_frequency_mhz\": 10.0,\n"
       << "  \"tap_state\": \"Run-Test/Idle\",\n"
       << "  \"target_idcode\": \"0x4BA00477 (ARM Cortex-M4 Core) / 0x0362D093 (Artix-7 TAP)\",\n"
       << "  \"signal_integrity\": \"OPTIMAL (0 signal glitches detected)\"\n"
       << "}";
    return ss.str();
}

bool JtagAdapter::inject_failure(const std::string& failure_type) {
    if (failure_type != "TIMEOUT" || (state_ != DeviceState::CONNECTED && state_ != DeviceState::STREAMING)) return false;
    failure_injected_ = true;
    state_ = DeviceState::ERROR_TIMEOUT;
    return true;
}

bool JtagAdapter::recover() {
    // Automated JTAG recovery: send 5 consecutive TMS=1 clock cycles to force TAP controller into Test-Logic-Reset
    if (state_ != DeviceState::ERROR_TIMEOUT || !failure_injected_) return false;
    failure_injected_ = false;
    state_ = DeviceState::CONNECTED;
    return true;
}

// --- DeviceManager Implementation ---

DeviceManager::DeviceManager()
    : mcu_(std::make_unique<McuAdapter>()),
      fpga_(std::make_unique<FpgaAdapter>()),
      jtag_(std::make_unique<JtagAdapter>())
{
}

std::vector<IDeviceAdapter*> DeviceManager::get_all_devices() {
    return {mcu_.get(), fpga_.get(), jtag_.get()};
}

IDeviceAdapter* DeviceManager::get_device(const std::string& id) {
    if (id == mcu_->id()) return mcu_.get();
    if (id == fpga_->id()) return fpga_.get();
    if (id == jtag_->id()) return jtag_.get();
    return nullptr;
}

std::string DeviceManager::serialize_all_status_json() {
    std::ostringstream ss;
    ss << "[\n";
    auto devices = get_all_devices();
    for (size_t i = 0; i < devices.size(); ++i) {
        auto* dev = devices[i];
        ss << "  {\n"
           << "    \"id\": \"" << dev->id() << "\",\n"
           << "    \"name\": \"" << dev->name() << "\",\n"
           << "    \"device_type\": \"" << dev->device_type() << "\",\n"
           << "    \"state\": \"" << to_string(dev->state()) << "\",\n"
           << "    \"is_connected\": " << (dev->is_connected() ? "true" : "false") << ",\n"
           << "    \"is_discovered\": " << (dev->state() != DeviceState::DISCONNECTED ? "true" : "false") << ",\n"
           << "    \"voltage_domain\": \"" << dev->voltage_domain() << "\",\n"
           << "    \"host_interface\": \"" << dev->host_interface() << "\",\n"
           << "    \"debug_role\": \"" << dev->debug_role() << "\"\n"
           << "  }" << (i + 1 < devices.size() ? ",\n" : "\n");
    }
    ss << "]";
    return ss.str();
}

} // namespace embedded
} // namespace workbench
