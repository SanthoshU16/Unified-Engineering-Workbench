#pragma once

#include "workbench/common/types.hpp"
#include "workbench/cad/stl_parser.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <map>
#include <cstdint>

namespace workbench {
namespace json {

inline std::string escape_string(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

inline void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

inline bool read_hex_quad(const std::string& source, size_t& pos, std::uint32_t& value) {
    if (pos + 4 > source.size()) return false;
    value = 0;
    for (int i = 0; i < 4; ++i) {
        const char c = source[pos++];
        value <<= 4;
        if (c >= '0' && c <= '9') value |= static_cast<std::uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint32_t>(c - 'A' + 10);
        else return false;
    }
    return true;
}

inline bool decode_json_string(const std::string& source, size_t& pos, std::string& output) {
    if (pos >= source.size() || source[pos] != '"') return false;
    ++pos;
    output.clear();
    while (pos < source.size()) {
        const unsigned char c = static_cast<unsigned char>(source[pos++]);
        if (c == '"') return true;
        if (c < 0x20) return false;
        if (c != '\\') {
            output.push_back(static_cast<char>(c));
            continue;
        }
        if (pos >= source.size()) return false;
        switch (source[pos++]) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                std::uint32_t codepoint = 0;
                if (!read_hex_quad(source, pos, codepoint)) return false;
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    if (pos + 2 > source.size() || source[pos] != '\\' || source[pos + 1] != 'u') return false;
                    pos += 2;
                    std::uint32_t low = 0;
                    if (!read_hex_quad(source, pos, low) || low < 0xdc00 || low > 0xdfff) return false;
                    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                    return false;
                }
                append_utf8(output, codepoint);
                break;
            }
            default: return false;
        }
    }
    return false;
}

inline std::string get_string_field(
    const std::string& source,
    const std::string& key,
    const std::string& default_value = "")
{
    const std::string pattern = "\"" + key + "\"";
    size_t pos = source.find(pattern);
    if (pos == std::string::npos) return default_value;
    pos = source.find(':', pos + pattern.size());
    if (pos == std::string::npos) return default_value;
    pos = source.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos) return default_value;

    std::string value;
    if (!decode_json_string(source, pos, value)) return default_value;
    return value;
}

inline std::string serialize_stl(const STLModel& model, bool include_triangles = true) {
    std::ostringstream ss;
    ss << std::setprecision(10);
    ss << "{\n";
    ss << "  \"solid_name\": \"" << escape_string(model.solid_name) << "\",\n";
    ss << "  \"triangle_count\": " << model.triangles.size() << ",\n";
    ss << "  \"file_size_bytes\": " << model.file_size_bytes << ",\n";
    ss << "  \"is_valid\": " << (model.is_valid ? "true" : "false") << ",\n";
    ss << "  \"error_message\": \"" << escape_string(model.error_message) << "\",\n";
    ss << "  \"bbox_min\": {\"x\": " << model.bbox_min.x << ", \"y\": " << model.bbox_min.y << ", \"z\": " << model.bbox_min.z << "},\n";
    ss << "  \"bbox_max\": {\"x\": " << model.bbox_max.x << ", \"y\": " << model.bbox_max.y << ", \"z\": " << model.bbox_max.z << "},\n";
    Vec3 dims = model.dimensions();
    ss << "  \"dimensions\": {\"x\": " << dims.x << ", \"y\": " << dims.y << ", \"z\": " << dims.z << "}";

    if (include_triangles && model.is_valid && model.triangles.size() <= cad::STLParser::MAX_RECOMMENDED_TRIANGLES) {
        ss << ",\n  \"triangles\": [\n";
        for (size_t i = 0; i < model.triangles.size(); ++i) {
            const auto& t = model.triangles[i];
            ss << "    ["
               << t.normal.x << "," << t.normal.y << "," << t.normal.z << ","
               << t.v1.x << "," << t.v1.y << "," << t.v1.z << ","
               << t.v2.x << "," << t.v2.y << "," << t.v2.z << ","
               << t.v3.x << "," << t.v3.y << "," << t.v3.z << "]"
               << (i + 1 < model.triangles.size() ? ",\n" : "\n");
        }
        ss << "  ]\n";
    } else {
        ss << "\n";
    }
    ss << "}";
    return ss.str();
}

inline std::string serialize_ansys(const ANSYSDataSet& ds, bool include_nodes = true) {
    std::ostringstream ss;
    ss << std::setprecision(10);
    ss << "{\n";
    ss << "  \"node_count\": " << ds.nodes.size() << ",\n";
    ss << "  \"file_size_bytes\": " << ds.file_size_bytes << ",\n";
    ss << "  \"is_valid\": " << (ds.is_valid ? "true" : "false") << ",\n";
    ss << "  \"error_message\": \"" << escape_string(ds.error_message) << "\",\n";
    ss << "  \"min_temperature\": " << ds.min_temperature << ",\n";
    ss << "  \"max_temperature\": " << ds.max_temperature << ",\n";
    ss << "  \"mean_temperature\": " << ds.mean_temperature << ",\n";
    ss << "  \"bbox_min\": {\"x\": " << ds.bbox_min.x << ", \"y\": " << ds.bbox_min.y << ", \"z\": " << ds.bbox_min.z << "},\n";
    ss << "  \"bbox_max\": {\"x\": " << ds.bbox_max.x << ", \"y\": " << ds.bbox_max.y << ", \"z\": " << ds.bbox_max.z << "}";

    if (include_nodes && ds.is_valid) {
        ss << ",\n  \"nodes\": [\n";
        for (size_t i = 0; i < ds.nodes.size(); ++i) {
            const auto& n = ds.nodes[i];
            ss << "    [" << n.node_id << "," << n.x << "," << n.y << "," << n.z << "," << n.temperature << "]"
               << (i + 1 < ds.nodes.size() ? ",\n" : "\n");
        }
        ss << "  ]\n";
    } else {
        ss << "\n";
    }
    ss << "}";
    return ss.str();
}

inline std::string serialize_fea_result(const FEAResult& res, const std::vector<ConvergenceEntry>& conv = {}) {
    std::ostringstream ss;
    ss << std::setprecision(10);
    ss << "{\n";
    ss << "  \"success\": " << (res.success ? "true" : "false") << ",\n";
    ss << "  \"error_message\": \"" << escape_string(res.error_message) << "\",\n";
    ss << "  \"config\": {\n";
    ss << "    \"L\": " << res.config.L << ",\n";
    ss << "    \"A\": " << res.config.A << ",\n";
    ss << "    \"k\": " << res.config.k << ",\n";
    ss << "    \"q\": " << res.config.q << ",\n";
    ss << "    \"T0\": " << res.config.T0 << ",\n";
    ss << "    \"TL\": " << res.config.TL << ",\n";
    ss << "    \"num_elements\": " << res.config.num_elements << "\n";
    ss << "  },\n";
    ss << "  \"h\": " << res.h << ",\n";
    ss << "  \"max_midpoint_error\": " << res.max_midpoint_error << ",\n";
    ss << "  \"max_nodal_error\": " << res.max_nodal_error << ",\n";

    // Nodal values
    ss << "  \"nodes\": [\n";
    for (size_t i = 0; i < res.node_coords.size(); ++i) {
        ss << "    {\"x\": " << res.node_coords[i]
           << ", \"T_fea\": " << res.nodal_temperatures[i]
           << "}" << (i + 1 < res.node_coords.size() ? ",\n" : "\n");
    }
    ss << "  ],\n";

    // Midpoint evaluations
    ss << "  \"midpoints\": [\n";
    for (size_t i = 0; i < res.midpoint_evals.size(); ++i) {
        const auto& p = res.midpoint_evals[i];
        ss << "    {\"x\": " << p.x
           << ", \"T_fea\": " << p.T_fea
           << ", \"T_exact\": " << p.T_exact
           << ", \"error\": " << p.error
           << "}" << (i + 1 < res.midpoint_evals.size() ? ",\n" : "\n");
    }
    ss << "  ]";

    // Convergence table
    if (!conv.empty()) {
        ss << ",\n  \"convergence\": [\n";
        for (size_t i = 0; i < conv.size(); ++i) {
            const auto& c = conv[i];
            ss << "    {\"N\": " << c.N
               << ", \"h\": " << c.h
               << ", \"max_midpoint_error\": " << c.max_midpoint_error
               << ", \"theoretical_error\": " << c.theoretical_error
               << ", \"convergence_rate\": " << c.convergence_rate
               << "}" << (i + 1 < conv.size() ? ",\n" : "\n");
        }
        ss << "  ]\n";
    } else {
        ss << "\n";
    }

    ss << "}";
    return ss.str();
}

inline std::string serialize_benchmark_result(const BenchmarkResult& res) {
    std::ostringstream ss;
    ss << std::setprecision(8);
    ss << "{\n";
    ss << "  \"cpu_model\": \"" << escape_string(res.cpu_model) << "\",\n";
    ss << "  \"logical_cpus\": " << res.logical_cpus << ",\n";
    ss << "  \"physical_cpus\": " << res.physical_cpus << ",\n";
    ss << "  \"compiler_info\": \"" << escape_string(res.compiler_info) << "\",\n";
    ss << "  \"optimization_flags\": \"" << escape_string(res.optimization_flags) << "\",\n";
    ss << "  \"launch_to_ready_ms\": " << res.launch_to_ready_ms << ",\n";
    ss << "  \"total_memory_bytes\": " << res.total_memory_bytes << ",\n";
    ss << "  \"num_cases\": " << res.config.num_cases << ",\n";
    ss << "  \"mesh_size\": " << res.config.mesh_size << ",\n";
    ss << "  \"seed\": " << res.config.seed << ",\n";
    ss << "  \"repetitions\": " << res.config.repetitions << ",\n";
    ss << "  \"warm_up_per_worker\": " << (res.config.warm_up ? "true" : "false") << ",\n";
    ss << "  \"serial_parallel_max_diff\": " << res.serial_parallel_max_diff << ",\n";
    ss << "  \"verification_passed\": " << (res.verification_passed ? "true" : "false") << ",\n";
    ss << "  \"verification_workers\": " << res.verification_workers << ",\n";
    ss << "  \"speedup_assessable\": " << (res.speedup_assessable ? "true" : "false") << ",\n";
    ss << "  \"measurements\": [\n";
    for (size_t i = 0; i < res.measurements.size(); ++i) {
        const auto& m = res.measurements[i];
        ss << "    {\n";
        ss << "      \"workers\": " << m.workers << ",\n";
        ss << "      \"median_time_ms\": " << m.median_time_ms << ",\n";
        ss << "      \"speedup\": " << m.speedup << ",\n";
        ss << "      \"efficiency\": " << m.efficiency << ",\n";
        ss << "      \"run_times_ms\": [";
        for (size_t j = 0; j < m.run_times_ms.size(); ++j) {
            ss << m.run_times_ms[j] << (j + 1 < m.run_times_ms.size() ? ", " : "");
        }
        ss << "]\n";
        ss << "    }" << (i + 1 < res.measurements.size() ? ",\n" : "\n");
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

inline std::string serialize_backprop_result(const BackpropResult& res) {
    std::ostringstream ss;
    ss << std::setprecision(10);
    ss << "{\n";
    ss << "  \"initial\": {\"x\": " << res.initial_x << ", \"y\": " << res.initial_y
       << ", \"w\": " << res.initial_w << ", \"b\": " << res.initial_b
       << ", \"v\": " << res.initial_v << ", \"c\": " << res.initial_c
       << ", \"learning_rate\": " << res.learning_rate << "},\n";
    ss << "  \"step0\": {\n";
    ss << "    \"z\": " << res.step0_z << ",\n";
    ss << "    \"h\": " << res.step0_h << ",\n";
    ss << "    \"y_hat\": " << res.step0_y_hat << ",\n";
    ss << "    \"loss\": " << res.step0_loss << ",\n";
    ss << "    \"gradients\": {\"w\": " << res.grad_w << ", \"b\": " << res.grad_b
       << ", \"v\": " << res.grad_v << ", \"c\": " << res.grad_c << "},\n";
    ss << "    \"finite_diff\": {\"w\": " << res.fd_grad_w << ", \"b\": " << res.fd_grad_b
       << ", \"v\": " << res.fd_grad_v << ", \"c\": " << res.fd_grad_c << "},\n";
    ss << "    \"fd_errors\": {\"w\": " << res.fd_err_w << ", \"b\": " << res.fd_err_b
       << ", \"v\": " << res.fd_err_v << ", \"c\": " << res.fd_err_c << "},\n";
    ss << "    \"fd_check_passed\": " << (res.fd_check_passed ? "true" : "false") << "\n";
    ss << "  },\n";
    ss << "  \"step1\": {\"w\": " << res.w_step1 << ", \"b\": " << res.b_step1
       << ", \"v\": " << res.v_step1 << ", \"c\": " << res.c_step1 << "},\n";
    ss << "  \"history\": [\n";
    for (size_t i = 0; i < res.history.size(); ++i) {
        const auto& s = res.history[i];
        ss << "    {\"step\": " << s.step
           << ", \"w\": " << s.w
           << ", \"b\": " << s.b
           << ", \"v\": " << s.v
           << ", \"c\": " << s.c
           << ", \"y_hat\": " << s.y_hat
           << ", \"loss\": " << s.loss
           << "}" << (i + 1 < res.history.size() ? ",\n" : "\n");
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

} // namespace json
} // namespace workbench

