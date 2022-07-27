#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct LineChange {
	uint32_t line = 0;
	bool deleted = false;
	std::string data;
};

bool operator==(const LineChange &lhs, const LineChange &rhs);

struct Hunk {
	std::vector<LineChange> lines;
};

struct FileModeChange {
	uint32_t old = 0;
	uint32_t neo = 0;
};

struct ParsedDiff {
	std::string filename {""};
	bool neo = false;
	bool deleted = false;
	bool binary = false;
	std::vector<Hunk> hunks;

	std::optional<std::string> copied_from;
	std::optional<std::string> renamed_from;
	std::optional<FileModeChange> file_mode;
};

struct ParsedPatch {
	std::vector<ParsedDiff> diffs {};

	static ParsedPatch fromJSONBuffer(std::string_view fileContents);
};
