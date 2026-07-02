#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

std::vector<uint8_t> readFileBinary(const std::filesystem::path &path);
