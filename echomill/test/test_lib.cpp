#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <string>

namespace echomill {

inline std::string find_data_dir()
{
    namespace fs = std::filesystem;
    fs::path current = fs::current_path();
    while (!current.empty() && current != "/") {
        if (fs::exists(current / "data")) {
            return (current / "data").string();
        }
        current = current.parent_path();
    }
    return "data";
}

} // namespace echomill
