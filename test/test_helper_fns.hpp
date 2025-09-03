#pragma once
#include <fstream>
#include <sys/stat.h>
#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

using json = nlohmann::json;

// Helper: check file existence
inline bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

// Helper: load JSON config
inline json loadJson(const std::string& path) {
    std::ifstream f(path);
    if (!f.good()) {
        throw std::runtime_error("Config file not readable: " + path);
    }
    json j;
    f >> j;
    return j;
}
