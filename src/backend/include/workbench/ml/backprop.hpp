#pragma once

#include "workbench/common/types.hpp"
#include <string>
#include <vector>

namespace workbench {
namespace ml {

class BackpropagationLab {
public:
    // Compute the forward pass given inputs and weights
    static void forward(
        double x, double y,
        double w, double b, double v, double c,
        double& z_out, double& h_out, double& y_hat_out, double& loss_out
    );

    // Compute analytical chain-rule gradients for all 4 parameters
    static void compute_analytical_gradients(
        double x, double y,
        double w, double b, double v, double c,
        double& grad_w, double& grad_b, double& grad_v, double& grad_c
    );

    // Compute central finite-difference gradients with given epsilon (default 1e-6)
    static void compute_finite_diff_gradients(
        double x, double y,
        double w, double b, double v, double c,
        double eps,
        double& fd_w, double& fd_b, double& fd_v, double& fd_c
    );

    // Execute complete evaluation: Step 0 verification, finite diff check, parameter update,
    // and 100-step gradient descent training history.
    static BackpropResult run_verification_and_training(
        double x = 1.0, double y = 2.0,
        double w0 = 0.5, double b0 = 0.0, double v0 = 1.0, double c0 = 0.0,
        double learning_rate = 0.1,
        int total_steps = 100
    );

    // Export training history to CSV formatted string
    static std::string export_csv(const BackpropResult& result);
};

} // namespace ml
} // namespace workbench

