#pragma once

#include "workbench/common/types.hpp"
#include <vector>
#include <string>

namespace workbench {
namespace fea {

class BatchThermalSolver {
public:
    // Generate a reproducible list of independent FEAConfigs using a fixed seed
    static std::vector<FEAConfig> generate_batch(int num_cases, int mesh_size, uint64_t seed);

    // Solve batch serially (1 CPU thread)
    // Excludes allocation overhead from timing if pre-allocated
    static std::vector<FEAResult> solve_serial(const std::vector<FEAConfig>& cases);

    // Solve batch using a bounded thread pool with num_workers threads
    // Retains strict deterministic input ordering in output vector
    static std::vector<FEAResult> solve_parallel(const std::vector<FEAConfig>& cases, int num_workers);

    // Verify serial vs parallel results agreement (max absolute diff <= 1e-8 C)
    static double verify_agreement(
        const std::vector<FEAResult>& serial_res,
        const std::vector<FEAResult>& parallel_res
    );

    // Run the multicore benchmark suite using the configured repetitions for 1, 2, and 4 workers.
    static BenchmarkResult run_benchmark(const BenchmarkConfig& config);

    // Export benchmark results to CSV format
    static std::string export_benchmark_csv(const BenchmarkResult& result);
};

} // namespace fea
} // namespace workbench

