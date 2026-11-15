#pragma once

#include "workbench/common/types.hpp"
#include <string>

namespace workbench {
namespace cad {

class ANSYSParser {
public:
    // Parse an ANSYS CSV nodal temperature export file from disk
    static ANSYSDataSet parse_file(const std::string& filepath);

    // Parse ANSYS CSV content from a string buffer
    static ANSYSDataSet parse_string(const std::string& content, const std::string& source_name = "buffer");

    // Maximum supported file size (50 MiB), enforced by file and string parsing.
    static constexpr size_t MAX_SUPPORTED_FILE_SIZE_BYTES = 50 * 1024 * 1024;
    // Maximum allowable nodes for smooth interactive rendering
    static constexpr size_t MAX_RECOMMENDED_NODES = 200000;
};

} // namespace cad
} // namespace workbench

