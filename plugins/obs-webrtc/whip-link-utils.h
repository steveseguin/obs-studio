#pragma once

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct whip_link {
	std::string url;
	std::map<std::string, std::string> parameters;
};

inline std::string whip_trim_link_value(const std::string &value)
{
	const auto begin = value.find_first_not_of(" \t");
	if (begin == std::string::npos) {
		return {};
	}
	return value.substr(begin, value.find_last_not_of(" \t") - begin + 1);
}

// Separators inside a URI reference or a quoted parameter are data, not delimiters.
inline std::vector<std::string> whip_split_link_value(const std::string &value, char separator)
{
	std::vector<std::string> parts;
	bool quoted = false;
	bool escaped = false;
	bool uri = false;
	size_t begin = 0;
	for (size_t i = 0; i < value.size(); i++) {
		const char c = value[i];
		if (c == '\r' || c == '\n') {
			return {};
		}
		if (escaped) {
			escaped = false;
		} else if (quoted && c == '\\') {
			escaped = true;
		} else if (!uri && c == '"') {
			quoted = !quoted;
		} else if (!quoted && c == '<') {
			uri = true;
		} else if (!quoted && c == '>') {
			uri = false;
		} else if (!quoted && !uri && c == separator) {
			parts.push_back(whip_trim_link_value(value.substr(begin, i - begin)));
			begin = i + 1;
		}
	}
	if (quoted || escaped || uri) {
		return {};
	}
	parts.push_back(whip_trim_link_value(value.substr(begin)));
	return parts;
}

inline bool whip_link_token(const std::string &value)
{
	if (value.empty()) {
		return false;
	}
	return std::all_of(value.begin(), value.end(), [](unsigned char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		       std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos;
	});
}

inline bool whip_decode_link_parameter(const std::string &value, std::string &decoded)
{
	decoded.clear();
	if (value.empty() || value.front() != '"') {
		decoded = value;
		return whip_link_token(value);
	}
	for (size_t i = 1; i < value.size(); i++) {
		if (value[i] == '"') {
			return i + 1 == value.size();
		}
		if (value[i] == '\\' && ++i == value.size()) {
			return false;
		}
		decoded += value[i];
	}
	return false;
}

// RFC 8288 allows both tokens and quoted strings, including quoted-pair escapes.
inline std::vector<whip_link> whip_parse_link_header(const std::string &value)
{
	std::vector<whip_link> links;
	for (const auto &entry : whip_split_link_value(value, ',')) {
		const auto parts = whip_split_link_value(entry, ';');
		if (parts.empty() || parts.front().size() < 3 || parts.front().front() != '<' ||
		    parts.front().back() != '>') {
			continue;
		}
		whip_link link;
		link.url = parts.front().substr(1, parts.front().size() - 2);
		bool valid = true;
		for (size_t i = 1; i < parts.size(); i++) {
			const auto equals = parts[i].find('=');
			std::string name = whip_trim_link_value(parts[i].substr(0, equals));
			if (!whip_link_token(name)) {
				valid = false;
				break;
			}
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
			std::string parameter;
			if (equals != std::string::npos &&
			    !whip_decode_link_parameter(whip_trim_link_value(parts[i].substr(equals + 1)), parameter)) {
				valid = false;
				break;
			}
			// Preserve the first occurrence, including rel, as required by RFC 8288.
			link.parameters.emplace(name, parameter);
		}
		if (valid) {
			links.push_back(std::move(link));
		}
	}
	return links;
}
