# Embedded-System Integration Design & Workbench Specification (Task E)

**Document ID:** UEW-EMB-SPEC-001  
**Project:** Unified Engineering Workbench  
**Assessment:** PASS Science Works | R&D Internship 2026–2027  

---

## 1. System Overview

The host-connected embedded workbench integrates three heterogeneous target devices into a unified diagnostic, telemetry, and debugging environment:
1. **Microcontroller Board (MCU)**: STMicroelectronics STM32F401 Discovery (ARM Cortex-M4F @ 84 MHz).
2. **Field-Programmable Gate Array (FPGA)**: Xilinx Artix-7 XC7A35T-1CSG324C.
3. **Dedicated JTAG Debug Adapter**: SEGGER J-Link / FTDI FT2232H MPSSE Hardware Debug Probe.

---

## 2. Interface, Pin-Voltage & Signal Ownership Table

The following table details the power distribution, ground references, logic voltage domains, level-shifting requirements, connector types, host communication protocols, and signal ownership assumptions:

| Specification Domain | Microcontroller Board (MCU) | FPGA Board (Artix-7) | Dedicated JTAG Debug Adapter |
| :--- | :--- | :--- | :--- |
| **Device Architecture** | 32-bit ARM Cortex-M4F (Hardware FPU) | Programmable Gate Array (33,280 Logic Cells, 90 DSPs) | USB-to-JTAG/SWD Protocol Engine (Hardware TAP Controller) |
| **System Role** | **Target Compute Node** (runs control algorithms) | **Target Hardware Accelerator** (DSP / custom logic) | **Dedicated Hardware Debug Host** (inspects/programs target) |
| **Primary Power Supply** | +5.0V VBUS via USB Micro-B | +5.0V VBUS via USB Type-B or external 5V/2A DC jack | +5.0V VBUS from Host USB Root Port (< 150 mA) |
| **On-Board Voltage Regulators** | Low-Dropout Regulator (LDO) stepping 5.0V $\to$ 3.3V | Multi-rail Switching PMIC: 1.0V (VCCINT), 1.8V (VCCAUX), 3.3V (VCCO) | Internal 3.3V LDO for core probe logic; target-sensing I/O buffers |
| **I/O Logic Voltage Domain** | Single rail: 3.3V LVCMOS | Multi-rail: 1.8V Bank 14/15, 3.3V Bank 34/35 | Dynamic Target Reference: 1.2V to 3.3V auto-sensing buffer |
| **Level Shifting Required?** | **No** (Direct 3.3V host UART communication) | **Yes** (Bidirectional TXS0108E / TXB0104 translators between 1.8V and 3.3V) | **Integrated** (Active bidirectional voltage-translating buffers) |
| **Grounding Architecture** | Star ground connected to Host USB Shield Ground | Digital/Analog split ground plane with star tie to Host Shield | Floating ground tied to Target Ground via JTAG Pin 4/6/8/10 |
| **Host Communication** | USB-CDC Virtual COM Port (115200 Baud, 8N1) | FTDI FT2232H High-Speed USB FIFO (Channel A, up to 30 MB/s) | USB 2.0 High-Speed Bulk Endpoint (WinUSB / libusb driver) |
| **Physical Debug Connector** | 10-pin 0.05" Micro-Header (ARM Cortex SWD) | 14-pin 2.00mm Header (Xilinx JTAG Specification) | Standard 20-pin 0.10" Ribbon Header with target breakout cable |
| **JTAG/SWD Signal Ownership** | **Target Slave** (SWCLK in, SWDIO bidirectional, nRST in) | **Target TAP Slave** (TCK in, TMS in, TDI in, TDO out, VTREF out) | **Debug Master** (Drives TCK, TMS, TDI, nSRST; Samples TDO, VTREF) |

---

## 3. Critical Architectural Analysis: Debug Interface vs Compute Node

### Why a JTAG Adapter is NOT a Generic Interchangeable Compute Board:
1. **Bus Mastership & Asymmetry**:
   A JTAG probe is a **hardware master** of the IEEE 1149.1 boundary-scan test bus. It directly clocks `TCK` and toggles `TMS` to navigate the 16-state TAP controller finite state machine inside the target chip. It does not execute user applications or exchange arbitrary network packets; its sole function is hardware instrumentation, register inspection, and flash/SRAM boundary bitstream programming.
2. **Voltage Incompatibility & Level Shifting**:
   Different silicon technologies operate on disparate I/O voltages (e.g. 1.0V core, 1.8V auxiliary, 3.3V peripherals). Connecting a 3.3V JTAG probe directly to a 1.8V FPGA bank will blow the input clamping diodes and cause permanent silicon latch-up. A compliant JTAG adapter must sample the target's `VTREF` pin and drive level-shifted buffers accordingly.
3. **Protocol Incompatibility**:
   JTAG is a low-level synchronous serial shift register protocol (TDI in, TDO out shifted on TCK). It does not implement framing, flow control, or error retransmission natively like USB, Ethernet, or UART. Therefore, boards cannot be assumed to share common connectors, voltages, or high-level communication stacks.

---

## 4. Software Mock Implementation & Fault Recovery Plan

### 4.1 Mock Adapter Architecture (`backend/include/workbench/embedded/`)
- `IDeviceAdapter`: Abstract base class declaring lifecycle methods (`discover`, `get_capability_report`, `connect`, `disconnect`, `poll_telemetry`, `inject_failure`, `recover`).
- `McuAdapter`: Simulates STM32F401 internal temperature sensor (42.5°C), 3.298V VDD rail, CPU utilization percentage, and packet sequence heartbeat.
- `FpgaAdapter`: Simulates Xilinx Artix-7 on-die XADC temperature (53.2°C), 1.002V VCCINT, 1.805V VCCAUX, 3.310V VCCO, and PLL lock register.
- `JtagAdapter`: Simulates TAP state machine (Run-Test/Idle), target VREF voltage sensing (3.298V), 10 MHz TCK frequency, and IDCODE retrieval (`0x4BA00477` / `0x0362D093`).

### 4.2 Recoverable Failure Demonstration (Timeout Sequence):
1. **Discovery and connection**: Each adapter begins in `DISCONNECTED`; discovery moves it to `DISCOVERED`. Connection is allowed only after discovery and moves it to `CONNECTED`.
2. **Normal state**: The UI polls connected adapters every 1200 ms. A successful sample moves the adapter to `STREAMING` and reports telemetry with status `STREAMING`.
3. **Fault injection**: `btn-fail` / `inject_failure("TIMEOUT")` is available only while connected. The deterministic simulation represents a UART, FIFO, or debug link that missed its 250 ms acknowledgement deadline; it does not sleep for 250 ms or access physical hardware.
4. **Error state**: The adapter enters `ERROR_TIMEOUT`, and the telemetry panel shows the simulated timeout and deadline.
5. **Safe recovery**: `Auto-Recover` is available only for an active timeout. The mock clears simulated buffers, resets the link handshake (or JTAG TAP), and returns to `CONNECTED`.
6. **Restoration**: The next successful telemetry poll transitions the adapter back to `STREAMING`. Since this is a mock, zero data corruption is a simulation invariant, not a measurement from a physical link.

### 4.3 Safe Integration Sequence and Prototype Limitations
- Verify the exact board and probe revisions, pin-one orientation, connector pinout, I/O-bank voltage, power source, and grounding guidance in the manufacturers' documentation.
- Power each target from its documented supply; do not treat a debug probe's `VTREF` sense input as a target power source. Confirm regulated rails and the planned ground reference before attaching signal cables.
- Select level translation for the actual signal direction, speed, and electrical load. Confirm the target voltage is within the adapter's supported `VTREF` range before enabling SWD/JTAG signaling.
- Connector families and signal roles in the table are design assumptions; numbered pin mappings must be confirmed against the documentation for the exact board and probe revisions.
- Attach the board-specific debug cable, confirm signal ownership, and bring up one host/debug link at a time. Check discovery, capability data, and a plausible telemetry sample before exercising recovery.
- The diagram is a reference architecture, not a wiring drawing or validated bill of materials. The mocks do not enumerate USB devices, measure rails or signal integrity, run firmware, enforce electrical limits, or program the FPGA. Physical integration must be checked against documentation for the exact hardware revision.

