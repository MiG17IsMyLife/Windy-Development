#pragma once
#include "common.h"

#include <vector>
#include <string>

class LinuxStack {
public:
    static uint32_t Setup(uint32_t size, const std::vector<std::string>& args);
};