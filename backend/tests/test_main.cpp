#include "workbench/fea/thermal_1d.hpp"
#include "workbench/fea/thermal_batch.hpp"
#include "workbench/cad/stl_parser.hpp"
#include "workbench/cad/ansys_parser.hpp"
#include "workbench/embedded/device_adapter.hpp"
#include "workbench/ml/backprop.hpp"
#include "workbench/multicore/thread_pool.hpp"
#include "workbench/common/json.hpp"

#include <iostream>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[-] FAILED: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUNNING] " << #fn << "... "; \
        if (fn()) { \
            std::cout << "PASSED\n"; \
            passed++; \
        } else { \
            failed++; \
        } \
        total++; \
    } while (0)

// --- Task C: Thermal FEA Tests ---

bool test_fea_positive_solve() {
    workbench::FEAConfig cfg;
    cfg.L = 1.0;
    cfg.A = 1e-4;
    cfg.k = 10.0;
    cfg.q = 10000.0;
    cfg.T0 = 100.0;
    cfg.TL = 20.0;
    cfg.num_elements = 10;

    auto res = workbench::fea::Thermal1DSolver::solve(cfg);
    TEST_ASSERT(res.success, "FEA solver should succeed for valid config");
    TEST_ASSERT(res.nodal_temperatures.size() == 11, "10 elements should yield 11 nodes");
    TEST_ASSERT(std::abs(res.nodal_temperatures[0] - 100.0) < 1e-12, "Left boundary condition T(0)=100 violated");
    TEST_ASSERT(std::abs(res.nodal_temperatures[10] - 20.0) < 1e-12, "Right boundary condition T(L)=20 violated");

    // Exact analytical match at nodes (superconvergence)
    TEST_ASSERT(res.max_nodal_error < 1e-10, "Nodal error should be near machine precision due to superconvergence");

    // Midpoint theoretical error for N=10: (q / (8*k)) * h^2 = (10000 / 80) * 0.01 = 1.25 C
    TEST_ASSERT(std::abs(res.max_midpoint_error - 1.25) < 1e-6, "N=10 midpoint error should be exactly 1.25 C");
    return true;
}

bool test_fea_convergence_rate() {
    workbench::FEAConfig cfg;
    auto conv = workbench::fea::Thermal1DSolver::run_convergence_study(cfg, {10, 20, 40, 80});

    TEST_ASSERT(conv.size() == 4, "Convergence study should have 4 entries");
    TEST_ASSERT(std::abs(conv[0].max_midpoint_error - 1.25000000) < 1e-5, "N=10 error mismatch");
    TEST_ASSERT(std::abs(conv[1].max_midpoint_error - 0.31250000) < 1e-5, "N=20 error mismatch");
    TEST_ASSERT(std::abs(conv[2].max_midpoint_error - 0.07812500) < 1e-5, "N=40 error mismatch");
    TEST_ASSERT(std::abs(conv[3].max_midpoint_error - 0.01953125) < 1e-5, "N=80 error mismatch");

    // Check convergence rate alpha ~= 2.0 (quadratic convergence)
    for (size_t i = 1; i < conv.size(); ++i) {
        TEST_ASSERT(std::abs(conv[i].convergence_rate - 2.0) < 0.05, "Convergence rate must be approximately 2.0");
    }
    return true;
}

bool test_fea_negative_heat_source_error_bound() {
    workbench::FEAConfig cfg;
    cfg.q = -10000.0;
    const auto conv = workbench::fea::Thermal1DSolver::run_convergence_study(cfg, {10, 20, 40, 80});
    TEST_ASSERT(conv.size() == 4, "Negative heat source should still produce a convergence study");
    TEST_ASSERT(std::abs(conv[0].theoretical_error - 1.25) < 1e-12,
                "Theoretical error bound must be non-negative for a heat sink");
    TEST_ASSERT(std::abs(conv[1].convergence_rate - 2.0) < 1e-10,
                "Negative heat source should retain second-order convergence");
    return true;
}

bool test_fea_csv_includes_convergence_data() {
    workbench::FEAConfig cfg;
    const auto solution = workbench::fea::Thermal1DSolver::solve(cfg);
    const auto convergence = workbench::fea::Thermal1DSolver::run_convergence_study(cfg, {10, 20, 40, 80});
    const auto csv = workbench::fea::Thermal1DSolver::export_csv(solution, convergence);
    TEST_ASSERT(csv.find("# mesh_profile") != std::string::npos,
                "FEA CSV should identify the selected mesh profile section");
    TEST_ASSERT(csv.find("# mesh_convergence") != std::string::npos,
                "FEA CSV should include a mesh convergence section");
    TEST_ASSERT(csv.find("80,0.0125,0.01953125,0.01953125,2") != std::string::npos,
                "FEA CSV should export the N=80 convergence data");
    return true;
}

bool test_fea_negative_validation() {
    workbench::FEAConfig cfg;

    cfg.L = -1.0;
    auto res = workbench::fea::Thermal1DSolver::solve(cfg);
    TEST_ASSERT(!res.success && !res.error_message.empty(), "Should reject negative length");

    cfg.L = 1.0;
    cfg.A = 0.0;
    res = workbench::fea::Thermal1DSolver::solve(cfg);
    TEST_ASSERT(!res.success && !res.error_message.empty(), "Should reject zero area");

    cfg.A = 1e-4;
    cfg.k = -5.0;
    res = workbench::fea::Thermal1DSolver::solve(cfg);
    TEST_ASSERT(!res.success && !res.error_message.empty(), "Should reject negative conductivity");

    cfg.k = 10.0;
    cfg.num_elements = 0;
    res = workbench::fea::Thermal1DSolver::solve(cfg);
    TEST_ASSERT(!res.success && !res.error_message.empty(), "Should reject N < 1");

    cfg.num_elements = 10;
    cfg.T0 = std::numeric_limits<double>::quiet_NaN();
    res = workbench::fea::Thermal1DSolver::solve(cfg);
    TEST_ASSERT(!res.success && res.error_message.find("finite") != std::string::npos,
                "Should reject non-finite boundary temperatures");
    return true;
}

// --- Task B: CAD & ANSYS Parsers Tests ---

bool test_stl_parser_positive() {
    std::string sample_stl =
        "solid test_triangle\n"
        "  facet normal 0.0 0.0 1.0\n"
        "    outer loop\n"
        "      vertex 0.0 0.0 0.0\n"
        "      vertex 1.0 0.0 0.0\n"
        "      vertex 0.0 1.0 0.0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid test_triangle\n";

    auto model = workbench::cad::STLParser::parse_string(sample_stl);
    TEST_ASSERT(model.is_valid, "Valid STL string should parse successfully");
    TEST_ASSERT(model.triangles.size() == 1, "Should have 1 triangle");
    TEST_ASSERT(model.solid_name == "test_triangle", "Solid name mismatch");
    TEST_ASSERT(model.bbox_min.x == 0.0 && model.bbox_max.x == 1.0, "Bounding box x mismatch");

    std::string binary_stl(84 + 50, '\0');
    const char header[] = "binary_test";
    std::copy(header, header + sizeof(header) - 1, binary_stl.begin());
    binary_stl[80] = 1; // One little-endian triangle record.
    auto write_float = [&binary_stl](size_t offset, float value) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        for (size_t byte = 0; byte < 4; ++byte) {
            binary_stl[offset + byte] = static_cast<char>((bits >> (byte * 8)) & 0xff);
        }
    };
    const float binary_values[] = {0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0};
    for (size_t i = 0; i < 12; ++i) write_float(84 + i * 4, binary_values[i]);
    const auto binary_model = workbench::cad::STLParser::parse_string(binary_stl, "binary_test.stl");
    TEST_ASSERT(binary_model.is_valid && binary_model.triangles.size() == 1,
                "C++ parser should accept a valid binary STL triangle");
    TEST_ASSERT(binary_model.dimensions().x == 1.0 && binary_model.dimensions().y == 1.0,
                "Binary STL dimensions should be computed from its vertices");
    return true;
}

bool test_json_string_upload_decode() {
    const std::string stl =
        "solid upload_case\n"
        "  # quoted note: \"sample\" at path\\mesh\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid upload_case\n";
    const std::string stl_request = "{\"raw_content\":\"" + workbench::json::escape_string(stl) + "\"}";
    const std::string decoded_stl = workbench::json::get_string_field(stl_request, "raw_content");
    TEST_ASSERT(decoded_stl == stl, "JSON string decoding must restore quotes, backslashes, and newlines");
    const auto model = workbench::cad::STLParser::parse_string(decoded_stl, "upload");
    TEST_ASSERT(model.is_valid && model.triangles.size() == 1, "Escaped STL upload content must parse");

    const std::string csv =
        "node_id,x_m,y_m,z_m,temperature_C\r\n"
        "1,0,0,0,42.5\r\n"
        "2,1,0,0,38.0\r\n";
    const std::string csv_request = "{\"raw_content\":\"" + workbench::json::escape_string(csv) + "\"}";
    const std::string decoded_csv = workbench::json::get_string_field(csv_request, "raw_content");
    TEST_ASSERT(decoded_csv == csv, "JSON string decoding must preserve CRLF CSV records");
    const auto dataset = workbench::cad::ANSYSParser::parse_string(decoded_csv, "upload");
    TEST_ASSERT(dataset.is_valid && dataset.nodes.size() == 2, "Escaped CSV upload content must parse");

    const std::string unicode = workbench::json::get_string_field("{\"label\":\"\\u03bc\"}", "label");
    TEST_ASSERT(unicode == "μ", "JSON Unicode escapes must decode to UTF-8");
    return true;
}

bool test_stl_parser_negative() {
    // Empty
    auto model1 = workbench::cad::STLParser::parse_string("");
    TEST_ASSERT(!model1.is_valid && !model1.error_message.empty(), "Empty STL should be rejected");

    const std::string oversized_stl(workbench::cad::STLParser::MAX_TESTED_FILE_SIZE_BYTES + 1, 'x');
    const auto oversized_model = workbench::cad::STLParser::parse_string(oversized_stl);
    TEST_ASSERT(!oversized_model.is_valid && oversized_model.error_message.find("150 MiB") != std::string::npos,
                "STL parser should reject input above the documented 150 MiB cap");

    // Corrupted coordinates (NaN)
    std::string nan_stl =
        "solid nan_test\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex NaN 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid nan_test\n";
    auto model2 = workbench::cad::STLParser::parse_string(nan_stl);
    TEST_ASSERT(!model2.is_valid, "NaN vertex coordinates must be rejected");

    // Incomplete facet
    std::string incomplete_stl = "solid bad\n  facet normal 0 0 1\n    outer loop\n      vertex 0 0 0\n";
    auto model3 = workbench::cad::STLParser::parse_string(incomplete_stl);
    TEST_ASSERT(!model3.is_valid, "Premature EOF must be rejected");

    const std::string missing_endsolid_stl =
        "solid missing_terminator\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n";
    const auto model4 = workbench::cad::STLParser::parse_string(missing_endsolid_stl);
    TEST_ASSERT(!model4.is_valid && model4.error_message.find("endsolid") != std::string::npos,
                "ASCII STL without its endsolid terminator must be rejected");

    std::string binary_nan_stl(84 + 50, '\0');
    binary_nan_stl[80] = 1;
    const float binary_values[] = {0, 0, 1, std::numeric_limits<float>::quiet_NaN(), 0, 0,
                                   1, 0, 0, 0, 1, 0};
    for (size_t i = 0; i < 12; ++i) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &binary_values[i], sizeof(bits));
        for (size_t byte = 0; byte < 4; ++byte) {
            binary_nan_stl[84 + i * 4 + byte] = static_cast<char>((bits >> (byte * 8)) & 0xff);
        }
    }
    const auto model5 = workbench::cad::STLParser::parse_string(binary_nan_stl, "binary_nan.stl");
    TEST_ASSERT(!model5.is_valid && model5.error_message.find("non-finite") != std::string::npos,
                "C++ parser should reject non-finite binary STL values");
    return true;
}

bool test_ansys_parser_positive() {
    std::string csv =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,100.0\n"
        "2,0.1,0.0,0.0,92.5\n"
        "3,0.2,0.0,0.0,85.0\n";

    auto ds = workbench::cad::ANSYSParser::parse_string(csv);
    TEST_ASSERT(ds.is_valid, "Valid ANSYS CSV must parse successfully");
    TEST_ASSERT(ds.nodes.size() == 3, "Node count should be 3");
    TEST_ASSERT(ds.min_temperature == 85.0, "Min temp mismatch");
    TEST_ASSERT(ds.max_temperature == 100.0, "Max temp mismatch");
    TEST_ASSERT(std::abs(ds.mean_temperature - 92.5) < 1e-8, "Mean temp mismatch");
    return true;
}

bool test_ansys_parser_negative() {
    // Empty CSV
    auto ds1 = workbench::cad::ANSYSParser::parse_string("");
    TEST_ASSERT(!ds1.is_valid && ds1.error_message.find("empty") != std::string::npos,
                "Empty CSV must be rejected with an explanatory message");

    // Missing header column
    auto ds2 = workbench::cad::ANSYSParser::parse_string("node_id,x_m,y_m\n1,0,0\n");
    TEST_ASSERT(!ds2.is_valid, "Incomplete header columns must be rejected");

    // Duplicate node_id
    std::string dup_csv =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "10,0.0,0.0,0.0,50.0\n"
        "10,0.1,0.1,0.1,60.0\n";
    auto ds3 = workbench::cad::ANSYSParser::parse_string(dup_csv);
    TEST_ASSERT(!ds3.is_valid && ds3.error_message.find("Duplicate") != std::string::npos,
                "Duplicate node_id must be rejected");

    auto ds4 = workbench::cad::ANSYSParser::parse_string(
        "node_id,x_m,y_m,z_m,temperature_C\n0,0,0,0,20\n");
    TEST_ASSERT(!ds4.is_valid && ds4.error_message.find("positive integer") != std::string::npos,
                "Zero node IDs must be rejected");

    auto ds5 = workbench::cad::ANSYSParser::parse_string(
        "node_id,x_m,y_m,z_m,temperature_C\n1,NaN,0,0,20\n");
    TEST_ASSERT(!ds5.is_valid && ds5.error_message.find("Non-finite") != std::string::npos,
                "Non-finite coordinates must be rejected");

    std::string oversized(workbench::cad::ANSYSParser::MAX_SUPPORTED_FILE_SIZE_BYTES + 1, '#');
    auto ds6 = workbench::cad::ANSYSParser::parse_string(oversized);
    TEST_ASSERT(!ds6.is_valid && ds6.error_message.find("50 MiB") != std::string::npos,
                "CSV input above the documented 50 MiB limit must be rejected");
    return true;
}

// --- Task D: Multicore & Agreement Tests ---

bool test_thread_pool_and_batch_solver() {
    int num_cases = 500;
    int mesh_size = 50;
    uint64_t seed = 42;

    auto cases = workbench::fea::BatchThermalSolver::generate_batch(num_cases, mesh_size, seed);
    TEST_ASSERT(cases.size() == static_cast<size_t>(num_cases), "Batch size mismatch");
    const auto repeated_cases = workbench::fea::BatchThermalSolver::generate_batch(num_cases, mesh_size, seed);
    TEST_ASSERT(repeated_cases.size() == cases.size(), "Repeated seeded batch size mismatch");
    for (size_t i = 0; i < cases.size(); ++i) {
        TEST_ASSERT(cases[i].k == repeated_cases[i].k && cases[i].q == repeated_cases[i].q &&
                    cases[i].T0 == repeated_cases[i].T0 && cases[i].TL == repeated_cases[i].TL,
                    "Same seed must reproduce every thermal case");
    }

    auto serial_res = workbench::fea::BatchThermalSolver::solve_serial(cases);
    auto parallel_res_2 = workbench::fea::BatchThermalSolver::solve_parallel(cases, 2);
    auto parallel_res_4 = workbench::fea::BatchThermalSolver::solve_parallel(cases, 4);

    TEST_ASSERT(serial_res.size() == parallel_res_2.size(), "Result size mismatch");

    // Serial vs parallel agreement check: max absolute difference <= 1e-8 C
    double diff2 = workbench::fea::BatchThermalSolver::verify_agreement(serial_res, parallel_res_2);
    double diff4 = workbench::fea::BatchThermalSolver::verify_agreement(serial_res, parallel_res_4);

    TEST_ASSERT(diff2 <= 1e-8, "Serial vs 2-worker parallel difference must be <= 1e-8 C");
    TEST_ASSERT(diff4 <= 1e-8, "Serial vs 4-worker parallel difference must be <= 1e-8 C");

    // Deterministic input ordering check: first and last cases
    TEST_ASSERT(std::abs(serial_res[0].nodal_temperatures[10] - parallel_res_4[0].nodal_temperatures[10]) < 1e-12,
                "Case 0 deterministic ordering mismatch");
    TEST_ASSERT(std::abs(serial_res.back().nodal_temperatures[10] - parallel_res_4.back().nodal_temperatures[10]) < 1e-12,
                "Final case deterministic ordering mismatch");
    return true;
}

bool test_benchmark_metadata_and_median() {
    workbench::BenchmarkConfig cfg;
    cfg.num_cases = 20;
    cfg.mesh_size = 10;
    cfg.seed = 7;
    cfg.worker_counts = {1, 2};
    cfg.repetitions = 4;
    cfg.warm_up = true;

    const auto result = workbench::fea::BatchThermalSolver::run_benchmark(cfg);
    TEST_ASSERT(result.total_memory_bytes > 0, "Total physical RAM should be reported");
    TEST_ASSERT(result.optimization_flags == "-O3" || result.optimization_flags == "/O2",
                "Compiler optimization flags should match the supported build configuration");
    TEST_ASSERT(result.verification_passed, "Parallel validation path should agree with serial results");
    TEST_ASSERT(result.verification_workers >= 2, "Parallel verification should identify its worker count");
    TEST_ASSERT(result.speedup_assessable == (result.measurements.size() > 1),
                "Speedup availability must reflect the measured worker counts");
    TEST_ASSERT(result.measurements[0].run_times_ms.size() == 4,
                "Benchmark should record the configured repetition count");

    auto sorted_times = result.measurements[0].run_times_ms;
    std::sort(sorted_times.begin(), sorted_times.end());
    const double expected_median = (sorted_times[1] + sorted_times[2]) / 2.0;
    TEST_ASSERT(std::abs(result.measurements[0].median_time_ms - expected_median) < 1e-12,
                "Even repetition counts should average the two middle timings");

    const std::string serialized = workbench::json::serialize_benchmark_result(result);
    TEST_ASSERT(serialized.find("\"total_memory_bytes\"") != std::string::npos,
                "Benchmark JSON should include RAM");
    TEST_ASSERT(serialized.find("\"warm_up_per_worker\": true") != std::string::npos,
                "Benchmark JSON should report the warm-up policy");
    TEST_ASSERT(serialized.find("\"speedup_assessable\"") != std::string::npos,
                "Benchmark JSON should report when speedup is assessable");

    cfg.worker_counts = {1};
    cfg.repetitions = 2;
    cfg.warm_up = false;
    const auto serial_only = workbench::fea::BatchThermalSolver::run_benchmark(cfg);
    TEST_ASSERT(serial_only.measurements.size() == 1, "A serial-only run should time only one worker");
    TEST_ASSERT(!serial_only.speedup_assessable, "Speedup must be unavailable without timed parallel workers");
    TEST_ASSERT(serial_only.verification_workers == 2 && serial_only.verification_passed,
                "A serial-only run must still exercise and verify the 2-worker path");
    return true;
}

// --- Task E: Embedded Integration Tests ---

bool test_embedded_mock_adapters() {
    workbench::embedded::DeviceManager dm;
    auto devices = dm.get_all_devices();
    TEST_ASSERT(devices.size() == 3, "Expected 3 devices: MCU, FPGA, JTAG");

    for (auto* dev : devices) {
        TEST_ASSERT(dev->state() == workbench::embedded::DeviceState::DISCONNECTED,
                    "Adapters must start disconnected");
        TEST_ASSERT(!dev->connect(), "A device must be discovered before it can connect");
        TEST_ASSERT(!dev->recover(), "Recovery must be rejected when no timeout is active");
        TEST_ASSERT(dev->discover(), "Device discovery must succeed");
        TEST_ASSERT(!dev->is_connected(), "Discovery must not connect a device");
        TEST_ASSERT(dev->state() == workbench::embedded::DeviceState::DISCOVERED,
                    "Discovered device must expose DISCOVERED state");
        TEST_ASSERT(!dev->get_capability_report().empty(), "Capability report must not be empty");

        // Connect
        TEST_ASSERT(dev->connect(), "Connect must succeed");
        TEST_ASSERT(dev->is_connected(), "Device state must be CONNECTED");

        // Telemetry
        std::string tel = dev->poll_telemetry();
        TEST_ASSERT(tel.find("\"status\": \"STREAMING\"") != std::string::npos,
                    "Telemetry status must match STREAMING device state");

        // Recoverable failure demonstration: inject timeout
        TEST_ASSERT(dev->inject_failure("TIMEOUT"), "Timeout injection must be accepted while connected");
        TEST_ASSERT(dev->state() == workbench::embedded::DeviceState::ERROR_TIMEOUT,
                    "State must transition to ERROR_TIMEOUT after failure injection");
        std::string err_tel = dev->poll_telemetry();
        TEST_ASSERT(err_tel.find("ERROR_TIMEOUT") != std::string::npos, "Telemetry must report error after timeout");

        // Recovery
        TEST_ASSERT(dev->recover(), "Recovery routine must succeed");
        TEST_ASSERT(dev->state() == workbench::embedded::DeviceState::CONNECTED,
                    "Device must return to CONNECTED after recovery");

        // Disconnect
        TEST_ASSERT(dev->disconnect(), "Disconnect must succeed");
        TEST_ASSERT(!dev->is_connected(), "Device must be offline after disconnect");
        TEST_ASSERT(dev->state() == workbench::embedded::DeviceState::DISCOVERED,
                    "Disconnect must preserve discovered-device state");
        TEST_ASSERT(!dev->inject_failure("TIMEOUT"), "Timeout injection must be rejected while disconnected");
        TEST_ASSERT(!dev->recover(), "Recovery must be rejected when no timeout is active");
    }
    return true;
}

// --- Task F: Backpropagation Tests ---

bool test_backpropagation_analytical_and_fd() {
    auto res = workbench::ml::BackpropagationLab::run_verification_and_training(
        1.0, 2.0, 0.5, 0.0, 1.0, 0.0, 0.1, 100
    );

    // Initial forward check
    // z = 0.5, h = tanh(0.5) ~= 0.46211716, y_hat = 0.46211716, loss = 0.5 * (y_hat - 2)^2 ~= 1.18254182
    TEST_ASSERT(std::abs(res.step0_z - 0.5) < 1e-12, "Step 0 z mismatch");
    TEST_ASSERT(std::abs(res.step0_h - std::tanh(0.5)) < 1e-12, "Step 0 h mismatch");
    TEST_ASSERT(std::abs(res.step0_loss - 1.182541819) < 1e-6, "Step 0 loss mismatch");

    // Analytical gradient check
    TEST_ASSERT(std::abs(res.grad_w - (-1.20946448)) < 1e-5, "Analytical dJ/dw mismatch");
    TEST_ASSERT(std::abs(res.grad_b - (-1.20946448)) < 1e-5, "Analytical dJ/db mismatch");
    TEST_ASSERT(std::abs(res.grad_v - (-0.71068205)) < 1e-5, "Analytical dJ/dv mismatch");
    TEST_ASSERT(std::abs(res.grad_c - (-1.53788284)) < 1e-5, "Analytical dJ/dc mismatch");

    // Central finite difference verification check (tolerance <= 1e-5)
    TEST_ASSERT(res.fd_err_w <= 1e-5, "Finite difference check for w failed");
    TEST_ASSERT(res.fd_err_b <= 1e-5, "Finite difference check for b failed");
    TEST_ASSERT(res.fd_err_v <= 1e-5, "Finite difference check for v failed");
    TEST_ASSERT(res.fd_err_c <= 1e-5, "Finite difference check for c failed");
    TEST_ASSERT(res.fd_check_passed, "Overall finite difference verification failed");

    // Step 1 update check
    TEST_ASSERT(std::abs(res.w_step1 - 0.62094645) < 1e-5, "Step 1 w mismatch");
    TEST_ASSERT(std::abs(res.b_step1 - 0.12094645) < 1e-5, "Step 1 b mismatch");
    TEST_ASSERT(std::abs(res.v_step1 - 1.07106820) < 1e-5, "Step 1 v mismatch");
    TEST_ASSERT(std::abs(res.c_step1 - 0.15378828) < 1e-5, "Step 1 c mismatch");

    // 100-step training loss monotonicity check
    TEST_ASSERT(res.history.size() == 101, "Expected 101 steps (0 to 100)");
    TEST_ASSERT(res.history.back().loss < res.history.front().loss, "Loss must decrease over 100 steps");
    TEST_ASSERT(res.history.back().loss < 0.05, "Final loss after 100 steps should be well converged (< 0.05)");
    return true;
}

int main() {
    std::cout << "========================================================\n"
              << "      UNIFIED ENGINEERING WORKBENCH - TEST SUITE        \n"
              << "========================================================\n";

    int passed = 0;
    int failed = 0;
    int total = 0;

    RUN_TEST(test_fea_positive_solve);
    RUN_TEST(test_fea_convergence_rate);
    RUN_TEST(test_fea_negative_heat_source_error_bound);
    RUN_TEST(test_fea_csv_includes_convergence_data);
    RUN_TEST(test_fea_negative_validation);
    RUN_TEST(test_stl_parser_positive);
    RUN_TEST(test_json_string_upload_decode);
    RUN_TEST(test_stl_parser_negative);
    RUN_TEST(test_ansys_parser_positive);
    RUN_TEST(test_ansys_parser_negative);
    RUN_TEST(test_thread_pool_and_batch_solver);
    RUN_TEST(test_benchmark_metadata_and_median);
    RUN_TEST(test_embedded_mock_adapters);
    RUN_TEST(test_backpropagation_analytical_and_fd);

    std::cout << "========================================================\n";
    std::cout << "Test Summary: " << passed << "/" << total << " tests passed";
    if (failed > 0) {
        std::cout << " (" << failed << " FAILED)\n";
        return 1;
    }
    std::cout << " (ALL TESTS PASSED 100%)\n";
    std::cout << "========================================================\n";
    return 0;
}

