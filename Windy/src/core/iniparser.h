#pragma once

#include <string>
#include <vector>
#include <map>

/**
 * @brief Key-Value pair structure for INI entries
 */
struct IniKeyValuePair {
    std::string key;
    std::string value;
};

/**
 * @brief Section structure containing name and key-value pairs
 */
struct IniSection {
    std::string name;
    std::vector<IniKeyValuePair> pairs;
};

/**
 * @brief INI file parser and writer
 * 
 * Provides functionality to load, modify, and save INI configuration files.
 * Ported from Lindbergh-loader's iniParser.c with C++ enhancements.
 */
class IniParser {
public:
    IniParser();
    ~IniParser();

    /**
     * @brief Load and parse an INI file
     * @param filename Path to the INI file
     * @return true if successful, false on failure
     */
    bool Load(const char* filename);

    /**
     * @brief Save the configuration to a file
     * @param filename Path to save to (uses loaded path if nullptr)
     * @return true if successful, false on failure
     */
    bool Save(const char* filename = nullptr);

    /**
     * @brief Get a value from the configuration
     * @param sectionName Name of the section
     * @param key Key to look up
     * @return Value string, or empty string if not found
     */
    const char* GetValue(const char* sectionName, const char* key) const;

    /**
     * @brief Get a value as integer
     * @param sectionName Name of the section
     * @param key Key to look up
     * @param defaultValue Value to return if not found
     * @return Integer value or defaultValue
     */
    int GetInt(const char* sectionName, const char* key, int defaultValue = 0) const;

    /**
     * @brief Get a value as boolean
     * @param sectionName Name of the section
     * @param key Key to look up
     * @param defaultValue Value to return if not found
     * @return Boolean value or defaultValue
     */
    bool GetBool(const char* sectionName, const char* key, bool defaultValue = false) const;

    /**
     * @brief Get a value as float
     * @param sectionName Name of the section
     * @param key Key to look up
     * @param defaultValue Value to return if not found
     * @return Float value or defaultValue
     */
    float GetFloat(const char* sectionName, const char* key, float defaultValue = 0.0f) const;

    /**
     * @brief Set a value in the configuration
     * @param sectionName Name of the section (created if doesn't exist)
     * @param key Key to set
     * @param value Value to set
     * @return true if successful
     */
    bool SetValue(const char* sectionName, const char* key, const char* value);

    /**
     * @brief Set an integer value
     */
    bool SetInt(const char* sectionName, const char* key, int value);

    /**
     * @brief Set a boolean value
     */
    bool SetBool(const char* sectionName, const char* key, bool value);

    /**
     * @brief Get a section by name
     * @param sectionName Name of the section
     * @return Pointer to section, or nullptr if not found
     */
    IniSection* GetSection(const char* sectionName);
    const IniSection* GetSection(const char* sectionName) const;

    /**
     * @brief Check if a section exists
     */
    bool HasSection(const char* sectionName) const;

    /**
     * @brief Check if a key exists in a section
     */
    bool HasKey(const char* sectionName, const char* key) const;

    /**
     * @brief Clear all loaded data
     */
    void Clear();

    /**
     * @brief Get the number of sections
     */
    size_t GetSectionCount() const { return m_sections.size(); }

    /**
     * @brief Get all sections (for iteration)
     */
    const std::vector<IniSection>& GetSections() const { return m_sections; }

private:
    /**
     * @brief Trim whitespace from a string
     */
    static std::string TrimString(const std::string& str);

    std::vector<IniSection> m_sections;
    std::string m_loadedPath;
};
