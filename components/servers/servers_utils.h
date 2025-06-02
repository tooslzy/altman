#pragma once

#include <string>
#include <array>
#include <cstdint>

std::array<std::uint8_t, 16> parseGuid(const std::string &guid);

std::size_t hashBytes(const std::array<std::uint8_t, 16> &bytes);

std::string guidToName(const std::string &guid);

std::string toLower(std::string s);

bool containsCI(const std::string &haystack, const std::string &needle);
