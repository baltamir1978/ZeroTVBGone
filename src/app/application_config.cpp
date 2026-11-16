/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "application_config.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <vector>

namespace app {
namespace {

std::string trim(std::string value) {
    const auto is_not_space = [](unsigned char character) {
        return !std::isspace(character);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), is_not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), is_not_space).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool parse_bool(const std::string& value, bool& result) {
    const auto normalized = lowercase(trim(value));
    if (normalized == "yes" || normalized == "true" || normalized == "1") {
        result = true;
        return true;
    }
    if (normalized == "no" || normalized == "false" || normalized == "0") {
        result = false;
        return true;
    }
    return false;
}

bool is_section(const std::string& line, const std::string& expected) {
    const auto normalized = trim(line);
    return normalized.size() >= 2 && normalized.front() == '[' && normalized.back() == ']' &&
           lowercase(trim(normalized.substr(1, normalized.size() - 2))) == expected;
}

bool is_key(const std::string& line, const std::string& expected) {
    const auto separator = line.find('=');
    return separator != std::string::npos &&
           lowercase(trim(line.substr(0, separator))) == expected;
}

} // namespace

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "could not open file";
        return false;
    }

    std::string section;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = lowercase(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = lowercase(trim(line.substr(0, separator)));
        if (section == "application" && key == "dark_mode") {
            if (!parse_bool(line.substr(separator + 1), config.dark_mode)) {
                error = "invalid dark_mode value at line " + std::to_string(line_number);
                return false;
            }
        }
        // Una región que no se entiende no impide arrancar: se queda la de por defecto.
        if (section == "application" && key == "region") {
            tvbgone::parse_region(lowercase(trim(line.substr(separator + 1))), config.region);
        }
    }

    return true;
}

bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error) {
    error.clear();

    const std::filesystem::path config_path(path);
    const auto parent = config_path.parent_path();
    if (!parent.empty()) {
        std::error_code filesystem_error;
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) {
            error = "could not create parent directory: " + filesystem_error.message();
            return false;
        }
    }

    std::vector<std::string> lines;
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }

    // Se reescriben sólo las claves de la app dentro de [application]; lo demás que
    // hubiera en el fichero (comentarios, otras secciones) se deja como estaba.
    struct Setting {
        std::string key;
        std::string line;
        bool written;
    };
    std::vector<Setting> settings = {
        {"dark_mode", config.dark_mode ? "dark_mode=yes" : "dark_mode=no", false},
        {"region", std::string("region=") + tvbgone::region_id(config.region), false},
    };
    const auto write_missing = [&settings](std::vector<std::string>& out) {
        for (auto& setting : settings) {
            if (!setting.written) {
                out.push_back(setting.line);
                setting.written = true;
            }
        }
    };

    std::vector<std::string> updated;
    bool in_application_section = false;
    bool found_application_section = false;

    for (const auto& current_line : lines) {
        const auto normalized = trim(current_line);
        const bool section_header = normalized.size() >= 2 && normalized.front() == '[' &&
                                    normalized.back() == ']';
        if (section_header) {
            if (in_application_section) {
                write_missing(updated);
            }
            in_application_section = is_section(current_line, "application");
            found_application_section = found_application_section || in_application_section;
        }

        bool replaced = false;
        if (in_application_section) {
            for (auto& setting : settings) {
                if (is_key(current_line, setting.key)) {
                    if (!setting.written) {
                        updated.push_back(setting.line);
                        setting.written = true;
                    }
                    replaced = true;
                    break;
                }
            }
        }
        if (!replaced) {
            updated.push_back(current_line);
        }
    }

    if (!found_application_section) {
        if (!updated.empty() && !updated.back().empty()) {
            updated.emplace_back();
        }
        updated.emplace_back("[application]");
    }
    // Si [application] no era la última sección, lo que falte queda al final del fichero
    // bajo otra: se repite la cabecera para que siga siendo suya.
    else if (!in_application_section &&
             std::any_of(settings.begin(), settings.end(), [](const Setting& s) { return !s.written; })) {
        updated.emplace_back("[application]");
    }
    write_missing(updated);

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = "could not open file for writing";
        return false;
    }
    for (const auto& updated_line : updated) {
        output << updated_line << '\n';
    }
    if (!output) {
        error = "failed while writing file";
        return false;
    }

    return true;
}

} // namespace app
