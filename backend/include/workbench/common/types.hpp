#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>
#include <sstream>
#include <iomanip>

namespace workbench {

// 3D Vector for geometry and coordinates
struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }

    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    double norm() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const {
        double n = norm();
        return (n > 1e-12) ? (*this / n) : Vec3(0, 0, 0);
    }

    bool is_finite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
};

// Triangle for STL geometry
struct Triangle {
    Vec3 normal;
    Vec3 v1;
    Vec3 v2;
    Vec3 v3;

    bool is_finite() const {
        return normal.is_finite() && v1.is_finite() && v2.is_finite() && v3.is_finite();
    }
};

// Parsed STL 3D Model
struct STLModel {
    std::string solid_name;
    std::vector<Triangle> triangles;
    Vec3 bbox_min{std::numeric_limits<double>::max(),
                  std::numeric_limits<double>::max(),
                  std::numeric_limits<double>::max()};
    Vec3 bbox_max{std::numeric_limits<double>::lowest(),
                  std::numeric_limits<double>::lowest(),
                  std::numeric_limits<double>::lowest()};
    size_t file_size_bytes{0};
    bool is_valid{false};
    std::string error_message;

    void update_bounds() {
        bbox_min = {std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::max()};
        bbox_max = {std::numeric_limits<double>::lowest(),
                    std::numeric_limits<double>::lowest(),
                    std::numeric_limits<double>::lowest()};
        for (const auto& tri : triangles) {
            for (const auto& v : {tri.v1, tri.v2, tri.v3}) {
                bbox_min.x = std::min(bbox_min.x, v.x);
                bbox_min.y = std::min(bbox_min.y, v.y);
                bbox_min.z = std::min(bbox_min.z, v.z);
                bbox_max.x = std::max(bbox_max.x, v.x);
                bbox_max.y = std::max(bbox_max.y, v.y);
                bbox_max.z = std::max(bbox_max.z, v.z);
            }
        }
    }

    Vec3 center() const {
        return (bbox_min + bbox_max) * 0.5;
    }

    Vec3 dimensions() const {
        return bbox_max - bbox_min;
    }
};

// ANSYS Nodal Temperature Data Point
struct ANSYSNode {
    int64_t node_id{0};
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double temperature{0.0}; // Celsius

    bool is_finite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(temperature);
    }
};

// ANSYS Dataset
struct ANSYSDataSet {
    std::vector<ANSYSNode> nodes;
    Vec3 bbox_min{0, 0, 0};
    Vec3 bbox_max{0, 0, 0};
    double min_temperature{0.0};
    double max_temperature{0.0};
    double mean_temperature{0.0};
    size_t file_size_bytes{0};
    bool is_valid{false};
    std::string error_message;

    void compute_statistics() {
        if (nodes.empty()) return;
        bbox_min = {std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::max()};
        bbox_max = {std::numeric_limits<double>::lowest(),
                    std::numeric_limits<double>::lowest(),
                    std::numeric_limits<double>::lowest()};
        min_temperature = std::numeric_limits<double>::max();
        max_temperature = std::numeric_limits<double>::lowest();
        double sum_temp = 0.0;

        for (const auto& n : nodes) {
            bbox_min.x = std::min(bbox_min.x, n.x);
            bbox_min.y = std::min(bbox_min.y, n.y);
            bbox_min.z = std::min(bbox_min.z, n.z);
            bbox_max.x = std::max(bbox_max.x, n.x);
            bbox_max.y = std::max(bbox_max.y, n.y);
            bbox_max.z = std::max(bbox_max.z, n.z);

            min_temperature = std::min(min_temperature, n.temperature);
            max_temperature = std::max(max_temperature, n.temperature);
            sum_temp += n.temperature;
        }
        mean_temperature = sum_temp / static_cast<double>(nodes.size());
    }
};

// 1D Thermal FEA Input Configuration
struct FEAConfig {
    double L{1.0};          // Length in m
    double A{1e-4};         // Cross-sectional area in m^2
    double k{10.0};         // Thermal conductivity in W/(m K)
    double q{10000.0};      // Volumetric heat generation in W/m^3
    double T0{100.0};       // Left boundary temperature at x=0 (deg C)
    double TL{20.0};        // Right boundary temperature at x=L (deg C)
    int num_elements{10};   // Number of linear 2-node elements
};

// Single FEA Point Evaluation
struct FEAPoint {
    double x{0.0};
    double T_fea{0.0};
    double T_exact{0.0};
    double error{0.0};
};

// 1D Thermal FEA Solution Result
struct FEAResult {
    FEAConfig config;
    double h{0.1};                           // Element length (L / num_elements)
    std::vector<double> node_coords;         // x_i (N + 1 nodes)
    std::vector<double> nodal_temperatures;  // T_i at nodes
    std::vector<FEAPoint> midpoint_evals;    // Evaluated at element midpoints
    double max_midpoint_error{0.0};          // Max absolute midpoint error
    double max_nodal_error{0.0};             // Max absolute nodal error
    bool success{true};
    std::string error_message;
};

// Multi-Mesh Convergence Point
struct ConvergenceEntry {
    int N{0};
    double h{0.0};
    double max_midpoint_error{0.0};
    double theoretical_error{0.0};
    double convergence_rate{0.0};
};

// Multicore Benchmark Configuration
struct BenchmarkConfig {
    int num_cases{20000};
    int mesh_size{200};
    uint64_t seed{42};
    std::vector<int> worker_counts{1, 2, 4};
    int repetitions{5};
    bool warm_up{true};
};

// Multicore Benchmark Run Measurement
struct WorkerMeasurement {
    int workers{1};
    std::vector<double> run_times_ms;
    double median_time_ms{0.0};
    double speedup{1.0};
    double efficiency{1.0};
};

// Multicore Benchmark Complete Results
struct BenchmarkResult {
    BenchmarkConfig config;
    std::vector<WorkerMeasurement> measurements;
    double serial_parallel_max_diff{0.0}; // Must be <= 1e-8
    double launch_to_ready_ms{0.0};
    uint64_t total_memory_bytes{0};
    int logical_cpus{1};
    int physical_cpus{1};
    int verification_workers{0};
    std::string cpu_model{"Generic CPU"};
    std::string compiler_info;
    std::string optimization_flags{"-O3"};
    bool speedup_assessable{false};
    bool verification_passed{false};
};

// Backpropagation Training Step Record
struct BackpropStep {
    int step{0};
    double w{0.0};
    double b{0.0};
    double v{0.0};
    double c{0.0};
    double y_hat{0.0};
    double loss{0.0};
    double grad_w{0.0};
    double grad_b{0.0};
    double grad_v{0.0};
    double grad_c{0.0};
};

// Backpropagation Complete Verification & Training Result
struct BackpropResult {
    // Step 0 forward
    double initial_x{1.0};
    double initial_y{2.0};
    double initial_w{0.5};
    double initial_b{0.0};
    double initial_v{1.0};
    double initial_c{0.0};
    double learning_rate{0.1};

    double step0_z{0.0};
    double step0_h{0.0};
    double step0_y_hat{0.0};
    double step0_loss{0.0};

    // Step 0 analytical gradients
    double grad_w{0.0};
    double grad_b{0.0};
    double grad_v{0.0};
    double grad_c{0.0};

    // Step 0 central finite difference gradients (eps = 1e-6)
    double fd_grad_w{0.0};
    double fd_grad_b{0.0};
    double fd_grad_v{0.0};
    double fd_grad_c{0.0};

    // Absolute errors between analytical and finite difference
    double fd_err_w{0.0};
    double fd_err_b{0.0};
    double fd_err_v{0.0};
    double fd_err_c{0.0};
    bool fd_check_passed{false};

    // Updated parameters after step 1
    double w_step1{0.0};
    double b_step1{0.0};
    double v_step1{0.0};
    double c_step1{0.0};

    // 100 training steps history
    std::vector<BackpropStep> history;
};

} // namespace workbench

