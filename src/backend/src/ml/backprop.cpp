#include "workbench/ml/backprop.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace workbench {
namespace ml {

void BackpropagationLab::forward(
    double x, double y,
    double w, double b, double v, double c,
    double& z_out, double& h_out, double& y_hat_out, double& loss_out)
{
    z_out = w * x + b;
    h_out = std::tanh(z_out);
    y_hat_out = v * h_out + c;
    double residual = y_hat_out - y;
    loss_out = 0.5 * residual * residual;
}

void BackpropagationLab::compute_analytical_gradients(
    double x, double y,
    double w, double b, double v, double c,
    double& grad_w, double& grad_b, double& grad_v, double& grad_c)
{
    double z, h, y_hat, loss;
    forward(x, y, w, b, v, c, z, h, y_hat, loss);

    double residual = y_hat - y; // dJ/dy_hat
    grad_c = residual * 1.0;     // dJ/dc
    grad_v = residual * h;       // dJ/dv

    double dJ_dh = residual * v;
    double dh_dz = 1.0 - h * h;  // d/dz(tanh(z)) = 1 - tanh^2(z)
    double dJ_dz = dJ_dh * dh_dz;

    grad_b = dJ_dz * 1.0;        // dJ/db
    grad_w = dJ_dz * x;          // dJ/dw
}

void BackpropagationLab::compute_finite_diff_gradients(
    double x, double y,
    double w, double b, double v, double c,
    double eps,
    double& fd_w, double& fd_b, double& fd_v, double& fd_c)
{
    double z, h, y_hat, lp, lm;

    // w
    forward(x, y, w + eps, b, v, c, z, h, y_hat, lp);
    forward(x, y, w - eps, b, v, c, z, h, y_hat, lm);
    fd_w = (lp - lm) / (2.0 * eps);

    // b
    forward(x, y, w, b + eps, v, c, z, h, y_hat, lp);
    forward(x, y, w, b - eps, v, c, z, h, y_hat, lm);
    fd_b = (lp - lm) / (2.0 * eps);

    // v
    forward(x, y, w, b, v + eps, c, z, h, y_hat, lp);
    forward(x, y, w, b, v - eps, c, z, h, y_hat, lm);
    fd_v = (lp - lm) / (2.0 * eps);

    // c
    forward(x, y, w, b, v, c + eps, z, h, y_hat, lp);
    forward(x, y, w, b, v, c - eps, z, h, y_hat, lm);
    fd_c = (lp - lm) / (2.0 * eps);
}

BackpropResult BackpropagationLab::run_verification_and_training(
    double x, double y,
    double w0, double b0, double v0, double c0,
    double learning_rate,
    int total_steps)
{
    BackpropResult res;
    res.initial_x = x;
    res.initial_y = y;
    res.initial_w = w0;
    res.initial_b = b0;
    res.initial_v = v0;
    res.initial_c = c0;
    res.learning_rate = learning_rate;

    // Step 0 forward
    forward(x, y, w0, b0, v0, c0, res.step0_z, res.step0_h, res.step0_y_hat, res.step0_loss);

    // Step 0 analytical gradients
    compute_analytical_gradients(x, y, w0, b0, v0, c0, res.grad_w, res.grad_b, res.grad_v, res.grad_c);

    // Step 0 central finite difference gradients with eps = 1e-6
    const double eps = 1e-6;
    compute_finite_diff_gradients(x, y, w0, b0, v0, c0, eps, res.fd_grad_w, res.fd_grad_b, res.fd_grad_v, res.fd_grad_c);

    // Check errors
    res.fd_err_w = std::abs(res.grad_w - res.fd_grad_w);
    res.fd_err_b = std::abs(res.grad_b - res.fd_grad_b);
    res.fd_err_v = std::abs(res.grad_v - res.fd_grad_v);
    res.fd_err_c = std::abs(res.grad_c - res.fd_grad_c);

    const double tolerance = 1e-5;
    res.fd_check_passed = (res.fd_err_w <= tolerance &&
                           res.fd_err_b <= tolerance &&
                           res.fd_err_v <= tolerance &&
                           res.fd_err_c <= tolerance);

    // Step 1 simultaneous gradient descent update
    res.w_step1 = w0 - learning_rate * res.grad_w;
    res.b_step1 = b0 - learning_rate * res.grad_b;
    res.v_step1 = v0 - learning_rate * res.grad_v;
    res.c_step1 = c0 - learning_rate * res.grad_c;

    // Run 100 training steps
    res.history.reserve(total_steps + 1);

    double cur_w = w0;
    double cur_b = b0;
    double cur_v = v0;
    double cur_c = c0;

    for (int step = 0; step <= total_steps; ++step) {
        double z, h, y_hat, loss;
        forward(x, y, cur_w, cur_b, cur_v, cur_c, z, h, y_hat, loss);

        double gw, gb, gv, gc;
        compute_analytical_gradients(x, y, cur_w, cur_b, cur_v, cur_c, gw, gb, gv, gc);

        BackpropStep s;
        s.step = step;
        s.w = cur_w;
        s.b = cur_b;
        s.v = cur_v;
        s.c = cur_c;
        s.y_hat = y_hat;
        s.loss = loss;
        s.grad_w = gw;
        s.grad_b = gb;
        s.grad_v = gv;
        s.grad_c = gc;
        res.history.push_back(s);

        // Perform simultaneous gradient descent update for next step
        cur_w -= learning_rate * gw;
        cur_b -= learning_rate * gb;
        cur_v -= learning_rate * gv;
        cur_c -= learning_rate * gc;
    }

    return res;
}

std::string BackpropagationLab::export_csv(const BackpropResult& res) {
    std::ostringstream ss;
    ss << std::setprecision(10);
    ss << "# Unified Engineering Workbench - Task F Backpropagation Training Loss\n";
    ss << "# Architecture: h = tanh(w*x + b), y_hat = v*h + c, loss = 0.5*(y_hat - y)^2\n";
    ss << "# Sample: x=" << res.initial_x << ", y=" << res.initial_y << " | Learning rate: " << res.learning_rate << "\n";
    ss << "# Finite Difference Verification (eps=1e-6, tol=1e-5): " << (res.fd_check_passed ? "PASSED" : "FAILED") << "\n";
    ss << "step,w,b,v,c,y_hat,loss,grad_w,grad_b,grad_v,grad_c\n";

    for (const auto& s : res.history) {
        ss << s.step << "," << s.w << "," << s.b << "," << s.v << "," << s.c << ","
           << s.y_hat << "," << s.loss << ","
           << s.grad_w << "," << s.grad_b << "," << s.grad_v << "," << s.grad_c << "\n";
    }

    return ss.str();
}

} // namespace ml
} // namespace workbench

