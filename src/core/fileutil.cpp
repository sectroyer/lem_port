// SPDX-License-Identifier: GPL-3.0-or-later
// File lookups without std::filesystem, which is unavailable before iOS 13.
#include <cctype>
#include <string>

#ifdef _WIN32
#include <filesystem>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "graphicset.h"

namespace lem {

namespace {

bool equalsNoCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(uint8_t(a[i])) != std::tolower(uint8_t(b[i]))) return false;
    return true;
}

} // namespace

bool fileExists(const std::string& path) {
#ifdef _WIN32
    std::error_code ec;
    return std::filesystem::exists(path, ec);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0;
#endif
}

std::string joinPath(const std::string& dir, const std::string& file) {
    std::string p = dir;
    if (!p.empty() && p.back() != '/' && p.back() != '\\') p += '/';
    if (fileExists(p + file)) return p + file;
    // Data copied from old media is often upper case; match case-insensitively.
#ifdef _WIN32
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
        if (equalsNoCase(e.path().filename().string(), file)) return e.path().string();
#else
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            if (equalsNoCase(e->d_name, file)) {
                std::string found = p + e->d_name;
                closedir(d);
                return found;
            }
        }
        closedir(d);
    }
#endif
    return p + file;
}

} // namespace lem
