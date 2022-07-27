#include <array>
#include <gtest/gtest.h>
#include <tuple>
#include <utility>

#include <ParsePatch.hpp>

using namespace ParsePatch;

TEST(ParsePatch, numbers) {
	auto cases = std::to_array<std::pair<std::string, NumbersT>>({
		{"@@ -123,456 +789,101112 @@", {123, 456, 789, 101112}},
		{"@@ -123 +789,101112 @@", {123, 1, 789, 101112}},
		{"@@ -123,456 +789 @@", {123, 456, 789, 1}},
		{"@@ -123 +789 @@", {123, 1, 789, 1}},
	});
	for(auto &c: cases) {
		LineReader line {.buf = c.first, .line = 1};
		auto numbers_some = line.parse_numbers();
		ASSERT_TRUE(numbers_some.has_value());
		ASSERT_EQ(*numbers_some, c.second);
	}
}

TEST(ParsePatch, get_filename) {
	auto cases = std::to_array<std::pair<std::string, std::string>>({
		{" a/hello/world", "hello/world"},
		{" b/world/hello", "world/hello"},
		{"  a/hello/world", "hello/world"},
		{"   b/world/hello\t", "world/hello"},
		{" /dev/null\t", ""},
	});
	for(auto &c: cases) {
		auto some_p = LineReader::get_filename(c.first, 0);
		ASSERT_TRUE(some_p.has_value());
		auto p = *some_p;
		ASSERT_EQ(p, c.second);
	}
}

/*
// Tautological bullshit, tests nothing, but standard library (in Ruast impl it does encoding to UTF-8)
TEST(ParsePatch, line_starts_with){
	auto cases = std::to_array<std::pair<std::string, std::string>>({
		{"+++ hello", "+++"},
		{"+++ hello", "++++"}
	});
	for(auto & c: cases){
		auto line = LineReader { .buf= c.first, .line= 1 };
		ASSERT_EQ(line.buf.starts_with(c.second), c.first.starts_with(c.second));
	}
}
*/

TEST(ParsePatch, skip_until_empty_line) {
	std::string s {
		"a. string1\n"
		"b. string2\n"
		"\n"
		"c. string3\n"};
	auto cpos = s.find('c');
	PatchReader p {
		.buf = s,
		.pos = 0,
		.line = 1,
		.last = {},
	};
	p.skip_until_empty_line();
	ASSERT_EQ(p.pos, cpos);
}

TEST(ParsePatch, skip_binary) {
	std::string s = {
		"literal 1\n"
		"abcdef\n"
		"ghijkl\n"
		"\n"
		"delta 2\n"
		"abcdef\n"
		"ghijkl\n"
		"\n"
		"Hello\n"};
	auto hpos = s.find('H');
	PatchReader p {
		.buf = s,
		.pos = 0,
		.line = 1,
		.last = {},
	};
	std::vector<BinaryHunk> sizes = p.skip_binary();

	ASSERT_EQ(p.pos, hpos);
	std::vector<BinaryHunk> etalon {
		BinaryHunk {BinaryHunkType::Literal, 1},
		BinaryHunk {BinaryHunkType::Delta, 2}};
	ASSERT_EQ(sizes, etalon);
}

TEST(ParsePatch, parse_files) {
	auto diffs = std::to_array<std::pair<std::string, std::pair<std::string, std::string>>>({
		{
			"diff --git a/foo/bar.cpp b/Foo/Bar/bar.cpp",
			{"foo/bar.cpp", "Foo/Bar/bar.cpp"},
		},
		{
			"diff -r a/foo/bar.cpp b/Foo/Bar/bar.cpp",
			{"foo/bar.cpp", "Foo/Bar/bar.cpp"},
		},
		{
			"diff --git foo/bar.cpp Foo/Bar/bar.cpp",
			{"foo/bar.cpp", "Foo/Bar/bar.cpp"},
		},
	});
	for(auto &s: diffs) {
		LineReader line {
			.buf = s.first,
			.line = 1,
		};
		auto oldNew_some = line.parse_files();
		ASSERT_TRUE(oldNew_some.has_value());
		auto [old, neo] = *oldNew_some;
		ASSERT_EQ(old, (s.second).first);
		ASSERT_EQ(neo, (s.second).second);
	}
}
