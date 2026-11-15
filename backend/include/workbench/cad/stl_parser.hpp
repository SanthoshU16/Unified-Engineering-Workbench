#pragma once

#include "workbench/common/types.hpp"
#include <string>

namespace workbench {
namespace cad {

class STLParser {
public:
    // Parse an ASCII or binary STL file from disk
    static STLModel parse_file(const std::string& filepath);

    // Parse ASCII or binary STL content from a byte string buffer
    static STLModel parse_string(const std::string& content, const std::string& source_name = "buffer");

    // Maximum accepted STL file size (150 MiB)
    static constexpr size_t MAX_TESTED_FILE_SIZE_BYTES = 150 * 1024 * 1024;
    // Keep parser, response, and interactive viewer memory bounded for both encodings.
    static constexpr size_t MAX_RECOMMENDED_TRIANGLES = 200000;
};

} // namespace cad
} // namespace workbench

