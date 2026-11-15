#include "workbench/server/http_server.hpp"
#include "workbench/common/json.hpp"
#include "workbench/common/system_info.hpp"
#include "workbench/fea/thermal_1d.hpp"
#include "workbench/fea/thermal_batch.hpp"
#include "workbench/cad/stl_parser.hpp"
#include "workbench/cad/ansys_parser.hpp"
#include "workbench/ml/backprop.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <cctype>
#include <algorithm>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#define CLOSE_SOCKET(s) closesocket(s)
#define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
using socket_t = int;
#define CLOSE_SOCKET(s) ::close(s)
#define IS_INVALID_SOCKET(s) ((s) < 0)
#endif

namespace workbench {
namespace server {

namespace {

// JSON extraction helpers
std::string get_json_str(const std::string& json, const std::string& key, const std::string& default_val = "") {
    return workbench::json::get_string_field(json, key, default_val);
}

double get_json_double(const std::string& json, const std::string& key, double default_val) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return default_val;
    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) return default_val;
    pos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos) return default_val;

    size_t end_pos = json.find_first_of(",}\r\n ", pos);
    std::string val_str = json.substr(pos, end_pos - pos);
    try {
        return std::stod(val_str);
    } catch (...) {
        return default_val;
    }
}

int get_json_int(const std::string& json, const std::string& key, int default_val) {
    return static_cast<int>(get_json_double(json, key, static_cast<double>(default_val)));
}

std::string get_mime_type(const std::string& path) {
    size_t dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".stl") return "text/plain; charset=utf-8";
    if (ext == ".csv") return "text/csv; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    return "text/plain; charset=utf-8";
}

} // anonymous namespace

HttpServer::HttpServer(int port, const std::string& web_root, bool bind_all_interfaces)
    : requested_port_(port), actual_port_(port), web_root_(web_root), bind_all_interfaces_(bind_all_interfaces)
{
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

HttpServer::~HttpServer() {
    stop();
#if defined(_WIN32)
    WSACleanup();
#endif
}

bool HttpServer::start() {
    if (running_) return true;

    socket_t s = -1;
    for (int port = requested_port_; port < requested_port_ + 20; ++port) {
        s = socket(AF_INET, SOCK_STREAM, 0);
        if (IS_INVALID_SOCKET(s)) continue;

        int opt = 1;
#if defined(_WIN32)
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(bind_all_interfaces_ ? INADDR_ANY : INADDR_LOOPBACK);
        addr.sin_port = htons(port);

        if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            if (listen(s, 64) == 0) {
                actual_port_ = port;
                server_socket_ = s;
                break;
            }
        }
        CLOSE_SOCKET(s);
        s = -1;
    }

    if (IS_INVALID_SOCKET(s)) {
        std::cerr << "[HttpServer] Failed to bind to any port in range "
                  << requested_port_ << "-" << requested_port_ + 20 << "\n";
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&HttpServer::run_server_loop, this);
    std::cout << "[HttpServer] Running on " << get_url() << " (serving: " << web_root_ << ")\n";
    return true;
}

void HttpServer::stop() {
    if (!running_) return;
    running_ = false;

    if (!IS_INVALID_SOCKET(server_socket_)) {
        CLOSE_SOCKET(server_socket_);
        server_socket_ = -1;
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void HttpServer::run_server_loop() {
    while (running_) {
        sockaddr_in client_addr;
#if defined(_WIN32)
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif
        socket_t client_sock = accept(server_socket_, (struct sockaddr*)&client_addr, &addr_len);
        if (IS_INVALID_SOCKET(client_sock)) {
            if (!running_) break;
            continue;
        }

        // Process request in a detached thread for non-blocking concurrent UI responsiveness
        std::thread([this, client_sock]() {
            this->handle_client(client_sock);
        }).detach();
    }
}

void HttpServer::handle_client(int client_sock) {
    constexpr size_t MAX_REQUEST_BODY_BYTES = 160 * 1024 * 1024;
    std::string raw_request;
    char buffer[4096];
    int bytes_read = 0;

    // Read HTTP request header
    while ((bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        raw_request.append(buffer, bytes_read);
        if (raw_request.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    if (raw_request.empty()) {
        CLOSE_SOCKET(client_sock);
        return;
    }

    // Parse header and body
    size_t header_end = raw_request.find("\r\n\r\n");
    std::string header_part = raw_request.substr(0, header_end);
    std::string body_part = (header_end != std::string::npos) ? raw_request.substr(header_end + 4) : "";

    std::istringstream hstream(header_part);
    std::string request_line;
    std::getline(hstream, request_line);

    HttpRequest req;
    std::istringstream rl_stream(request_line);
    rl_stream >> req.method >> req.path;

    // Remove query parameters from path for routing
    size_t query_pos = req.path.find('?');
    if (query_pos != std::string::npos) {
        req.path = req.path.substr(0, query_pos);
    }

    // Parse headers
    std::string hline;
    size_t content_length = 0;
    while (std::getline(hstream, hline)) {
        if (!hline.empty() && hline.back() == '\r') hline.pop_back();
        size_t colon = hline.find(':');
        if (colon != std::string::npos) {
            std::string key = hline.substr(0, colon);
            std::string val = hline.substr(colon + 1);
            while (!val.empty() && val[0] == ' ') val.erase(val.begin());
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            req.headers[key] = val;
            if (key == "content-length") {
                try { content_length = std::stoul(val); } catch (...) {}
            }
        }
    }

    HttpResponse res;
    if (content_length > MAX_REQUEST_BODY_BYTES) {
        res = HttpResponse::error(413, "Request body exceeds the 160 MiB upload transport limit.");
    } else {
        // Read remaining body if Content-Length exceeds what was already buffered.
        while (body_part.size() < content_length) {
            int r = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (r <= 0) break;
            body_part.append(buffer, r);
        }
        req.body = body_part;
        res = route_request(req);
    }

    // Send HTTP response
    std::ostringstream resp_stream;
    resp_stream << "HTTP/1.1 " << res.status_code << " " << res.status_text << "\r\n";
    resp_stream << "Content-Type: " << res.content_type << "\r\n";
    resp_stream << "Content-Length: " << res.body.size() << "\r\n";
    resp_stream << "Access-Control-Allow-Origin: *\r\n";
    resp_stream << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    resp_stream << "Access-Control-Allow-Headers: Content-Type\r\n";
    resp_stream << "Connection: close\r\n\r\n";
    resp_stream << res.body;

    std::string resp_str = resp_stream.str();
    size_t sent = 0;
    while (sent < resp_str.size()) {
        const size_t remaining = resp_str.size() - sent;
        const int chunk_size = static_cast<int>(std::min(remaining, static_cast<size_t>(64 * 1024)));
        const int result = send(client_sock, resp_str.data() + sent, chunk_size, 0);
        if (result <= 0) break;
        sent += static_cast<size_t>(result);
    }
    CLOSE_SOCKET(client_sock);
}

HttpResponse HttpServer::route_request(const HttpRequest& req) {
    if (req.method == "OPTIONS") {
        HttpResponse res;
        res.status_code = 200;
        res.status_text = "OK";
        return res;
    }

    // REST API routes
    if (req.path == "/api/system/info" && req.method == "GET") {
        return handle_api_system_info();
    }
    if (req.path == "/api/fea/solve" && req.method == "POST") {
        return handle_api_fea_solve(req.body);
    }
    if (req.path == "/api/cad/stl/parse" && req.method == "POST") {
        return handle_api_stl_parse(req);
    }
    if (req.path == "/api/cad/ansys/parse" && req.method == "POST") {
        return handle_api_ansys_parse(req.body);
    }
    if (req.path == "/api/benchmark/run" && req.method == "POST") {
        return handle_api_benchmark_run(req.body);
    }
    if (req.path == "/api/embedded/devices" && req.method == "GET") {
        return handle_api_embedded_devices();
    }
    if (req.path == "/api/embedded/action" && req.method == "POST") {
        return handle_api_embedded_action(req.body);
    }
    if (req.path == "/api/backprop/run" && req.method == "POST") {
        return handle_api_backprop_run(req.body);
    }

    // Fall back to static file serving
    return handle_static_file(req.path);
}

HttpResponse HttpServer::handle_static_file(const std::string& path) {
    std::string local_path = path;
    if (local_path.empty() || local_path == "/") {
        local_path = "/src/frontend/public/index.html";
    }

    // Prevent directory traversal attacks
    if (local_path.find("..") != std::string::npos) {
        return HttpResponse::error(403, "Forbidden path traversal");
    }

    // Map URL path to filesystem path
    std::string file_path = web_root_ + local_path;
    std::ifstream f(file_path, std::ios::binary);
    if (!f.is_open()) {
        // Resolve root-relative assets from the structured source tree.
        std::string fallback_path = web_root_ + "/src/frontend/public" + local_path;
        f.open(fallback_path, std::ios::binary);
        if (f.is_open()) {
            file_path = fallback_path;
        } else {
            // ES modules are served from src/frontend/src.
            fallback_path = web_root_ + "/src/frontend" + local_path;
            f.open(fallback_path, std::ios::binary);
            if (f.is_open()) {
                file_path = fallback_path;
            } else {
                return HttpResponse::error(404, "Static file not found: " + path);
            }
        }
    }

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    HttpResponse res;
    res.status_code = 200;
    res.status_text = "OK";
    res.content_type = get_mime_type(file_path);
    res.body = content;
    return res;
}

HttpResponse HttpServer::handle_api_system_info() {
    const int logical_cpus = system_info::logical_cpu_count();
    const int physical_cpus = system_info::physical_cpu_count();

    std::ostringstream ss;
    ss << "{\n"
       << "  \"status\": \"READY\",\n"
       << "  \"workbench_version\": \"1.0.0-R&D\",\n"
       << "  \"logical_cpus\": " << logical_cpus << ",\n"
       << "  \"physical_cpus\": " << physical_cpus << ",\n"
       << "  \"total_memory_bytes\": " << system_info::total_memory_bytes() << ",\n"
       << "  \"cpu_model\": \"" << json::escape_string(system_info::cpu_model()) << "\",\n"
#if defined(__APPLE__)
       << "  \"platform\": \"macOS\",\n"
#elif defined(_WIN32)
       << "  \"platform\": \"Windows\",\n"
#else
       << "  \"platform\": \"Linux\",\n"
#endif
#if defined(__clang__)
       << "  \"compiler\": \"Clang " << __clang_major__ << "." << __clang_minor__ << "\",\n"
#elif defined(__GNUC__)
       << "  \"compiler\": \"GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "\",\n"
#elif defined(_MSC_VER)
       << "  \"compiler\": \"MSVC " << _MSC_VER << "\",\n"
#else
       << "  \"compiler\": \"C++17 Compiler\",\n"
#endif
#if defined(_MSC_VER)
       << "  \"optimization_flags\": \"/O2\",\n"
#elif defined(__clang__) || defined(__GNUC__)
       << "  \"optimization_flags\": \"-O3\",\n"
#else
       << "  \"optimization_flags\": \"Compiler default (unknown)\",\n"
#endif
       << "  \"server_port\": " << actual_port_ << "\n"
       << "}";
    return HttpResponse::json(ss.str());
}

HttpResponse HttpServer::handle_api_fea_solve(const std::string& body) {
    FEAConfig cfg;
    cfg.L = get_json_double(body, "L", 1.0);
    cfg.A = get_json_double(body, "A", 1e-4);
    cfg.k = get_json_double(body, "k", 10.0);
    cfg.q = get_json_double(body, "q", 10000.0);
    cfg.T0 = get_json_double(body, "T0", 100.0);
    cfg.TL = get_json_double(body, "TL", 20.0);
    cfg.num_elements = get_json_int(body, "num_elements", 10);

    FEAResult res = fea::Thermal1DSolver::solve(cfg);
    if (!res.success) {
        return HttpResponse::json(json::serialize_fea_result(res), 400);
    }
    auto conv = fea::Thermal1DSolver::run_convergence_study(cfg, {10, 20, 40, 80});

    std::string json_res = json::serialize_fea_result(res, conv);
    return HttpResponse::json(json_res, res.success ? 200 : 400);
}

HttpResponse HttpServer::handle_api_stl_parse(const HttpRequest& request) {
    std::string content_type;
    for (const auto& header : request.headers) {
        std::string key = header.first;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (key == "content-type") {
            content_type = header.second;
            std::transform(content_type.begin(), content_type.end(), content_type.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            break;
        }
    }

    const std::string& body = request.body;
    if (content_type.find("application/octet-stream") != std::string::npos) {
        STLModel model = cad::STLParser::parse_string(body, "uploaded_stl");
        return HttpResponse::json(json::serialize_stl(model, true), model.is_valid ? 200 : 400);
    }

    std::string filepath = get_json_str(body, "filepath", "");
    std::string raw_stl = get_json_str(body, "raw_content", "");

    STLModel model;
    if (!filepath.empty()) {
        std::string full_path = (filepath[0] == '/') ? filepath : (web_root_ + "/" + filepath);
        model = cad::STLParser::parse_file(full_path);
    } else if (!raw_stl.empty()) {
        model = cad::STLParser::parse_string(raw_stl, "user_upload.stl");
    } else {
        // If the body itself is raw STL ASCII
        if (body.rfind("solid", 0) == 0) {
            model = cad::STLParser::parse_string(body, "user_upload.stl");
        } else {
            return HttpResponse::error(400, "Missing filepath or raw_content in STL parse request");
        }
    }

    std::string json_res = json::serialize_stl(model, true);
    return HttpResponse::json(json_res, model.is_valid ? 200 : 400);
}

HttpResponse HttpServer::handle_api_ansys_parse(const std::string& body) {
    std::string filepath = get_json_str(body, "filepath", "");
    std::string raw_csv = get_json_str(body, "raw_content", "");

    ANSYSDataSet ds;
    if (!filepath.empty()) {
        std::string full_path = (filepath[0] == '/') ? filepath : (web_root_ + "/" + filepath);
        ds = cad::ANSYSParser::parse_file(full_path);
    } else if (!raw_csv.empty()) {
        ds = cad::ANSYSParser::parse_string(raw_csv, "user_upload.csv");
    } else {
        // If body itself is CSV
        if (body.find("node_id") != std::string::npos || body.find("temperature") != std::string::npos) {
            ds = cad::ANSYSParser::parse_string(body, "user_upload.csv");
        } else {
            return HttpResponse::error(400, "Missing filepath or raw_content in ANSYS CSV parse request");
        }
    }

    std::string json_res = json::serialize_ansys(ds, true);
    return HttpResponse::json(json_res, ds.is_valid ? 200 : 400);
}

HttpResponse HttpServer::handle_api_benchmark_run(const std::string& body) {
    BenchmarkConfig cfg;
    cfg.num_cases = get_json_int(body, "num_cases", 2000);
    cfg.mesh_size = get_json_int(body, "mesh_size", 100);
    cfg.seed = static_cast<uint64_t>(get_json_double(body, "seed", 42.0));
    cfg.repetitions = get_json_int(body, "repetitions", 5);
    if (cfg.repetitions < 1 || cfg.repetitions > 50) {
        return HttpResponse::error(400, "Benchmark repetitions must be between 1 and 50.");
    }
    cfg.warm_up = true;

    BenchmarkResult res = fea::BatchThermalSolver::run_benchmark(cfg);
    res.launch_to_ready_ms = launch_to_ready_ms_.load(std::memory_order_relaxed);
    std::string json_res = json::serialize_benchmark_result(res);
    return HttpResponse::json(json_res);
}

HttpResponse HttpServer::handle_api_embedded_devices() {
    std::lock_guard<std::mutex> lock(embedded_mutex_);
    std::string json_res = device_manager_.serialize_all_status_json();
    return HttpResponse::json(json_res);
}

HttpResponse HttpServer::handle_api_embedded_action(const std::string& body) {
    std::lock_guard<std::mutex> lock(embedded_mutex_);
    std::string dev_id = get_json_str(body, "device_id", "");
    std::string action = get_json_str(body, "action", "");

    auto* dev = device_manager_.get_device(dev_id);
    if (!dev) {
        return HttpResponse::error(404, "Device ID not found: " + dev_id);
    }

    if (action == "connect") {
        bool ok = dev->connect();
        return HttpResponse::json("{\"status\": \"" + embedded::to_string(dev->state()) + "\", \"success\": " + (ok ? "true" : "false") + ", \"message\": \"" + (ok ? "Connection established." : "Discover the device before connecting.") + "\"}");
    } else if (action == "discover") {
        bool ok = dev->discover();
        return HttpResponse::json("{\"status\": \"" + embedded::to_string(dev->state()) + "\", \"success\": " + (ok ? "true" : "false") + ", \"message\": \"" + (ok ? "Device discovered and ready to connect." : "Disconnect the device before discovery.") + "\"}");
    } else if (action == "disconnect") {
        dev->disconnect();
        return HttpResponse::json("{\"status\": \"" + embedded::to_string(dev->state()) + "\", \"success\": true}");
    } else if (action == "telemetry") {
        return HttpResponse::json(dev->poll_telemetry());
    } else if (action == "capabilities") {
        return HttpResponse::json(dev->get_capability_report());
    } else if (action == "inject_failure") {
        bool ok = dev->inject_failure("TIMEOUT");
        return HttpResponse::json("{\"status\": \"" + embedded::to_string(dev->state()) + "\", \"failure_injected\": " + (ok ? "true" : "false") + ", \"success\": " + (ok ? "true" : "false") + ", \"message\": \"" + (ok ? "Simulated timeout injected." : "Connect the device before injecting a timeout.") + "\"}");
    } else if (action == "recover") {
        bool ok = dev->recover();
        return HttpResponse::json("{\"status\": \"" + embedded::to_string(dev->state()) + "\", \"recovered\": " + (ok ? "true" : "false") + ", \"success\": " + (ok ? "true" : "false") + ", \"message\": \"" + (ok ? "Interface restored; telemetry will resume." : "Recovery is available only after a timeout fault.") + "\"}");
    }

    return HttpResponse::error(400, "Unknown action: " + action);
}

HttpResponse HttpServer::handle_api_backprop_run(const std::string& body) {
    double lr = get_json_double(body, "learning_rate", 0.1);
    int steps = get_json_int(body, "steps", 100);

    BackpropResult res = ml::BackpropagationLab::run_verification_and_training(
        1.0, 2.0, 0.5, 0.0, 1.0, 0.0, lr, steps
    );

    std::string json_res = json::serialize_backprop_result(res);
    return HttpResponse::json(json_res);
}

} // namespace server
} // namespace workbench

