#include "workbench/fea/thermal_1d.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace workbench {
namespace fea {

double Thermal1DSolver::exact_temperature(double x, const FEAConfig& cfg) {
    // T_exact(x) = T0 + (TL - T0)*(x / L) + [q / (2*k)] * x * (L - x)
    // For standard parameters: 100 - 80*(x/L) + [10000 / (20)] * x * (1 - x)
    double linear_part = cfg.T0 + (cfg.TL - cfg.T0) * (x / cfg.L);
    double generation_part = (cfg.q / (2.0 * cfg.k)) * x * (cfg.L - x);
    return linear_part + generation_part;
}

bool Thermal1DSolver::solve_tridiagonal(
    const std::vector<double>& a,
    const std::vector<double>& b,
    const std::vector<double>& c,
    const std::vector<double>& d,
    std::vector<double>& x_out)
{
    const size_t n = d.size();
    if (n == 0) return false;
    if (n == 1) {
        if (std::abs(b[0]) < 1e-15) return false;
        x_out.resize(1);
        x_out[0] = d[0] / b[0];
        return true;
    }

    std::vector<double> cp(n, 0.0);
    std::vector<double> dp(n, 0.0);
    x_out.resize(n, 0.0);

    // Forward sweep
    if (std::abs(b[0]) < 1e-15) return false;
    cp[0] = c[0] / b[0];
    dp[0] = d[0] / b[0];

    for (size_t i = 1; i < n; ++i) {
        double denom = b[i] - a[i] * cp[i - 1];
        if (std::abs(denom) < 1e-15) return false;
        if (i < n - 1) {
            cp[i] = c[i] / denom;
        }
        dp[i] = (d[i] - a[i] * dp[i - 1]) / denom;
    }

    // Back substitution
    x_out[n - 1] = dp[n - 1];
    for (int i = static_cast<int>(n) - 2; i >= 0; --i) {
        x_out[i] = dp[i] - cp[i] * x_out[i + 1];
    }

    return true;
}

FEAResult Thermal1DSolver::solve(const FEAConfig& cfg, bool compute_detailed_evals) {
    FEAResult res;
    res.config = cfg;
    res.success = false;

    // Parameter validation
    if (!std::isfinite(cfg.L) || !std::isfinite(cfg.A) || !std::isfinite(cfg.k) ||
        !std::isfinite(cfg.q) || !std::isfinite(cfg.T0) || !std::isfinite(cfg.TL)) {
        res.error_message = "Validation Error: L, A, k, q, T(0), and T(L) must all be finite numbers.";
        return res;
    }
    if (cfg.L <= 0.0) {
        res.error_message = "Validation Error: Length L must be positive (received L=" + std::to_string(cfg.L) + " m).";
        return res;
    }
    if (cfg.A <= 0.0) {
        res.error_message = "Validation Error: Cross-sectional area A must be positive (received A=" + std::to_string(cfg.A) + " m^2).";
        return res;
    }
    if (cfg.k <= 0.0) {
        res.error_message = "Validation Error: Thermal conductivity k must be positive (received k=" + std::to_string(cfg.k) + " W/(m K)).";
        return res;
    }
    if (cfg.num_elements < 1) {
        res.error_message = "Validation Error: Number of elements N must be >= 1 (received N=" + std::to_string(cfg.num_elements) + ").";
        return res;
    }
    if (cfg.num_elements > 100000) {
        res.error_message = "Validation Error: Number of elements N exceeds safety limit of 100,000.";
        return res;
    }

    const int N = cfg.num_elements;
    const double h = cfg.L / static_cast<double>(N);
    res.h = h;

    const int num_nodes = N + 1;
    if (compute_detailed_evals) {
        res.node_coords.resize(num_nodes);
        for (int i = 0; i < num_nodes; ++i) {
            res.node_coords[i] = static_cast<double>(i) * h;
        }
        // Snap final node coordinate to exactly L to prevent floating precision drift
        res.node_coords.back() = cfg.L;
    }

    res.nodal_temperatures.resize(num_nodes, 0.0);
    res.nodal_temperatures[0] = cfg.T0;
    res.nodal_temperatures[N] = cfg.TL;

    if (N == 1) {
        // Trivial 1-element case
        res.success = true;
    } else {
        // Interior degrees of freedom: nodes 1 to N - 1 (m = N - 1 equations)
        const size_t m = N - 1;
        const double k_elem = (cfg.k * cfg.A) / h;
        const double f_elem = (cfg.q * cfg.A * h) / 2.0;

        std::vector<double> a(m, -k_elem);  // subdiagonal
        std::vector<double> b(m, 2.0 * k_elem); // main diagonal
        std::vector<double> c(m, -k_elem);  // superdiagonal
        std::vector<double> d(m, 2.0 * f_elem); // load vector: q*A*h for internal nodes

        // Dirichlet boundary condition contributions
        d[0] += k_elem * cfg.T0;
        d[m - 1] += k_elem * cfg.TL;

        std::vector<double> T_int;
        if (!solve_tridiagonal(a, b, c, d, T_int)) {
            res.error_message = "Numerical Error: Singular or ill-conditioned stiffness matrix in Thomas algorithm.";
            return res;
        }

        for (size_t i = 0; i < m; ++i) {
            res.nodal_temperatures[i + 1] = T_int[i];
        }
    }

    if (compute_detailed_evals) {
        // Midpoint evaluations and error calculation
        res.midpoint_evals.resize(N);
        double max_mid_err = 0.0;
        for (int e = 0; e < N; ++e) {
            double x_mid = 0.5 * (res.node_coords[e] + res.node_coords[e + 1]);
            double T_fea_mid = 0.5 * (res.nodal_temperatures[e] + res.nodal_temperatures[e + 1]);
            double T_ex_mid = exact_temperature(x_mid, cfg);
            double err = std::abs(T_fea_mid - T_ex_mid);

            res.midpoint_evals[e] = {x_mid, T_fea_mid, T_ex_mid, err};
            max_mid_err = std::max(max_mid_err, err);
        }
        res.max_midpoint_error = max_mid_err;

        // Nodal error calculation (verifies superconvergence)
        double max_node_err = 0.0;
        for (int i = 0; i < num_nodes; ++i) {
            double T_ex_node = exact_temperature(res.node_coords[i], cfg);
            double err = std::abs(res.nodal_temperatures[i] - T_ex_node);
            max_node_err = std::max(max_node_err, err);
        }
        res.max_nodal_error = max_node_err;
    }

    res.success = true;
    return res;
}

std::vector<ConvergenceEntry> Thermal1DSolver::run_convergence_study(
    FEAConfig base_config,
    const std::vector<int>& mesh_sizes)
{
    std::vector<ConvergenceEntry> entries;
    entries.reserve(mesh_sizes.size());

    for (size_t i = 0; i < mesh_sizes.size(); ++i) {
        int N = mesh_sizes[i];
        base_config.num_elements = N;
        FEAResult res = solve(base_config);
        if (!res.success) return {};

        ConvergenceEntry entry;
        entry.N = N;
        entry.h = res.h;
        entry.max_midpoint_error = res.max_midpoint_error;
        // Theoretical midpoint error: (q / (8*k)) * h^2
        entry.theoretical_error = (std::abs(base_config.q) / (8.0 * base_config.k)) * res.h * res.h;

        if (i == 0) {
            entry.convergence_rate = 0.0;
        } else {
            // alpha = ln(E_prev / E_curr) / ln(h_prev / h_curr)
            double h_ratio = entries[i - 1].h / entry.h;
            double err_ratio = entries[i - 1].max_midpoint_error / entry.max_midpoint_error;
            if (h_ratio > 1.0 && err_ratio > 0.0) {
                entry.convergence_rate = std::log(err_ratio) / std::log(h_ratio);
            } else {
                entry.convergence_rate = 0.0;
            }
        }
        entries.push_back(entry);
    }

    return entries;
}

std::string Thermal1DSolver::export_csv(
    const FEAResult& res,
    const std::vector<ConvergenceEntry>& convergence)
{
    std::ostringstream ss;
    ss << std::setprecision(10);
    ss << "# Unified Engineering Workbench - 1D Thermal FEA Results\n";
    ss << "# Parameters: L=" << res.config.L << " m, A=" << res.config.A << " m2, k="
       << res.config.k << " W/(m K), q=" << res.config.q << " W/m3, T0="
       << res.config.T0 << " C, TL=" << res.config.TL << " C, N=" << res.config.num_elements << "\n";
    ss << "# mesh_profile\n";
    ss << "type,x_m,T_fea_C,T_exact_C,absolute_error_C\n";

    // Nodal values
    for (size_t i = 0; i < res.node_coords.size(); ++i) {
        double x = res.node_coords[i];
        double t_fea = res.nodal_temperatures[i];
        double t_ex = exact_temperature(x, res.config);
        ss << "node," << x << "," << t_fea << "," << t_ex << "," << std::abs(t_fea - t_ex) << "\n";
    }

    // Midpoint values
    for (const auto& p : res.midpoint_evals) {
        ss << "midpoint," << p.x << "," << p.T_fea << "," << p.T_exact << "," << p.error << "\n";
    }

    ss << "# mesh_convergence\n";
    ss << "elements_N,element_length_h_m,max_midpoint_error_C,theoretical_error_bound_C,observed_convergence_rate\n";
    for (const auto& entry : convergence) {
        ss << entry.N << "," << entry.h << "," << entry.max_midpoint_error << ","
           << entry.theoretical_error << "," << entry.convergence_rate << "\n";
    }

    return ss.str();
}

} // namespace fea
} // namespace workbench

