#pragma once
#include <cstdint>

#include <optional>
#include <string_view>

#if __has_include(<expected>)
	#include <expected>
#else
	#if __has_include(<tl/expected.hpp>)
		#include <tl/expected.hpp>
	#else
		#if __has_include(<nonstd/expected.hpp>)
			#include <nonstd/expected.hpp>
		#else
			#if __has_include(<rd/expected.hpp>)
				#include <rd/expected.hpp>
			#else
				#error "Your compiler doesn't support std::expected and you have no polyfills (https://github.com/TartanLlama/expected, https://github.com/martinmoene/expected-lite, https://github.com/RishabhRD/expected, the first one has CPack packaging implemented) installed."
			#endif
		#endif
	#endif
#endif

#ifdef _MSC_VER
	#define PARSEPATCH_EXPORT_API __declspec(dllexport)
	#define PARSEPATCH_IMPORT_API __declspec(dllimport)
	#define PARSEPATCH_PACKED __declspec(packed)
#else
	#define PARSEPATCH_PACKED [[gnu::packed]]
	#ifdef _WIN32
		#define PARSEPATCH_EXPORT_API [[gnu::dllexport]]
		#define PARSEPATCH_IMPORT_API [[gnu::dllimport]]
	#else
		#define PARSEPATCH_EXPORT_API [[gnu::visibility("default")]]
		#define PARSEPATCH_IMPORT_API
	#endif
#endif

#ifdef PARSEPATCH_EXPORTS
	#define PARSEPATCH_API PARSEPATCH_EXPORT_API
#else
	#define PARSEPATCH_API PARSEPATCH_IMPORT_API
#endif

namespace ParsePatch {

#if __has_include(<expected>)
	template <typename T, typename E>
	using expected = std::expected<T, E>;

	template <typename E>
	using unexpeced = std::unexpected<E>;

#else
	#if __has_include(<tl/expected.hpp>)
		template <typename T, typename E>
		using expected = tl::expected<T, E>;

		template <typename E>
		using unexpected = tl::unexpected<E>;
	#else
		#if __has_include(<nonstd/expected.hpp>)
			template <typename T, typename E>
			using expected = nonstd::expected<T, E>;

			template <typename E>
			using unexpected = nonstd::unexpected<E>;
		#else
			#if __has_include(<rd/expected.hpp>)
				template <typename T, typename E>
				using expected = rd::expected<T, E>;

				template <typename E>
				using unexpected = rd::unexpected<E>;
			#endif
		#endif
	#endif
#endif

enum struct BinaryHunkType : uint8_t {
	Literal,
	Delta
};

struct PARSEPATCH_API BinaryHunk {
	BinaryHunkType type;
	size_t size;
};

PARSEPATCH_API bool operator==(const BinaryHunk &lhs, const BinaryHunk &rhs);

/// File mode change
struct FileMode {
	uint32_t old, neo;
};

enum struct ParsepatchErrorCode : uint8_t {
	OK = 0,
	InvalidHunkHeader,
	NewModeExpected,
	NoFilename,
	InvalidString
};

struct PARSEPATCH_API ParsepatchError {
	ParsepatchErrorCode code;
	uint64_t line_or_str;

	constexpr operator bool() const {
		return code != ParsepatchErrorCode::OK;
	}
};

enum struct FileOpCode : uint8_t {
	New,	/// The file is neo with its mode
	Deleted,/// The file is deleted with its mode
	Renamed,/// The file is renamed
	Copied, /// The file is copied
	None	/// The file is touched
};

struct FileOp {
	FileOpCode code;
	uint32_t something = 0;

	/// The different file operation
	bool is_new_or_deleted();
};

template <typename T> using Result = expected<T, ParsepatchError>;

struct PARSEPATCH_API NumbersT {
	uint32_t old_count = 0, old_lines = 0, new_count = 0, new_lines = 0;
};

PARSEPATCH_API bool operator==(const NumbersT &lhs, const NumbersT &rhs);

struct PARSEPATCH_API LineReader {
	std::string_view buf;
	size_t line;

	bool is_empty() const;

	bool is_binary() const;

	bool is_rename_from();

	bool is_copy_from();

	bool is_new_file();

	bool is_triple_minus();

	bool is_triple_plus();

	bool is_index();

	bool is_deleted_file();

	size_t get_line() const;

	FileOp get_file_op();

	Result<NumbersT> parse_numbers();

	static Result<std::string_view> get_filename(std::string_view buf, size_t line);

	uint32_t parse_mode(const std::string_view start);

	std::string_view get_file(std::optional<std::string_view> slice, std::string starter) const;

	Result<std::tuple<std::string_view, std::string_view>> parse_files();
};

/// A type to handle lines in a diff
struct PARSEPATCH_API Diff {
	virtual ~Diff();

	/// Set the file info
	virtual void set_info(const std::string_view old_name, const std::string_view new_name, FileOp op, std::optional<std::vector<BinaryHunk>> binary_sizes, std::optional<FileMode> file_mode) = 0;

	/// Add a line in the diff
	///
	/// If a line is added (+) then old_line is 0 and new_line is the line
	/// in the destination file.
	///
	/// If a line is removed (-) then old_line is the line in the source
	/// file and new_line is 0.
	///
	/// If a line is nothing ( ) then old_line is the line in the source
	/// file and new_line is the line in the destination file.
	virtual void add_line(uint32_t old_line, uint32_t new_line, std::string_view &&line) = 0;

	/// A new hunk is created
	virtual void new_hunk() = 0;

	/// Close the diff: no more lines will be added
	virtual void close() = 0;
};

struct PARSEPATCH_API Patch {
	virtual ~Patch();

	/// Create a new diff where lines will be added
	virtual Diff *new_diff() = 0;

	/// Close the patch
	virtual void close() = 0;
};

typedef bool (*NextFilterF)(LineReader &);

namespace ScannerUtils {
size_t parse_usize(const std::string_view buf);

bool diff(LineReader &line);

bool useful(LineReader &line);

bool starter(LineReader &line);

bool mv(LineReader &_);

bool hunk_at(LineReader &line);

bool old_mode(LineReader &line);

bool hunk_change(LineReader &line);
};// namespace ScannerUtils

/// Type to read a patch
struct PARSEPATCH_API PatchReader {
	std::string_view buf;
	size_t pos;
	size_t line;
	std::optional<LineReader> last;
	std::ostream *tracing = nullptr;

	void reset();

	ParsepatchError by_buf(std::string_view buf, Patch &patch);

	size_t get_line() const;

	//<Diff D, Patch<D> P>
	ParsepatchError parse(Patch &patch);

	ParsepatchError parse_diff(LineReader &diff_line, Patch &patch);

	ParsepatchError parse_minus(LineReader &line, FileOp op, std::optional<FileMode> file_mode, Patch &patch);

	ParsepatchError parse_hunks(LineReader &line, Diff *diff);

	void parse_hunk(NumbersT lines_count, Diff *diff);

	void set_last(LineReader line);

	std::optional<LineReader> next(NextFilterF filter, bool return_on_false);

	void skip_until_empty_line();

	std::vector<BinaryHunk> skip_binary();
};

PARSEPATCH_API std::ostream &operator<<(std::ostream &s, const FileOp &op);

PARSEPATCH_API std::ostream &operator<<(std::ostream &s, const ParsepatchError &err);

PARSEPATCH_API std::ostream &operator<<(std::ostream &s, const LineReader &line);

};// namespace ParsePatch
