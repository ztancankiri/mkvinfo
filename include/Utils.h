#ifndef UTILS_H
#define UTILS_H

#include <fmt/format.h>
#include <sys/time.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>
#include <vector>

using json = nlohmann::json;

#define gettext(s) (s)
#define Y(s) gettext(u8##s)

constexpr auto TIMESTAMP_SCALE = 1'000'000;
constexpr auto is_blank_or_tab(char c) { return (c == ' ') || (c == '\t'); }
constexpr auto is_newline(char c) { return (c == '\n') || (c == '\r'); }

uint64_t get_uint_be(const void* buf, int num_bytes);
uint32_t get_uint32_be(const void* buf);

inline int gettimeofday(struct timeval* tv, void*);
int64_t get_current_time_millis();

std::string to_hex(const unsigned char* buf, size_t size, bool compact = false);
void strip_back(std::string& s, bool newlines = false);
void strip(std::string& s, bool newlines = false);

std::vector<std::string> split(const std::string& input, char delimiter);
bool contains(const std::string& str, char targetChar);

void xmlToJsonObject(const pugi::xml_node& node, json& jsonObj);
void recursiveIteration(json& jsonObj);

#endif