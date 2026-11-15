#include "workbench/server/http_server.hpp"
#include "workbench/fea/thermal_1d.hpp"
#include "workbench/fea/thermal_batch.hpp"
#include "workbench/ml/backprop.hpp"
#include "workbench/cad/stl_parser.hpp"
#include "workbench/cad/ansys_parser.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <csignal>
#include <atomic>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

void open_browser(const std::string& url) {
#if defined(_WIN32)
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string cmd = "open " + url;
    std::system(cmd.c_str());
#else
    std::string cmd = "xdg-open " + url + " > /dev/null 2>&1 &";
    std::system(cmd.c_str());
#endif
}

void print_banner() {
    std::cout << "========================================================\n"
              << "             UNIFIED ENGINEERING WORKBENCH              \n"
              << "========================================================\n"
              << "  [Task A] Web Interface, C++ Core & One-Click Execution\n"
              << "  [Task B] CAD (STL) & ANSYS 3D Data Visualization\n"
              << "  [Task C] 1D Steady-State Mechanical Thermal FEA Solver\n"
              << "  [Task D] Multicore Batch Worker Pool & Scaling Benchmark\n"
              << "  [Task E] Embedded System Workbench (MCU, FPGA, JTAG)\n"
              << "  [Task F] Analytical Backpropagation & Gradient Check\n"
              << "========================================================\n";
}

void save_file(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (out.is_open()) {
        out << content;
        std::cout << "[Export] Saved: " << path << "\n";
    } else {
        std::cerr << "[Export Error] Could not write to: " << path << "\n";
    }
}

static std::atomic<bool> g_terminated{false};
extern "C" void handle_sigterm(int) {
    g_terminated = true;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handle_sigterm);
    std::signal(SIGTERM, handle_sigterm);

    const auto t_start = std::chrono::steady_clock::now();
    print_banner();

    int port = 8080;
    bool headless = false;
    bool auto_open = true;

    // Detect environment variables for cloud deployment (e.g. Render, Railway, Fly.io, Docker)
    const char* env_port = std::getenv("PORT");
    if (env_port) {
        try {
            port = std::stoi(env_port);
            auto_open = false;
        } catch (...) {}
    }
    if (std::getenv("RENDER") || std::getenv("CONTAINER") || std::getenv("DOCKER")) {
        auto_open = false;
    }

    bool run_benchmarks = false;
    bool run_fea = false;
    bool run_backprop = false;
    std::string web_root = ".";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--no-browser") {
            auto_open = false;
        } else if (arg == "--headless") {
            headless = true;
            auto_open = false;
        } else if (arg == "--run-benchmarks") {
            run_benchmarks = true;
            headless = true;
        } else if (arg == "--run-fea") {
            run_fea = true;
            headless = true;
        } else if (arg == "--run-backprop") {
            run_backprop = true;
            headless = true;
        } else if (arg == "--run-all-headless") {
            run_benchmarks = true;
            run_fea = true;
            run_backprop = true;
            headless = true;
        } else if (arg == "--web-root" && i + 1 < argc) {
            web_root = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: workbench_server [options]\n"
                      << "Options:\n"
                      << "  --port <port>         HTTP port (default: 8080)\n"
                      << "  --no-browser          Do not automatically open default browser\n"
                      << "  --headless            Run in headless mode without UI server\n"
                      << "  --run-fea             Run 1D thermal FEA convergence & export CSV\n"
                      << "  --run-benchmarks      Run multicore CPU benchmark & export CSV\n"
                      << "  --run-backprop        Run backpropagation test & export CSV\n"
                      << "  --run-all-headless    Run all computational tasks and export results\n"
                      << "  --web-root <path>     Directory containing frontend static files\n"
                      << "  --help                Show this help message\n";
            return 0;
        }
    }

    // Execute headless computation and export if requested
    if (run_fea) {
        std::cout << "\n[FEA] Running 1D Thermal FEA Convergence Study (N = 10, 20, 40, 80)...\n";
        workbench::FEAConfig cfg;
        auto res = workbench::fea::Thermal1DSolver::solve(cfg);
        auto conv = workbench::fea::Thermal1DSolver::run_convergence_study(cfg, {10, 20, 40, 80});

        std::cout << std::left << std::setw(6) << "N"
                  << std::setw(12) << "h (m)"
                  << std::setw(22) << "Max Midpoint Error (C)"
                  << std::setw(22) << "Theoretical Error (C)"
                  << std::setw(16) << "Convergence Rate" << "\n";
        std::cout << std::string(78, '-') << "\n";

        for (const auto& c : conv) {
            std::cout << std::left << std::setw(6) << c.N
                      << std::setw(12) << c.h
                      << std::setw(22) << c.max_midpoint_error
                      << std::setw(22) << c.theoretical_error
                      << std::setw(16) << c.convergence_rate << "\n";
        }
        std::cout << "Max nodal error: " << res.max_nodal_error << " C (verifying superconvergence at nodes)\n";

        std::string csv_data = workbench::fea::Thermal1DSolver::export_csv(res, conv);
        save_file("results/fea_convergence.csv", csv_data);
    }

    if (run_benchmarks) {
        std::cout << "\n[Benchmark] Running Multicore Parallel Thermal Solver Benchmark...\n";
        workbench::BenchmarkConfig bcfg;
        bcfg.num_cases = 20000;
        bcfg.mesh_size = 200;
        bcfg.seed = 42;
        bcfg.repetitions = 5;

        auto bres = workbench::fea::BatchThermalSolver::run_benchmark(bcfg);
        std::cout << "Hardware: " << bres.cpu_model << " | " << bres.physical_cpus << " Physical / "
                  << bres.logical_cpus << " Logical CPUs | RAM: " << bres.total_memory_bytes << " bytes\n"
                  << "Compiler: " << bres.compiler_info << " | Optimization Flags: " << bres.optimization_flags << "\n"
                  << "Warm-up: " << (bcfg.warm_up ? "one untimed pass per measured worker count" : "disabled") << "\n"
                  << "Max Serial/Parallel Diff: " << bres.serial_parallel_max_diff << " C (<= 1e-8 check: "
                  << (bres.verification_passed ? "PASSED" : "FAILED") << ")\n";
        if (!bres.speedup_assessable) {
            std::cout << "Speedup cannot be assessed with one available logical CPU; parallel path checked with "
                      << bres.verification_workers << " workers.\n";
        }
        std::cout << "\n";

        std::cout << std::left << std::setw(10) << "Workers"
                  << std::setw(18) << "Median Time (ms)"
                  << std::setw(16) << "Speedup (T1/Tp)"
                  << std::setw(16) << "Efficiency" << "\n";
        std::cout << std::string(60, '-') << "\n";

        for (const auto& m : bres.measurements) {
            std::cout << std::left << std::setw(10) << m.workers
                      << std::setw(18) << m.median_time_ms
                      << std::setw(16) << (bres.speedup_assessable ? std::to_string(m.speedup) : "N/A")
                      << std::setw(16) << (bres.speedup_assessable ? std::to_string(m.efficiency) : "N/A") << "\n";
        }

        std::string b_csv = workbench::fea::BatchThermalSolver::export_benchmark_csv(bres);
        save_file("results/benchmark_serial_parallel.csv", b_csv);
    }

    if (run_backprop) {
        std::cout << "\n[Backprop] Running Analytical Backpropagation & Central Finite Difference Check...\n";
        auto bpres = workbench::ml::BackpropagationLab::run_verification_and_training();

        std::cout << "Initial Loss: " << bpres.step0_loss << "\n";
        std::cout << "Gradients: dJ/dw=" << bpres.grad_w
                  << ", dJ/db=" << bpres.grad_b
                  << ", dJ/dv=" << bpres.grad_v
                  << ", dJ/dc=" << bpres.grad_c << "\n";
        std::cout << "Finite Difference Errors (eps=1e-6, tol=1e-5): "
                  << "w=" << bpres.fd_err_w << ", b=" << bpres.fd_err_b
                  << ", v=" << bpres.fd_err_v << ", c=" << bpres.fd_err_c
                  << " -> " << (bpres.fd_check_passed ? "PASSED" : "FAILED") << "\n";
        std::cout << "Step 1 Updated Weights: w1=" << bpres.w_step1
                  << ", b1=" << bpres.b_step1
                  << ", v1=" << bpres.v_step1
                  << ", c1=" << bpres.c_step1 << "\n";
        std::cout << "Final Step 100 Loss: " << bpres.history.back().loss << "\n";

        std::string bp_csv = workbench::ml::BackpropagationLab::export_csv(bpres);
        save_file("results/backprop_training_loss.csv", bp_csv);
    }

    if (headless) {
        std::cout << "\n[Headless] All requested headless tasks finished successfully.\n";
        return 0;
    }

    // Start UI HTTP Server
    workbench::server::HttpServer server(port, web_root);
    if (!server.start()) {
        std::cerr << "Failed to start HTTP server on port " << port << "\n";
        return 1;
    }

    const auto t_ready = std::chrono::steady_clock::now();
    double launch_ms = std::chrono::duration<double, std::milli>(t_ready - t_start).count();
    server.set_launch_to_ready_ms(launch_ms);
    std::cout << "[Launch] Workbench core ready in " << std::fixed << std::setprecision(2) << launch_ms << " ms.\n";
    std::cout << "[Workbench] Open " << server.get_url() << " in your browser to access the workbench.\n";
    std::cout << "[Workbench] Press Ctrl+C or enter 'q' to stop server.\n";

    if (auto_open) {
        open_browser(server.get_url());
    }

    // Keep running until user enters 'q' or process receives termination signal
    std::string line;
    while (!g_terminated.load() && std::getline(std::cin, line)) {
        if (line == "q" || line == "quit" || line == "exit") {
            break;
        }
    }

    // In non-interactive environments (e.g. Docker, Render web service), std::cin hits EOF immediately
    if (std::cin.eof() && !g_terminated.load()) {
        std::cout << "[Workbench] Non-interactive environment detected. Keeping server daemon active...\n";
        while (!g_terminated.load() && server.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    std::cout << "[Workbench] Shutting down server gracefully...\n";
    server.stop();
    return 0;
}

