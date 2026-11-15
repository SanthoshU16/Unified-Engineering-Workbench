#pragma once

#include "workbench/common/types.hpp"
#include "workbench/embedded/device_adapter.hpp"
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>

namespace workbench {
namespace server {

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code{200};
    std::string status_text{"OK"};
    std::string content_type{"text/plain; charset=utf-8"};
    std::string body;

    static HttpResponse json(const std::string& json_str, int code = 200) {
        HttpResponse res;
        res.status_code = code;
        res.status_text = (code == 200) ? "OK" : (code == 400 ? "Bad Request" : "Internal Error");
        res.content_type = "application/json; charset=utf-8";
        res.body = json_str;
        return res;
    }

    static HttpResponse html(const std::string& html_str, int code = 200) {
        HttpResponse res;
        res.status_code = code;
        res.content_type = "text/html; charset=utf-8";
        res.body = html_str;
        return res;
    }

    static HttpResponse error(int code, const std::string& msg) {
        HttpResponse res;
        res.status_code = code;
        res.status_text = (code == 404) ? "Not Found" : "Error";
        res.content_type = "application/json; charset=utf-8";
        res.body = "{\"error\": \"" + msg + "\"}";
        return res;
    }
};

class HttpServer {
public:
    HttpServer(int port = 8080, const std::string& web_root = ".");
    ~HttpServer();

    // Start listening on port (blocking or non-blocking)
    bool start();
    void stop();
    void set_launch_to_ready_ms(double duration_ms) {
        launch_to_ready_ms_.store(duration_ms, std::memory_order_relaxed);
    }

    int get_actual_port() const { return actual_port_; }
    std::string get_url() const { return "http://127.0.0.1:" + std::to_string(actual_port_); }
    bool is_running() const { return running_.load(); }

private:
    int requested_port_{8080};
    int actual_port_{8080};
    std::string web_root_;
    std::atomic<bool> running_{false};
    std::atomic<double> launch_to_ready_ms_{0.0};
    std::thread server_thread_;
    int server_socket_{-1};

    embedded::DeviceManager device_manager_;
    std::mutex embedded_mutex_;

    void run_server_loop();
    void handle_client(int client_socket);
    HttpResponse route_request(const HttpRequest& req);

    HttpResponse handle_static_file(const std::string& path);
    HttpResponse handle_api_system_info();
    HttpResponse handle_api_fea_solve(const std::string& body);
    HttpResponse handle_api_stl_parse(const HttpRequest& request);
    HttpResponse handle_api_ansys_parse(const std::string& body);
    HttpResponse handle_api_benchmark_run(const std::string& body);
    HttpResponse handle_api_embedded_devices();
    HttpResponse handle_api_embedded_action(const std::string& body);
    HttpResponse handle_api_backprop_run(const std::string& body);
};

} // namespace server
} // namespace workbench

