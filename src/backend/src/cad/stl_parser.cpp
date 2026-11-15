#include "workbench/cad/stl_parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>

namespace workbench {
namespace cad {

namespace {

// Helper: Trim leading and trailing whitespace
std::string trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }
    if (start == str.size()) return "";
    size_t end = str.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
        end--;
    }
    return str.substr(start, end - start + 1);
}

// Helper: Case-insensitive lowercase
std::string to_lower(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return res;
}

std::uint32_t read_u32_le(const char* bytes) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3])) << 24);
}

float read_f32_le(const char* bytes) {
    unsigned char ordered[4] = {
        static_cast<unsigned char>(bytes[0]), static_cast<unsigned char>(bytes[1]),
        static_cast<unsigned char>(bytes[2]), static_cast<unsigned char>(bytes[3])
    };
    const std::uint16_t endian_probe = 1;
    if (*reinterpret_cast<const unsigned char*>(&endian_probe) != 1) {
        std::reverse(ordered, ordered + 4);
    }
    float value = 0.0f;
    std::memcpy(&value, ordered, sizeof(value));
    return value;
}

bool parse_binary_stl(const std::string& content, const std::string& source_name, STLModel& model) {
    if (content.size() < 84) return false;
    const std::uint32_t triangle_count = read_u32_le(content.data() + 80);
    const std::uint64_t expected_size = 84ULL + static_cast<std::uint64_t>(triangle_count) * 50ULL;
    if (expected_size != content.size()) return false;

    model.file_size_bytes = content.size();
    std::string header;
    header.reserve(80);
    for (size_t i = 0; i < 80 && content[i] != '\0'; ++i) {
        const unsigned char ch = static_cast<unsigned char>(content[i]);
        header.push_back(ch >= 32 && ch <= 126 ? static_cast<char>(ch) : ' ');
    }
    model.solid_name = trim(header);
    if (model.solid_name.empty()) model.solid_name = trim(source_name);
    if (model.solid_name.empty()) model.solid_name = "binary_stl";

    if (triangle_count == 0) {
        model.error_message = "Validation Error: STL file contains 0 triangles. Solid has no geometry.";
        return true;
    }
    if (triangle_count > STLParser::MAX_RECOMMENDED_TRIANGLES) {
        model.error_message = "Rendering Limit Exceeded: STL models are limited to 200,000 triangles in the interactive viewer.";
        return true;
    }

    model.triangles.reserve(triangle_count);
    size_t offset = 84;
    for (std::uint32_t i = 0; i < triangle_count; ++i, offset += 50) {
        const char* record = content.data() + offset;
        Triangle triangle;
        triangle.normal = {read_f32_le(record), read_f32_le(record + 4), read_f32_le(record + 8)};
        triangle.v1 = {read_f32_le(record + 12), read_f32_le(record + 16), read_f32_le(record + 20)};
        triangle.v2 = {read_f32_le(record + 24), read_f32_le(record + 28), read_f32_le(record + 32)};
        triangle.v3 = {read_f32_le(record + 36), read_f32_le(record + 40), read_f32_le(record + 44)};
        if (!triangle.is_finite()) {
            model.error_message = "Validation Error: Binary STL contains a non-finite normal or vertex coordinate.";
            return true;
        }
        if (triangle.normal.norm() < 1e-12) {
            triangle.normal = (triangle.v2 - triangle.v1).cross(triangle.v3 - triangle.v1).normalized();
        }
        model.triangles.push_back(triangle);
    }
    model.update_bounds();
    model.is_valid = true;
    return true;
}

} // anonymous namespace

STLModel STLParser::parse_file(const std::string& filepath) {
    STLModel model;
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        model.is_valid = false;
        model.error_message = "File Error: Unable to open file at path: " + filepath;
        return model;
    }

    auto file_size = file.tellg();
    if (file_size == 0) {
        model.is_valid = false;
        model.file_size_bytes = 0;
        model.error_message = "File Error: File is completely empty (0 bytes). A valid STL must contain triangle facets.";
        return model;
    }

    if (static_cast<size_t>(file_size) > MAX_TESTED_FILE_SIZE_BYTES) {
        model.is_valid = false;
        model.file_size_bytes = static_cast<size_t>(file_size);
        model.error_message = "File Limit Exceeded: File size (" + std::to_string(file_size / (1024 * 1024)) +
                              " MiB) exceeds the 150 MiB STL limit.";
        return model;
    }

    file.seekg(0, std::ios::beg);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    model.file_size_bytes = content.size();

    return parse_string(content, filepath);
}

STLModel STLParser::parse_string(const std::string& content, const std::string& source_name) {
    STLModel model;
    model.file_size_bytes = content.size();
    model.is_valid = false;

    if (content.empty()) {
        model.error_message = "Parse Error: Input content is empty (0 bytes). Expected ASCII or binary STL data.";
        return model;
    }
    if (content.size() > MAX_TESTED_FILE_SIZE_BYTES) {
        model.error_message = "File Limit Exceeded: Input content exceeds the 150 MiB STL limit.";
        return model;
    }

    // Binary STL uses an 80-byte header, a facet count, then 50 bytes per facet.
    // Exact length also handles binary headers that happen to begin with "solid".
    if (parse_binary_stl(content, source_name, model)) return model;

    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;

    // Read first non-empty line
    bool found_solid = false;
    while (std::getline(stream, line)) {
        line_number++;
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        std::string lower = to_lower(trimmed);
        if (lower.rfind("solid", 0) == 0) {
            found_solid = true;
            if (trimmed.size() > 5) {
                model.solid_name = trim(trimmed.substr(5));
            } else {
                model.solid_name = "unnamed_solid";
            }
            break;
        } else {
            model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                  "]: Expected 'solid [name]' header, found: '" + trimmed + "'. Not a valid ASCII STL.";
            return model;
        }
    }

    if (!found_solid) {
        model.error_message = "Parse Error: File contains only whitespace or no 'solid' definition.";
        return model;
    }

    Triangle current_tri;
    int vertex_count = 0;
    bool in_facet = false;
    bool in_loop = false;
    bool found_endsolid = false;

    while (std::getline(stream, line)) {
        line_number++;
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        std::string lower = to_lower(trimmed);

        if (lower.rfind("endsolid", 0) == 0) {
            found_endsolid = true;
            break;
        } else if (lower.rfind("facet normal", 0) == 0) {
            if (in_facet) {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: Unexpected 'facet normal' inside an unclosed facet.";
                return model;
            }
            in_facet = true;
            vertex_count = 0;

            std::istringstream ls(trimmed.substr(12));
            if (!(ls >> current_tri.normal.x >> current_tri.normal.y >> current_tri.normal.z)) {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: Malformed normal coordinates in 'facet normal'.";
                return model;
            }
            if (!current_tri.normal.is_finite()) {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: Non-finite normal vector (NaN or Inf detected).";
                return model;
            }
        } else if (lower.rfind("outer loop", 0) == 0) {
            if (!in_facet || in_loop) {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: Unexpected 'outer loop' keyword.";
                return model;
            }
            in_loop = true;
            vertex_count = 0;
        } else if (lower.rfind("vertex", 0) == 0) {
            if (!in_loop) {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: 'vertex' found outside 'outer loop'.";
                return model;
            }
            Vec3 v;
            std::istringstream ls(trimmed.substr(6));
            if (!(ls >> v.x >> v.y >> v.z)) {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: Malformed vertex coordinates in 'vertex'.";
                return model;
            }
            if (!v.is_finite()) {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: Non-finite vertex coordinate (NaN or Inf detected).";
                return model;
            }

            if (vertex_count == 0) current_tri.v1 = v;
            else if (vertex_count == 1) current_tri.v2 = v;
            else if (vertex_count == 2) current_tri.v3 = v;
            else {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: More than 3 vertices found in triangle loop.";
                return model;
            }
            vertex_count++;
        } else if (lower.rfind("endloop", 0) == 0) {
            if (!in_loop || vertex_count != 3) {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: 'endloop' encountered but loop had " + std::to_string(vertex_count) + " vertices (expected 3).";
                return model;
            }
            in_loop = false;
        } else if (lower.rfind("endfacet", 0) == 0) {
            if (!in_facet || in_loop) {
                model.error_message = "Parse Error [line " + std::to_string(line_number) +
                                      "]: Unexpected 'endfacet' keyword.";
                return model;
            }
            in_facet = false;

            // If normal was zero vector (common in some exporters), compute from vertices
            if (current_tri.normal.norm() < 1e-12) {
                Vec3 edge1 = current_tri.v2 - current_tri.v1;
                Vec3 edge2 = current_tri.v3 - current_tri.v1;
                current_tri.normal = edge1.cross(edge2).normalized();
            }

            if (model.triangles.size() >= MAX_RECOMMENDED_TRIANGLES) {
                model.error_message = "Rendering Limit Exceeded: STL models are limited to 200,000 triangles in the interactive viewer.";
                return model;
            }
            model.triangles.push_back(current_tri);
        }
    }

    if (!found_endsolid) {
        model.error_message = "Parse Error: ASCII STL is missing the required 'endsolid' terminator.";
        return model;
    }
    if (in_facet || in_loop) {
        model.error_message = "Parse Error: File ended prematurely inside an unclosed facet or loop.";
        return model;
    }

    if (model.triangles.empty()) {
        model.error_message = "Validation Error: STL file contains 0 triangles. Solid has no geometry.";
        return model;
    }

    model.update_bounds();
    model.is_valid = true;
    return model;
}

} // namespace cad
} // namespace workbench
