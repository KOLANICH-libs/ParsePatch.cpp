#include "json.hpp"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

//#include <json_struct/json_struct.h>

bool operator==(const LineChange &lhs, const LineChange &rhs) {
	return lhs.line == rhs.line && lhs.deleted == rhs.deleted && lhs.data == rhs.data;
}

void to_json(json &j, const LineChange &l) {
	j = json {{"line", l.line}, {"deleted", l.deleted}, {"data", l.data}};
}

void from_json(const json &j, LineChange &l) {
	j.at("line").get_to(l.line);
	j.at("deleted").get_to(l.deleted);
	j.at("data").get_to(l.data);
}

void to_json(json &j, const Hunk &h) {
	j = json {{"lines", h.lines}};
}

void from_json(const json &j, Hunk &h) {
	j.at("lines").get_to(h.lines);
}

void to_json(json &j, const FileModeChange &m) {
	j = json {{"old", m.old}, {"new", m.neo}};
}

void from_json(const json &j, FileModeChange &m) {
	j.at("old").get_to(m.old);
	j.at("new").get_to(m.neo);
}

void to_json(json &j, const ParsedDiff &d) {
	j = json {
		{"filename", d.filename},
		{"new", d.neo},
		{"deleted", d.deleted},
		{"binary", d.binary},
		{"hunks", d.hunks},
	};
	if(d.copied_from) {
		j.at("copied_from") = *d.copied_from;
	}
	if(d.renamed_from) {
		j.at("renamed_from") = *d.renamed_from;
	}
	if(d.file_mode) {
		j.at("file_mode") = *d.file_mode;
	}
}

void from_json(const json &j, ParsedDiff &d) {
	j.at("filename").get_to(d.filename);
	j.at("new").get_to(d.neo);
	j.at("deleted").get_to(d.deleted);
	j.at("binary").get_to(d.binary);
	j.at("hunks").get_to(d.hunks);
	try {
		d.copied_from = {j.at("copied_from")};
	} catch(json::exception ex) {
		d.copied_from = {};
	}
	try {
		d.renamed_from = {j.at("renamed_from")};
	} catch(json::exception ex) {
		d.renamed_from = {};
	}
	try {
		d.file_mode = {j.at("file_mode")};
	} catch(json::exception ex) {
		d.file_mode = {};
	}
}

void to_json(json &j, const ParsedPatch &p) {
	j = json {
		{"diffs", p.diffs},
	};
}

void from_json(const json &j, ParsedPatch &p) {
	j.at("diffs").get_to(p.diffs);
}

ParsedPatch ParsedPatch::fromJSONBuffer(std::string_view fileContents) {
	return json::parse(fileContents).get<ParsedPatch>();
}
