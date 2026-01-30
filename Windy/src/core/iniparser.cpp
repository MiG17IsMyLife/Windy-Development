#define _CRT_SECURE_NO_WARNINGS

#include "iniparser.h"
#include "log.h"

#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <algorithm>

// ============================================================
// Constructor / Destructor
// ============================================================

IniParser::IniParser() {
}

IniParser::~IniParser() {
    Clear();
}

// ============================================================
// String Utilities
// ============================================================

std::string IniParser::TrimString(const std::string& str) {
    if (str.empty()) return str;

    size_t start = 0;
    size_t end = str.length();

    // Trim leading whitespace
    while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }

    // Trim trailing whitespace
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        end--;
    }

    return str.substr(start, end - start);
}

// ============================================================
// File Operations
// ============================================================

bool IniParser::Load(const char* filename) {
    if (!filename) {
        log_error("IniParser::Load: filename is null");
        return false;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        log_warn("IniParser::Load: Cannot open file '%s'", filename);
        return false;
    }

    // Clear existing data
    Clear();
    m_loadedPath = filename;

    std::string line;
    IniSection* currentSection = nullptr;

    while (std::getline(file, line)) {
        std::string trimmedLine = TrimString(line);

        // Skip empty lines and comments
        if (trimmedLine.empty() || trimmedLine[0] == ';' || trimmedLine[0] == '#') {
            continue;
        }

        // Check for section header [SectionName]
        if (trimmedLine[0] == '[' && trimmedLine.back() == ']') {
            // Extract section name
            std::string sectionName = trimmedLine.substr(1, trimmedLine.length() - 2);
            sectionName = TrimString(sectionName);

            // Add new section
            IniSection newSection;
            newSection.name = sectionName;
            m_sections.push_back(newSection);
            currentSection = &m_sections.back();

            log_trace("IniParser: Found section [%s]", sectionName.c_str());
        }
        // Check for key=value pair
        else if (currentSection != nullptr) {
            size_t eqPos = trimmedLine.find('=');
            if (eqPos != std::string::npos) {
                std::string key = TrimString(trimmedLine.substr(0, eqPos));
                std::string value = TrimString(trimmedLine.substr(eqPos + 1));

                IniKeyValuePair pair;
                pair.key = key;
                pair.value = value;
                currentSection->pairs.push_back(pair);

                log_trace("IniParser: [%s] %s = %s", 
                    currentSection->name.c_str(), key.c_str(), value.c_str());
            }
        }
    }

    file.close();
    log_info("IniParser: Loaded '%s' with %zu sections", filename, m_sections.size());
    return true;
}

bool IniParser::Save(const char* filename) {
    const char* savePath = filename ? filename : m_loadedPath.c_str();
    
    if (!savePath || savePath[0] == '\0') {
        log_error("IniParser::Save: No filename specified");
        return false;
    }

    std::ofstream file(savePath);
    if (!file.is_open()) {
        log_error("IniParser::Save: Cannot open file '%s' for writing", savePath);
        return false;
    }

    for (size_t i = 0; i < m_sections.size(); i++) {
        const IniSection& section = m_sections[i];
        
        // Write section header
        file << "[" << section.name << "]\n";

        // Write key-value pairs
        for (const auto& pair : section.pairs) {
            file << pair.key << " = " << pair.value << "\n";
        }

        // Add blank line between sections (except last)
        if (i < m_sections.size() - 1) {
            file << "\n";
        }
    }

    file.close();
    log_info("IniParser: Saved to '%s'", savePath);
    return true;
}

// ============================================================
// Section Access
// ============================================================

IniSection* IniParser::GetSection(const char* sectionName) {
    if (!sectionName) return nullptr;

    for (auto& section : m_sections) {
        if (section.name == sectionName) {
            return &section;
        }
    }
    return nullptr;
}

const IniSection* IniParser::GetSection(const char* sectionName) const {
    if (!sectionName) return nullptr;

    for (const auto& section : m_sections) {
        if (section.name == sectionName) {
            return &section;
        }
    }
    return nullptr;
}

bool IniParser::HasSection(const char* sectionName) const {
    return GetSection(sectionName) != nullptr;
}

bool IniParser::HasKey(const char* sectionName, const char* key) const {
    const IniSection* section = GetSection(sectionName);
    if (!section || !key) return false;

    for (const auto& pair : section->pairs) {
        if (pair.key == key) {
            return true;
        }
    }
    return false;
}

// ============================================================
// Value Getters
// ============================================================

const char* IniParser::GetValue(const char* sectionName, const char* key) const {
    const IniSection* section = GetSection(sectionName);
    if (!section || !key) return "";

    for (const auto& pair : section->pairs) {
        if (pair.key == key) {
            return pair.value.c_str();
        }
    }
    return "";
}

int IniParser::GetInt(const char* sectionName, const char* key, int defaultValue) const {
    const char* value = GetValue(sectionName, key);
    if (!value || value[0] == '\0') {
        return defaultValue;
    }

    // Handle hex values (0x prefix)
    if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        return static_cast<int>(strtol(value, nullptr, 16));
    }

    return atoi(value);
}

bool IniParser::GetBool(const char* sectionName, const char* key, bool defaultValue) const {
    const char* value = GetValue(sectionName, key);
    if (!value || value[0] == '\0') {
        return defaultValue;
    }

    std::string strValue = value;
    // Convert to lowercase for comparison
    std::transform(strValue.begin(), strValue.end(), strValue.begin(), 
        [](unsigned char c) { return std::tolower(c); });

    if (strValue == "1" || strValue == "true" || strValue == "yes" || strValue == "on") {
        return true;
    }
    if (strValue == "0" || strValue == "false" || strValue == "no" || strValue == "off") {
        return false;
    }

    return defaultValue;
}

float IniParser::GetFloat(const char* sectionName, const char* key, float defaultValue) const {
    const char* value = GetValue(sectionName, key);
    if (!value || value[0] == '\0') {
        return defaultValue;
    }

    return static_cast<float>(atof(value));
}

// ============================================================
// Value Setters
// ============================================================

bool IniParser::SetValue(const char* sectionName, const char* key, const char* value) {
    if (!sectionName || !key || !value) {
        return false;
    }

    // Find or create section
    IniSection* section = GetSection(sectionName);
    if (!section) {
        IniSection newSection;
        newSection.name = sectionName;
        m_sections.push_back(newSection);
        section = &m_sections.back();
    }

    // Find existing key or add new one
    for (auto& pair : section->pairs) {
        if (pair.key == key) {
            pair.value = value;
            return true;
        }
    }

    // Key not found, add new pair
    IniKeyValuePair newPair;
    newPair.key = key;
    newPair.value = value;
    section->pairs.push_back(newPair);
    return true;
}

bool IniParser::SetInt(const char* sectionName, const char* key, int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return SetValue(sectionName, key, buffer);
}

bool IniParser::SetBool(const char* sectionName, const char* key, bool value) {
    return SetValue(sectionName, key, value ? "1" : "0");
}

// ============================================================
// Utility
// ============================================================

void IniParser::Clear() {
    m_sections.clear();
    m_loadedPath.clear();
}
