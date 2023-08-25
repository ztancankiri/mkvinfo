#include <Utils.h>

uint32_t get_uint32_be(const void* buf) {
	return get_uint_be(buf, 4);
}

uint64_t get_uint_be(const void* buf, int num_bytes) {
	int i;
	num_bytes = std::min(std::max(1, num_bytes), 8);
	auto tmp = static_cast<unsigned char const*>(buf);
	uint64_t ret = 0;
	for (i = 0; num_bytes > i; ++i)
		ret = (ret << 8) + (tmp[i] & 0xff);

	return ret;
}

inline int gettimeofday(struct timeval* tv, void*) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	return 0;
}

int64_t get_current_time_millis() {
	struct timeval tv;
	if (0 != gettimeofday(&tv, nullptr))
		return -1;

	return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

std::string to_hex(const unsigned char* buf, size_t size, bool compact) {
	if (!buf || !size)
		return {};

	std::string hex;
	for (int idx = 0; idx < static_cast<int>(size); ++idx)
		hex += (compact || hex.empty() ? "" : " ") + fmt::format(compact ? "{0:02x}" : "0x{0:02x}", static_cast<unsigned int>(buf[idx]));

	return hex;
}

void strip_back(std::string& s, bool newlines) {
	auto c = s.c_str();
	int idx = 0, len = s.length();

	while ((idx < len) && (!c[len - idx - 1] || is_blank_or_tab(c[len - idx - 1]) || (newlines && is_newline(c[len - idx - 1]))))
		++idx;

	if (idx > 0)
		s.erase(len - idx, idx);
}

void strip(std::string& s, bool newlines) {
	auto c = s.c_str();
	int idx = 0, len = s.length();

	while ((idx < len) && (!c[idx] || is_blank_or_tab(c[idx]) || (newlines && is_newline(c[idx]))))
		++idx;

	if (idx > 0)
		s.erase(0, idx);

	strip_back(s, newlines);
}

std::vector<std::string> split(const std::string& input, char delimiter) {
	std::vector<std::string> parts;

	size_t startPos = 0;
	size_t foundPos = input.find(delimiter);

	while (foundPos != std::string::npos) {
		parts.push_back(input.substr(startPos, foundPos - startPos));
		startPos = foundPos + 1;
		foundPos = input.find(delimiter, startPos);
	}

	if (startPos < input.length()) {
		parts.push_back(input.substr(startPos));
	}

	return parts;
}

bool contains(const std::string& str, char targetChar) {
	return str.find(targetChar) != std::string::npos;
}

void xmlToJsonObject(const pugi::xml_node& node, json& jsonObj) {
	for (pugi::xml_node childNode = node.first_child(); childNode; childNode = childNode.next_sibling()) {
		json childJsonObj;

		if (childNode.first_child() && childNode.first_child().type() == pugi::node_pcdata) {
			// Handle text content of the node
			childJsonObj = childNode.first_child().value();
		} else {
			xmlToJsonObject(childNode, childJsonObj);
		}

		jsonObj[childNode.name()].push_back(childJsonObj);
	}
}

void recursiveIteration(json& jsonObj) {
	for (auto& [key, value] : jsonObj.items()) {
		if (value.is_object()) {
			recursiveIteration(value);
		} else if (value.is_array()) {
			if (value.size() == 1 && value[0].is_string()) {
				jsonObj[key] = value[0];
			} else {
				for (auto& arrayElem : value) {
					recursiveIteration(arrayElem);
				}
			}
		}
	}
}