#include "file.h"

#include <fstream>

std::vector<uint8_t> readFileBinary(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (not file) {
    return {};
  }

  file.seekg(0, std::ios::end);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> res(size);
  if (not file.read((char *)res.data(), size)) {
    return {};
  }

  return res;
}
