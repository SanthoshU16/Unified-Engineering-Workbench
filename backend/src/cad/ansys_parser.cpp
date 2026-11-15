#include "workbench/cad/ansys_parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace workbench {
namespace cad {

namespace {

std::string trim_str(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && (std::isspace(static_cast<unsigned char>(str[start])) || str[start] == '\r')) {
        start++;
    }
    if (start == str.size()) return "";
    size_t end = str.size() - 1;
    while (end > start && (std::isspace(static_cast<unsigned char>(str[end])) || str[end] == '\r')) {
        end--;
    }
    return str.substr(start, end - start + 1);
}

std::string remove_quotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            in_quotes = !in_quotes;
            token += c;
        } else if (c == ',' && !in_quotes) {
            tokens.push_back(trim_str(remove_quotes(trim_str(token))));
            token.clear();
        } else {
            token += c;
        }
    }
    tokens.push_back(trim_str(remove_quotes(trim_str(token))));
    return tokens;
}

std::string to_lower_str(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return res;
}

} // anonymous namespace

ANSYSDataSet ANSYSParser::parse_file(const std::string& filepath) {
    ANSYSDataSet ds;
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ds.is_valid = false;
        ds.error_message = "File Error: Unable to open ANSYS CSV file at path: " + filepath;
        return ds;
    }

    auto file_size = file.tellg();
    if (file_size == 0) {
        ds.is_valid = false;
        ds.file_size_bytes = 0;
        ds.error_message = "File Error: CSV file is completely empty (0 bytes). Expected ANSYS nodal temperature export.";
        return ds;
    }

    if (static_cast<size_t>(file_size) > MAX_SUPPORTED_FILE_SIZE_BYTES) {
        ds.is_valid = false;
        ds.file_size_bytes = static_cast<size_t>(file_size);
        ds.error_message = "File Limit Exceeded: File size (" + std::to_string(file_size / (1024 * 1024)) +
                           " MiB) exceeds the supported limit of 50 MiB.";
        return ds;
    }

    file.seekg(0, std::ios::beg);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    ds.file_size_bytes = content.size();

    return parse_string(content, filepath);
}

ANSYSDataSet ANSYSParser::parse_string(const std::string& content, const std::string& /*source_name*/) {
    ANSYSDataSet ds;
    ds.file_size_bytes = content.size();
    ds.is_valid = false;

    if (content.empty()) {
        ds.error_message = "Parse Error: Input CSV content is empty (0 bytes). Expected headers: node_id,x_m,y_m,z_m,temperature_C.";
        return ds;
    }
    if (content.size() > MAX_SUPPORTED_FILE_SIZE_BYTES) {
        ds.error_message = "File Limit Exceeded: Input content exceeds the 50 MiB parser limit.";
        return ds;
    }

    std::istringstream stream(content);
    std::string line;
    size_t line_num = 0;

    // Read header line
    bool found_header = false;
    int col_id = -1, col_x = -1, col_y = -1, col_z = -1, col_temp = -1;

    while (std::getline(stream, line)) {
        line_num++;
        std::string trimmed = trim_str(line);
        if (trimmed.empty() || trimmed[0] == '#') continue; // Allow comment lines

        auto tokens = split_csv_line(trimmed);
        if (tokens.size() < 5) {
            ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                               "]: Header must contain at least 5 columns (node_id, x_m, y_m, z_m, temperature_C). Found " +
                               std::to_string(tokens.size()) + " columns.";
            return ds;
        }

        for (size_t i = 0; i < tokens.size(); ++i) {
            std::string t = to_lower_str(tokens[i]);
            if (t == "node_id" || t == "node" || t == "id") col_id = static_cast<int>(i);
            else if (t == "x_m" || t == "x" || t == "x [m]") col_x = static_cast<int>(i);
            else if (t == "y_m" || t == "y" || t == "y [m]") col_y = static_cast<int>(i);
            else if (t == "z_m" || t == "z" || t == "z [m]") col_z = static_cast<int>(i);
            else if (t == "temperature_c" || t == "temperature" || t == "temp" || t == "temp_c" || t == "temperature [c]") col_temp = static_cast<int>(i);
        }

        if (col_id != -1 && col_x != -1 && col_y != -1 && col_z != -1 && col_temp != -1) {
            found_header = true;
            break;
        } else {
            ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                               "]: Missing required column headers. Expected: node_id, x_m, y_m, z_m, temperature_C. Received: " + trimmed;
            return ds;
        }
    }

    if (!found_header) {
        ds.error_message = "Parse Error: Could not locate a valid CSV header with node_id, x_m, y_m, z_m, temperature_C.";
        return ds;
    }

    std::unordered_set<int64_t> seen_node_ids;
    seen_node_ids.reserve(10000);

    while (std::getline(stream, line)) {
        line_num++;
        std::string trimmed = trim_str(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        auto tokens = split_csv_line(trimmed);
        if (tokens.size() <= static_cast<size_t>(std::max({col_id, col_x, col_y, col_z, col_temp}))) {
            ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                               "]: Incomplete row. Expected at least 5 columns, found " + std::to_string(tokens.size()) + ".";
            return ds;
        }

        ANSYSNode node;
        try {
            size_t idx = 0;
            node.node_id = std::stoll(tokens[col_id], &idx);
            if (idx != tokens[col_id].size()) {
                ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                                   "]: Malformed integer for node_id: '" + tokens[col_id] + "'.";
                return ds;
            }
        } catch (...) {
            ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                               "]: Invalid or non-integer node_id: '" + tokens[col_id] + "'.";
            return ds;
        }

        if (node.node_id <= 0) {
            ds.error_message = "Validation Error [line " + std::to_string(line_num) +
                               "]: node_id must be a positive integer.";
            return ds;
        }

        if (seen_node_ids.find(node.node_id) != seen_node_ids.end()) {
            ds.error_message = "Validation Error [line " + std::to_string(line_num) +
                               "]: Duplicate node_id detected: " + std::to_string(node.node_id) + ". Node IDs must be unique.";
            return ds;
        }
        seen_node_ids.insert(node.node_id);

        try {
            size_t idx = 0;
            node.x = std::stod(tokens[col_x], &idx);
            if (idx != tokens[col_x].size()) throw std::runtime_error("Trailing characters");
        } catch (...) {
            ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                               "]: Malformed floating point value for x_m: '" + tokens[col_x] + "'.";
            return ds;
        }

        try {
            size_t idx = 0;
            node.y = std::stod(tokens[col_y], &idx);
            if (idx != tokens[col_y].size()) throw std::runtime_error("Trailing characters");
        } catch (...) {
            ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                               "]: Malformed floating point value for y_m: '" + tokens[col_y] + "'.";
            return ds;
        }

        try {
            size_t idx = 0;
            node.z = std::stod(tokens[col_z], &idx);
            if (idx != tokens[col_z].size()) throw std::runtime_error("Trailing characters");
        } catch (...) {
            ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                               "]: Malformed floating point value for z_m: '" + tokens[col_z] + "'.";
            return ds;
        }

        try {
            size_t idx = 0;
            node.temperature = std::stod(tokens[col_temp], &idx);
            if (idx != tokens[col_temp].size()) throw std::runtime_error("Trailing characters");
        } catch (...) {
            ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                               "]: Malformed floating point value for temperature_C: '" + tokens[col_temp] + "'.";
            return ds;
        }

        if (!node.is_finite()) {
            ds.error_message = "Parse Error [line " + std::to_string(line_num) +
                               "]: Non-finite data detected (NaN or Inf in coordinates or temperature).";
            return ds;
        }

        if (ds.nodes.size() >= MAX_RECOMMENDED_NODES) {
            ds.error_message = "Rendering Limit Exceeded: CSV point clouds are limited to 200,000 nodes in the interactive viewer.";
            return ds;
        }
        ds.nodes.push_back(node);
    }

    if (ds.nodes.empty()) {
        ds.error_message = "Validation Error: CSV file contains valid header but 0 data rows.";
        return ds;
    }

    ds.compute_statistics();
    ds.is_valid = true;
    return ds;
}

} // namespace cad
} // namespace workbench
