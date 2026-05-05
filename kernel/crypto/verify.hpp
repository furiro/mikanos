#pragma once
#include <stdint.h>
#include <vector>
#include "error.hpp"


WithError<uint64_t> VerifyModuleSignature(const std::vector<uint8_t> module_data, const std::vector<uint8_t> module_signature);
