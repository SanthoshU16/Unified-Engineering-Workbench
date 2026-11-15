#include "workbench/fea/thermal_batch.hpp"
#include "workbench/fea/thermal_1d.hpp"
#include "workbench/multicore/thread_pool.hpp"
#include "workbench/common/system_info.hpp"
#include <random>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace workbench {
namespace fea {

std::vector<FEAConfig> BatchThermalSolver::generate_batch(int num_cases, int mesh_size, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist_k(1.0, 50.0);       // W/(m K)
    std::uniform_real_distribution<double> dist_q(1000.0, 50000.0); // W/m^3
    std::uniform_real_distribution<double> dist_T0(50.0, 150.0);    // C
    std::uniform_real_distribution<double> dist_TL(10.0, 45.0);     // C

    std::vector<FEAConfig> cases;
    cases.reserve(num_cases);

    for (int i = 0; i < num_cases; ++i) {
        FEAConfig cfg;
        cfg.L = 1.0;
        cfg.A = 1e-4;
        cfg.k = dist_k(rng);
        cfg.q = dist_q(rng);
        cfg.T0 = dist_T0(rng);
        cfg.TL = dist_TL(rng);
        cfg.num_elements = mesh_size;
        cases.push_back(cfg);
    }

    return cases;
}

std::vector<FEAResult> BatchThermalSolver::solve_serial(const std::vector<FEAConfig>& cases) {
    std::vector<FEAResult> results(cases.size());
    for (size_t i = 0; i < cases.size(); ++i) {
        results[i] = Thermal1DSolver::solve(cases[i], false);
    }
    return results;
}

std::vector<FEAResult> BatchThermalSolver::solve_parallel(const std::vector<FEAConfig>& cases, int num_workers) {
    if (num_workers <= 1) {
        return solve_serial(cases);
    }

    std::vector<FEAResult> results(cases.size());
    multicore::ThreadPool pool(num_workers);

    // Chunk size heuristic: balanced chunking to reduce synchronization overhead
    const size_t total_cases = cases.size();
    const size_t chunk_size = std::max<size_t>(1, total_cases / (num_workers * 8));

    for (size_t start = 0; start < total_cases; start += chunk_size) {
        size_t end = std::min(start + chunk_size, total_cases);
        pool.enqueue([&cases, &results, start, end]() {
            for (size_t i = start; i < end; ++i) {
                results[i] = Thermal1DSolver::solve(cases[i], false);
            }
        });
    }

    pool.wait_all();
    return results;
}

double BatchThermalSolver::verify_agreement(
    const std::vector<FEAResult>& serial_res,
    const std::vector<FEAResult>& parallel_res)
{
    if (serial_res.size() != parallel_res.size()) {
        return std::numeric_limits<double>::infinity();
    }

    double max_diff = 0.0;
    for (size_t i = 0; i < serial_res.size(); ++i) {
        const auto& s = serial_res[i];
        const auto& p = parallel_res[i];
        if (s.nodal_temperatures.size() != p.nodal_temperatures.size()) {
            return std::numeric_limits<double>::infinity();
        }
        for (size_t j = 0; j < s.nodal_temperatures.size(); ++j) {
            double diff = std::abs(s.nodal_temperatures[j] - p.nodal_temperatures[j]);
            if (diff > max_diff) {
                max_diff = diff;
            }
        }
    }
    return max_diff;
}

BenchmarkResult BatchThermalSolver::run_benchmark(const BenchmarkConfig& cfg) {
    BenchmarkResult res;
    res.config = cfg;
    res.logical_cpus = system_info::logical_cpu_count();
    res.physical_cpus = system_info::physical_cpu_count();
    res.total_memory_bytes = system_info::total_memory_bytes();
    res.cpu_model = system_info::cpu_model();

#if defined(__clang__)
    res.compiler_info = "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__GNUC__)
    res.compiler_info = "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#elif defined(_MSC_VER)
    res.compiler_info = "MSVC " + std::to_string(_MSC_VER);
#else
    res.compiler_info = "C++17 Compliant Compiler";
#endif

#if defined(_MSC_VER)
    res.optimization_flags = "/O2";
#elif defined(__clang__) || defined(__GNUC__)
    res.optimization_flags = "-O3";
#else
    res.optimization_flags = "Compiler default (unknown)";
#endif

    // Generate batch with fixed seed
    auto cases = generate_batch(cfg.num_cases, cfg.mesh_size, cfg.seed);

    // Determine tested worker counts capped at available logical CPUs
    std::vector<int> tested_workers;
    for (int w : cfg.worker_counts) {
        if (w > 0 && w <= res.logical_cpus &&
            std::find(tested_workers.begin(), tested_workers.end(), w) == tested_workers.end()) {
            tested_workers.push_back(w);
        }
    }
    if (tested_workers.empty()) {
        tested_workers.push_back(1);
    }
    std::sort(tested_workers.begin(), tested_workers.end());
    res.speedup_assessable = tested_workers.size() > 1;

    // Warm up each measured execution path once; none of these passes are timed or reported.
    if (cfg.warm_up) {
        for (int workers : tested_workers) {
            auto warmup_results = workers == 1 ? solve_serial(cases) : solve_parallel(cases, workers);
            (void)warmup_results;
        }
    }

    double t1_median = 0.0;
    std::vector<FEAResult> baseline_serial_res;

    for (int workers : tested_workers) {
        WorkerMeasurement meas;
        meas.workers = workers;
        meas.run_times_ms.reserve(cfg.repetitions);

        for (int rep = 0; rep < cfg.repetitions; ++rep) {
            auto t_start = std::chrono::steady_clock::now();
            std::vector<FEAResult> r;
            if (workers == 1) {
                r = solve_serial(cases);
                if (baseline_serial_res.empty()) {
                    baseline_serial_res = r;
                }
            } else {
                r = solve_parallel(cases, workers);
            }
            auto t_end = std::chrono::steady_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
            meas.run_times_ms.push_back(elapsed_ms);
        }

        // Compute median
        auto sorted_times = meas.run_times_ms;
        std::sort(sorted_times.begin(), sorted_times.end());
        const size_t middle = sorted_times.size() / 2;
        meas.median_time_ms = sorted_times.size() % 2 == 0
            ? (sorted_times[middle - 1] + sorted_times[middle]) / 2.0
            : sorted_times[middle];

        if (workers == 1) {
            t1_median = meas.median_time_ms;
            meas.speedup = 1.0;
            meas.efficiency = 1.0;
        } else {
            meas.speedup = (meas.median_time_ms > 0.0) ? (t1_median / meas.median_time_ms) : 1.0;
            meas.efficiency = meas.speedup / static_cast<double>(workers);
        }

        res.measurements.push_back(meas);
    }

    // Verify every measured parallel configuration. On a single-logical-CPU host,
    // exercise and verify the 2-worker path without timing or claiming speedup.
    bool compared_parallel_results = false;
    res.serial_parallel_max_diff = 0.0;
    for (int workers : tested_workers) {
        if (workers <= 1) continue;
        auto parallel_res = solve_parallel(cases, workers);
        res.serial_parallel_max_diff = std::max(
            res.serial_parallel_max_diff,
            verify_agreement(baseline_serial_res, parallel_res));
        res.verification_workers = workers;
        compared_parallel_results = true;
    }
    if (!compared_parallel_results) {
        constexpr int verification_only_workers = 2;
        auto parallel_res = solve_parallel(cases, verification_only_workers);
        res.serial_parallel_max_diff = verify_agreement(baseline_serial_res, parallel_res);
        res.verification_workers = verification_only_workers;
    }

    res.verification_passed = (res.serial_parallel_max_diff <= 1e-8);
    return res;
}

std::string BatchThermalSolver::export_benchmark_csv(const BenchmarkResult& res) {
    std::ostringstream ss;
    ss << std::setprecision(8);
    ss << "# Unified Engineering Workbench - Multicore Thermal Benchmark Results\n";
    ss << "# CPU: " << res.cpu_model << " | Physical Cores: " << res.physical_cpus
       << " | Logical Cores: " << res.logical_cpus << " | Compiler: " << res.compiler_info
       << " | Optimization Flags: " << res.optimization_flags
       << " | RAM: " << res.total_memory_bytes << " bytes\n";
    ss << "# Workload: " << res.config.num_cases << " cases, Mesh Size N: " << res.config.mesh_size << ", Seed: " << res.config.seed << "\n";
    ss << "# Warm-up: " << (res.config.warm_up ? "one untimed pass per measured worker count" : "disabled") << "\n";
    if (res.launch_to_ready_ms > 0.0) {
        ss << "# Backend launch-to-ready (main entry to HTTP listening; OS process load excluded): "
           << res.launch_to_ready_ms << " ms\n";
    } else {
        ss << "# Backend launch-to-ready: N/A (headless benchmark)\n";
    }
    ss << "# Verification: Max serial/parallel abs diff = " << res.serial_parallel_max_diff << " C (Pass: " << (res.verification_passed ? "YES" : "NO") << ")\n";
    ss << "# Speedup assessable: " << (res.speedup_assessable ? "YES" : "NO")
       << " | Parallel verification workers: " << res.verification_workers << "\n";
    ss << "workers";
    for (int rep = 1; rep <= res.config.repetitions; ++rep) ss << ",rep" << rep << "_ms";
    ss << ",median_ms,speedup_T1_Tp,efficiency\n";

    for (const auto& m : res.measurements) {
        ss << m.workers;
        for (double t : m.run_times_ms) {
            ss << "," << t;
        }
        ss << "," << m.median_time_ms << ",";
        if (res.speedup_assessable) ss << m.speedup << "," << m.efficiency;
        else ss << ",";
        ss << "\n";
    }

    return ss.str();
}

} // namespace fea
} // namespace workbench

