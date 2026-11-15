#pragma once

#include "workbench/common/types.hpp"
#include <vector>
#include <string>

namespace workbench {
namespace fea {

class Thermal1DSolver {
public:
    // Solve the 1D steady-state thermal FEA problem for the given configuration
    static FEAResult solve(const FEAConfig& config);

    // Compute exact analytical temperature at position x:
    // T_exact(x) = T0 + (TL - T0)*x/L + [q/(2k)]*x*(L - x)
    static double exact_temperature(double x, const FEAConfig& config);

    // Run convergence study for N = 10, 20, 40, 80 (or custom mesh sizes)
    static std::vector<ConvergenceEntry> run_convergence_study(
        FEAConfig base_config,
        const std::vector<int>& mesh_sizes = {10, 20, 40, 80}
    );

    // Export FEA result to CSV formatted string
    static std::string export_csv(
        const FEAResult& result,
        const std::vector<ConvergenceEntry>& convergence = {}
    );

    // Tridiagonal Matrix Algorithm (Thomas algorithm) solver
    // Solves A * x = d where A is tridiagonal with subdiagonal a, diagonal b, superdiagonal c
    static bool solve_tridiagonal(
        const std::vector<double>& a,
        const std::vector<double>& b,
        const std::vector<double>& c,
        const std::vector<double>& d,
        std::vector<double>& x_out
    );
};

} // namespace fea
} // namespace workbench

