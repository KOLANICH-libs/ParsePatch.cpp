#include <iostream>

#include "ParsePatch.hpp"

#if __has_include(<magic_enum.hpp>)
#include <magic_enum.hpp>
#else
#warning "magic_enum is unavailable. The built variant of the lib will output enums as numbers instead of fancy string names. You can get magic_enum byt he link: https://github.com/Neargye/magic_enum"
#endif

namespace ParsePatch {

using namespace ScannerUtils;

Diff::~Diff() = default;
Patch::~Patch() = default;

bool operator==(const BinaryHunk &lhs, const BinaryHunk &rhs) {
	return (lhs.size == rhs.size) && (lhs.type == rhs.type);
}

bool operator==(const NumbersT &lhs, const NumbersT &rhs) {
	return lhs.new_lines == rhs.new_lines && lhs.new_count == rhs.new_count && lhs.old_lines == rhs.old_lines && lhs.old_count == rhs.old_count;
}

std::ostream &operator<<(std::ostream &s, const FileOp &op) {
	return s << "FileOp(" <<
#if defined(NEARGYE_MAGIC_ENUM_HPP)
	magic_enum::enum_name(op.code)
#else
	std::static_cast<uint16_t>(op.code)
#endif
	<< ", " << op.something << ")" << std::endl;
}

std::ostream &operator<<(std::ostream &s, const ParsepatchError &err) {
	switch(err.code) {
		case ParsepatchErrorCode::OK: {
			return s << "OK" << std::endl;
		} break;
		case ParsepatchErrorCode::InvalidHunkHeader: {
			return s << "Invalid hunk header at line " << err.line_or_str << std::endl;
		} break;
		case ParsepatchErrorCode::NewModeExpected: {
			return s << "New mode expected after old mode at line " << err.line_or_str << std::endl;
		} break;
		case ParsepatchErrorCode::NoFilename: {
			return s << "Cannot get filename at line " << err.line_or_str << std::endl;
		} break;
		case ParsepatchErrorCode::InvalidString: {
			return s << "Invalid utf-8 at line " << err.line_or_str << std::endl;
		} break;
	}
	return s;
}

std::ostream &operator<<(std::ostream &s, const LineReader &line) {
	return s << "buffer: " << line.buf;
}

template<typename IntT, typename = std::enable_if_t<std::is_integral<IntT>::value>>
constexpr IntT decimal_reducer(IntT r, char c) {
	return r * 10 + static_cast<IntT>(c - '0');
};

constexpr bool is_digit(char c) {
	return c >= '0' && c <= '9';
};

constexpr uint32_t octal_reducer(uint32_t r, char c) {
	return r * 8 + static_cast<uint32_t>(c - '0');
};

constexpr bool is_octal_digit(char c) {
	return c >= '0' && c <= '7';
}

constexpr uint32_t parse_decimal_number(const char *&iter, const char *bound, char c = '0') {
	auto res = static_cast<uint32_t>(c - '0');
	for(; iter != bound; ++iter) {
		auto el = *iter;
		if(!is_digit(el))
			break;

		res = decimal_reducer(res, el);
	}
	return res;
}

bool FileOp::is_new_or_deleted() {
	switch(code) {
		case FileOpCode::New:
			return true;
		case FileOpCode::Deleted:
			return true;
		default:
			return false;
	}
}

static ParsepatchError noParsePatchError {
	.code = ParsepatchErrorCode::OK,
	.line_or_str = 0};

bool LineReader::is_empty() const {
	return this->buf.empty();
}

bool LineReader::is_binary() const {
	return this->buf == "GIT binary patch";
}

bool LineReader::is_rename_from() {
	return this->buf.starts_with("rename from");
}

bool LineReader::is_copy_from() {
	return this->buf.starts_with("copy from");
}

bool LineReader::is_new_file() {
	return this->buf.starts_with("new file");
}

bool LineReader::is_triple_minus() {
	return this->buf.starts_with("--- ");
}

bool LineReader::is_triple_plus() {
	return this->buf.starts_with("+++ ");
}

bool LineReader::is_index() {
	return this->buf.starts_with("index ");
}

bool LineReader::is_deleted_file() {
	return this->buf.starts_with("deleted file");
}

size_t LineReader::get_line() const {
	return this->line;
}

FileOp LineReader::get_file_op() {
	if(this->is_new_file()) {
		return {FileOpCode::New, this->parse_mode("new file mode ")};
	} else if(this->is_deleted_file()) {
		return {FileOpCode::Deleted, this->parse_mode("deleted file mode ")};
	} else if(this->is_rename_from()) {
		return {FileOpCode::Renamed};
	} else if(this->is_copy_from()) {
		return {FileOpCode::Copied};
	} else {
		return {FileOpCode::None};
	}
}

Result<NumbersT> LineReader::parse_numbers() {
	// we know that line is beginning with "@@ -"
	auto buf = std::string_view(begin(this->buf) + 4, end(this->buf));

	// NUMS_PAT = re.compile(r'^@@ -([0-9]+),?([0-9]+)?
	// \+([0-9]+),?([0-9]+)? @@')
	auto iter = begin(buf);
	auto old_start = parse_decimal_number(iter, end(buf));
	++iter;
	if(iter == end(buf)) {
		return unexpected<ParsepatchError>({ParsepatchErrorCode::InvalidHunkHeader, this->get_line()});
	}
	auto c = *(iter++);
	uint32_t old_lines;
	if(is_digit(c)) {
		old_lines = parse_decimal_number(iter, end(buf), c);
	} else {
		old_lines = 1;
	}

	if(c != '+') {
		iter = std::find(iter, std::end(buf), '+');
		++iter;// C++ addition, difference in behaviour?
	}

	auto new_start = parse_decimal_number(iter, end(buf));

	++iter;
	if(iter == end(buf)) {
		return unexpected<ParsepatchError>({ParsepatchErrorCode::InvalidHunkHeader, this->get_line()});
	}

	c = *(iter++);
	uint32_t new_lines;
	if(is_digit(c)) {
		new_lines = parse_decimal_number(iter, end(buf), c);
	} else {
		new_lines = 1;
	}

	return {{old_start, old_lines, new_start, new_lines}};
}

Result<std::string_view> LineReader::get_filename(std::string_view buf, size_t line) {
	auto pos1It = std::find_if(begin(buf), end(buf), [&](char c) {
		return c != ' ';
	});
	if(pos1It >= end(buf)) {
		return unexpected<ParsepatchError>({ParsepatchErrorCode::NoFilename, line});
	}
	auto pos2It = std::find_if(pos1It, end(buf), [&](char c) {
		return c == '\t';
	});
	if(pos2It < end(buf)) {
		buf = std::string_view(pos1It, pos2It);
	} else {
		buf = std::string_view(pos1It, end(buf));
	}

	if(buf.starts_with("a/") || buf.starts_with("b/")) {
		buf = std::string_view(begin(buf) + 2, end(buf));
	}

	if(buf == "/dev/null") {
		return "";
	} else {
		//return unexpected<ParsepatchError>({ParsepatchErrorCode::InvalidString, line}); // In the case of utf-8 decoding error
		return buf;
	}
}

uint32_t LineReader::parse_mode(const std::string_view start) {
	// we know that line is beginning with "old/new mode "
	// the following number is an octal number with 6 digits (so max is
	// 8^6 - 1)
	auto buf = std::string_view(begin(this->buf) + start.size(), end(this->buf));
	uint32_t res = 0;
	for(auto c: buf) {
		if(!is_octal_digit(c)) {
			break;
		}
		res = octal_reducer(res, c);
	}
	return res;
}

std::string_view LineReader::get_file(std::optional<std::string_view> slice, std::string starter) const {
	if(slice) {
		auto path = *slice;
		auto start = std::string_view(begin(path), begin(path) + 2);
		if(end(start) <= end(path)) {
			if(start == starter) {
				return {begin(path) + 2, end(path)};
			} else {
				return path;
			}
		} else {
			return path;
		}
	} else {
		return "";
	}
}

Result<std::tuple<std::string_view, std::string_view>> LineReader::parse_files() {
	// We know we start with 'diff '
	if(this->buf.size() < 5) {
		return unexpected<ParsepatchError>({ParsepatchErrorCode::InvalidHunkHeader, this->get_line()});
	}

	auto buf = std::string_view(begin(this->buf) + 5, end(this->buf));// `unsafe` block and maybe a bug in the original lib.

	std::vector<std::string_view> splitted;
	uint8_t idx = 0;

	std::string_view old = {};
	std::string_view neo = {};

	auto loopBodyLam = [&](const std::string_view cur, uint8_t idx) {
		switch(idx) {
			case 0:
				break;
			case 1:
				old = get_file(cur, "a/");
				break;
			case 2:
				neo = get_file(cur, "b/");
				break;
		}
	};

	for(size_t start = 0, end = buf.find(' '); idx != 3u; start = end + 1, end = buf.find(' ', start), ++idx) {
		std::string_view cur;
		if(end == std::string::npos) {
			cur = std::string_view {begin(buf) + start, std::end(buf)};
			loopBodyLam(cur, idx);
			break;
		} else {
			cur = buf.substr(start, end - start);
			loopBodyLam(cur, idx);
		}
	}

	if(!old.size() || !neo.size()) {
		return unexpected<ParsepatchError>({ParsepatchErrorCode::InvalidString, this->get_line()});
	}

	return {{old, neo}};
}

/// Prepares the object to parsing of new patch
void PatchReader::reset() {
	this->pos = 0;
	this->line = 1;
	this->last = {};
}

/// Read a patch from the given buffer
ParsepatchError PatchReader::by_buf(std::string_view buf, Patch &patch) {
	reset();
	this->buf = buf;
	return parse(patch);
}

size_t PatchReader::get_line() const {
	return line;
}

//<Diff D, Patch<D> P>
ParsepatchError PatchReader::parse(Patch &patch) {
	while(true) {
		auto some_line = this->next(starter, false);
		if(!some_line) {
			break;
		}
		auto &line = *some_line;
		ParsepatchError res = this->parse_diff(line, patch);
		if(res) {
			return res;
		}
	}
	patch.close();

	return noParsePatchError;
}

ParsepatchError PatchReader::parse_diff(LineReader &diff_line, Patch &patch) {

	if(tracing) {
		*tracing << "Diff " << diff_line << std::endl;
	}

	if(diff_line.is_triple_minus()) {
		// The diff starts with a ---: need to look ahead for no "diff ..."
		// to be sure that we aren't in the header.
		std::string diff = "\ndiff -";
		auto buf = std::string_view(begin(this->buf) + pos, end(this->buf));
		auto some_pos = std::search(begin(buf), end(buf), begin(diff), end(diff));

		if(some_pos != end(buf)) {
			decltype(this->pos) pos = some_pos - begin(buf);
			// +1 for the '\n'
			this->pos += pos + 1u;
			return noParsePatchError;
		}
		return this->parse_minus(diff_line, FileOp {FileOpCode::None}, {}, patch);
	}

	auto some_line = this->next(mv, false);
	LineReader line;
	if(some_line) {
		line = *some_line;
	} else {
		// Nothing more... so close it
		auto someOldNew = diff_line.parse_files();
		if(!someOldNew) {
			return someOldNew.error();
		}
		auto [old, neo] = *someOldNew;

		if(tracing) {
			*tracing << "Single diff line: new: " << neo;
		}

		auto diff = patch.new_diff();
		diff->set_info(old, neo, FileOp {FileOpCode::None}, {}, {});
		diff->close();
		return noParsePatchError;
	}

	std::optional<FileMode> file_mode;
	if(old_mode(line)) {
		auto old = line.parse_mode("old mode ");
		auto some_l = this->next(mv, false);
		if(!some_l) {
			return ParsepatchError {ParsepatchErrorCode::NewModeExpected, this->get_line()};
		}
		auto l = *some_l;
		auto neo = l.parse_mode("new mode ");
		auto file_mode_1 = FileMode {old, neo};
		if(auto some_l = this->next(mv, false)) {
			line = *some_l;
			file_mode = file_mode_1;
		} else {
			// Nothing more... so close it
			auto some_oldNew = diff_line.parse_files();
			if(!some_oldNew) {
				return some_oldNew.error();
			}
			auto [old, neo] = *some_oldNew;

			if(tracing) {
				*tracing << "Single diff line (mode change): new: " << neo;
			}

			auto diff = patch.new_diff();
			diff->set_info(old, neo, FileOp {FileOpCode::None}, {}, std::optional(file_mode));
			diff->close();
			return noParsePatchError;
		}
	}

	auto op = line.get_file_op();

	if(tracing) {
		*tracing << "Diff (op = " <<
		#if defined(NEARGYE_MAGIC_ENUM_HPP)
		magic_enum::enum_name(op.code)
		#else
		static_cast<uint16_t>(op.code)
		#endif
		<< "): " << diff_line << ", next_line: " << line << std::endl;
	}

	if(diff(line) || line.is_empty()) {
		auto someOldNew = diff_line.parse_files();
		if(!someOldNew) {
			return someOldNew.error();
		}
		auto [old, neo] = *someOldNew;

		if(tracing) {
			*tracing << "Single diff line: old:  " << old << " -- new: " << neo << std::endl;
		}

		auto diff = patch.new_diff();
		diff->set_info(old, neo, {FileOpCode::None, 0}, {}, file_mode);
		diff->close();
		this->set_last(line);
		return noParsePatchError;
	}

	if(op.code == FileOpCode::Renamed || op.code == FileOpCode::Copied) {
		// when no changes in the file there is no ---/+++ stuff
		// so need to get info here
		uint32_t shift_1, shift_2;
		if(op.code == FileOpCode::Renamed) {
			shift_1 = sizeof("rename from ") - 1;
			shift_2 = sizeof("rename to ") - 1;
		} else {
			shift_1 = sizeof("copy from ") - 1;
			shift_2 = sizeof("copy to ") - 1;
		}
		auto some_old = LineReader::get_filename(std::string_view(begin(line.buf) + shift_1, end(line.buf)), line.get_line());
		if(!some_old) {
			return some_old.error();
		}
		auto old = *some_old;
		auto someLine = this->next(mv, false);
		if(!someLine) {
			return ParsepatchError {ParsepatchErrorCode::InvalidHunkHeader, this->get_line()};
		}
		auto _line = *someLine;
		auto some_neo = LineReader::get_filename(std::string_view(begin(_line.buf) + shift_2, end(_line.buf)), line.get_line());
		if(!some_neo) {
			return some_neo.error();
		}
		auto neo = *some_neo;

		if(tracing) {
			*tracing << "Copy/Renamed from " << old << " to " << neo << std::endl;
		}

		auto diff = patch.new_diff();
		diff->set_info(old, neo, {FileOpCode::Renamed, 0}, {}, file_mode);

		auto some_line = this->next(mv, false);
		if(some_line) {
			auto _line = *some_line;
			if(_line.is_triple_minus()) {
				// skip +++ line
				this->next(mv, false);
				auto line_some = this->next(mv, false);
				if(!line_some) {
					return ParsepatchError {ParsepatchErrorCode::InvalidHunkHeader, this->get_line()};
				}
				line = *line_some;
				auto err = this->parse_hunks(line, diff);
				if(err) {
					return err;
				}
				diff->close();
			} else {
				// we just have a rename/copy but no changes in the file
				diff->close();
				this->set_last(_line);
			}
		}
	} else {
		if(op.is_new_or_deleted() || line.is_index()) {
			if(tracing) {
				*tracing << "New/Delete file: " << line << std::endl;
			}
			auto some_line = this->next(useful, false);
			if(some_line) {
				line = *some_line;
			} else {
				// Nothing more... so close it
				auto some_neoOld = diff_line.parse_files();
				if(!some_neoOld) {
					return some_neoOld.error();
				}
				auto [old, neo] = *some_neoOld;

				if(tracing) {
					*tracing << "Single new/delete diff line: new: " << neo;
				}

				auto diff = patch.new_diff();
				diff->set_info(old, neo, op, {}, file_mode);
				diff->close();
				return noParsePatchError;
			}
			if(tracing) {
				*tracing << "New/Delete file: next useful line " << line << std::endl;
			}
			if(line.is_binary()) {
				// We've file info only in the diff line
				// TODO: old is probably useless here
				auto some_neoOld = diff_line.parse_files();
				if(!some_neoOld) {
					return some_neoOld.error();
				}
				auto [old, neo] = *some_neoOld;

				if(tracing) {
					*tracing << "Binary file (op == " << op << "): " << neo << std::endl;
				}

				auto diff = patch.new_diff();
				auto sizes = this->skip_binary();

				diff->set_info(old, neo, op, {sizes}, file_mode);
				diff->close();
				return noParsePatchError;
			} else if(diff(line)) {
				auto some_neoOld = diff_line.parse_files();
				if(!some_neoOld) {
					return some_neoOld.error();
				}
				auto [old, neo] = *some_neoOld;

				if(tracing) {
					*tracing << "Single new/delete diff line: new: " << neo << std::endl;
				}

				auto diff = patch.new_diff();
				diff->set_info(old, neo, op, {}, file_mode);
				diff->close();
				this->set_last(line);
				return noParsePatchError;
			}
		}

		if(line.is_triple_minus()) {
			return this->parse_minus(line, op, file_mode, patch);
		}
	}

	return noParsePatchError;
}

ParsepatchError PatchReader::parse_minus(LineReader &line, FileOp op, std::optional<FileMode> file_mode, Patch &patch) {
	if(tracing) {
		*tracing << "DEBUG (---): " << line << std::endl;
	}

	// here we've a ---
	auto old_some = LineReader::get_filename(std::string_view(begin(line.buf) + 3, end(line.buf)), line.get_line());
	if(!old_some) {
		return old_some.error();
	}
	auto old = *old_some;

	auto some_line = this->next(mv, false);
	if(!some_line) {
		return ParsepatchError {ParsepatchErrorCode::InvalidHunkHeader, this->get_line()};
	}
	auto _line = *some_line;

	if(!_line.is_triple_plus()) {
		if(tracing) {
			*tracing << "DEBUG (not a +++): " << _line << std::endl;
		}
		return noParsePatchError;
	}
	// 3 == len("+++")
	auto some_new = LineReader::get_filename(std::string_view(begin(_line.buf) + 3, end(_line.buf)), line.get_line());
	if(!some_new) {
		return some_new.error();
	}
	auto neo = *some_new;

	if(tracing) {
		*tracing << "Files: old: " << old << " -- new: " << neo << std::endl;
	}

	auto diff = patch.new_diff();
	diff->set_info(old, neo, op, {}, file_mode);
	auto line_some = this->next(mv, false);
	{
		if(!line_some) {
			return ParsepatchError {ParsepatchErrorCode::InvalidHunkHeader, this->get_line()};
		}
		auto line = *line_some;
		auto parseHunksError = this->parse_hunks(line, diff);
		if(parseHunksError) {
			return parseHunksError;
		}
		diff->close();
		return noParsePatchError;
	}
}

ParsepatchError PatchReader::parse_hunks(LineReader &line, Diff *diff) {
	auto nums_some = line.parse_numbers();
	if(!nums_some) {
		return nums_some.error();
	}
	auto nums = *nums_some;
	this->parse_hunk(nums, diff);
	for(auto line_some = this->next(hunk_at, true); line_some; line_some = this->next(hunk_at, true)) {
		auto line = *line_some;
		nums_some = line.parse_numbers();
		if(!nums_some) {
			return nums_some.error();
		}
		auto [o1, o2, n1, n2] = *nums_some;
		this->parse_hunk(nums, diff);
	}

	return noParsePatchError;
}

void PatchReader::parse_hunk(NumbersT lines_count, Diff *diff) {
	diff->new_hunk();
	for(std::optional<LineReader> line_some = this->next(hunk_change, true); line_some; line_some = this->next(hunk_change, true)) {
		auto line = *line_some;
		// we know that line is beginning with -, +, ... so no need to check
		// bounds
		auto first = line.buf[0];
		switch(first) {
			case '-': {
				diff->add_line(lines_count.old_count, 0, std::string_view {begin(line.buf) + 1, end(line.buf)});
				lines_count.old_count += 1;
				lines_count.old_lines -= 1;
			} break;
			case '+': {
				diff->add_line(0, lines_count.new_count, std::string_view {begin(line.buf) + 1, end(line.buf)});
				lines_count.new_count += 1;
				lines_count.new_lines -= 1;
			} break;
			case ' ': {
				diff->add_line(lines_count.old_count, lines_count.new_count, std::string_view(begin(line.buf) + 1, end(line.buf)));
				lines_count.old_count += 1;
				lines_count.new_count += 1;
				lines_count.old_lines -= 1;
				lines_count.new_lines -= 1;
			} break;
			default: {
			} break;
		}
		if(lines_count.old_lines == 0 && lines_count.new_lines == 0) {
			break;
		}
	}
}

void PatchReader::set_last(LineReader line) {
	this->last = std::optional<LineReader> {line};
}

std::optional<LineReader> PatchReader::next(NextFilterF filter, bool return_on_false) {
	std::optional<LineReader> l = {};
	std::swap(l, this->last);
	if(l) {
		auto line = *l;
		if(filter(line)) {
			return {line};
		} else {
			if(return_on_false) {
				return {};
			}
		}
	}

	auto pos = this->pos;
	auto buf = std::string_view(begin(this->buf) + pos, end(this->buf));
	if(end(buf) <= end(this->buf) && begin(buf) < end(buf)) {
		size_t n = 0;
		for(auto c: buf) {
			if(c == '\n') {
				auto npos = this->pos + n;
				if(npos > 0) {
					auto &prev = this->buf[npos - 1];
					if(prev == '\r') {
						npos -= 1;
					}
				}
				auto line = LineReader {
					.buf = std::string_view {begin(this->buf) + pos, begin(this->buf) + npos},
					.line = this->line,
				};
				this->line += 1;
				if(filter(line)) {
					this->pos += n + 1;
					return {line};
				} else if(return_on_false) {
					return {};
				}
				pos = this->pos + n + 1;
			}
			++n;
		}
	}
	return {};
}

void PatchReader::skip_until_empty_line() {
	auto buf = std::string_view(begin(this->buf) + pos, end(this->buf));
	if(begin(buf) < end(buf)) {
		size_t spos = 0;
		size_t n = 0;
		for(auto c: buf) {
			if(c == '\n') {
				this->line += 1;
				if(n == spos) {
					this->pos += n + 1;
					return;
				} else {
					spos = n + 1;
				}
			}
			++n;
		}
	}
	this->pos = this->buf.size();
}

namespace ScannerUtils {
size_t parse_usize(const std::string_view buf) {
	uint32_t res = 0;
	for(auto c: buf) {
		if(!is_digit(c)) {
			break;
		}
		res = decimal_reducer(res, c);
	}
	return res;
}

bool diff(LineReader &line) {
	return line.buf.starts_with("diff -");
}

bool useful(LineReader &line) {
	return line.is_binary() || line.is_triple_minus() || diff(line);
}

bool starter(LineReader &line) {
	return line.is_triple_minus() || diff(line);
}

bool mv([[maybe_unused]] LineReader &_) {
	return true;
}

bool hunk_at(LineReader &line) {
	return line.buf.starts_with("@@ -");
}

bool old_mode(LineReader &line) {
	return line.buf.starts_with("old mode ");
}

bool hunk_change(LineReader &line) {
	if(line.buf.size()) {
		auto c = line.buf[0];
		return c == '-' || c == '+' || c == ' ' || line.buf.starts_with("\\ No newline");
	} else {
		return false;
	}
}
};// namespace ScannerUtils

std::vector<BinaryHunk> PatchReader::skip_binary() {
	auto sizes = std::vector<BinaryHunk>();

	for(std::string_view buf = {begin(this->buf) + pos, end(this->buf)}; begin(buf) < end(buf); buf = {begin(this->buf) + pos, end(this->buf)}) {
		if(buf.starts_with("literal ")) {
			this->pos += 8;
			auto buf1 = std::string_view(begin(this->buf) + pos, end(this->buf));
			sizes.emplace_back(BinaryHunk {BinaryHunkType::Literal, parse_usize(buf1)});
		} else {
			if(buf.starts_with("delta ")) {
				this->pos += 6;
				auto buf1 = std::string_view(begin(this->buf) + pos, end(this->buf));
				sizes.emplace_back(BinaryHunk {BinaryHunkType::Delta, parse_usize(buf1)});
			} else {
				break;
			}
		}
		this->skip_until_empty_line();
	}
	return sizes;
}

}// namespace ParsePatch
