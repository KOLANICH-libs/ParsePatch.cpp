ParsePatch.cpp
==============
[![Libraries.io Status](https://img.shields.io/librariesio/github/KOLANICH-libs/ParsePatch.cpp.svg)](https://libraries.io/github/KOLANICH-libs/ParsePatch.cpp)

**We have moved to https://codeberg.org/KFmts/ParsePatch.cpp (note: the namespace has changed from `KOLANICH-libs` to `KFmts`, I have no Web UI access to GH anymore, so I cannot do the same restructuring here too), grab new versions there.**

Under the disguise of "better security" Micro$oft-owned GitHub has [discriminated users of 1FA passwords](https://github.blog/2023-03-09-raising-the-bar-for-software-security-github-2fa-begins-march-13/) while having commercial interest in success and wide adoption of [FIDO 1FA specifications](https://fidoalliance.org/specifications/download/) and [Windows Hello implementation](https://support.microsoft.com/en-us/windows/passkeys-in-windows-301c8944-5ea2-452b-9886-97e4d2ef4422) which [it promotes as a replacement for passwords](https://github.blog/2023-07-12-introducing-passwordless-authentication-on-github-com/). It will result in dire consequencies and is competely inacceptable, [read why](https://codeberg.org/KOLANICH/Fuck-GuanTEEnomo).

If you don't want to participate in harming yourself, it is recommended to follow the lead and migrate somewhere away of GitHub and Micro$oft. Here is [the list of alternatives and rationales to do it](https://github.com/orgs/community/discussions/49869). If they delete the discussion, there are certain well-known places where you can get a copy of it. [Read why you should also leave GitHub](https://codeberg.org/KOLANICH/Fuck-GuanTEEnomo).

---

A C++ port of https://github.com/mozilla/rust-parsepatch MPL-2.0-licensed patch file parsing library. API is not yet stable, minor changes can happen.

Corresponds to [`b3dbb48ecc73de3fecea20c7e8848a3fc2239772`](https://github.com/mozilla/rust-parsepatch/commit/b3dbb48ecc73de3fecea20c7e8848a3fc2239772) commit of the original library.

Goals:
* API similar to the one of the Rust library.
* But when needed, it can differ.
* Speed and efficiency.
* Safety.
* Usage as a shared lib.
* Packaging.

Differences:
1. Instead of extensive using of templates we use abstract interfaces.
2. Instead of buffers we use `string_view`s. Everything is pointers into the original buffer. If one wants the data to outlive them, he must convert them into allocated buffers.
3. Rust enumerations are translated into C++ classes with the first member conveying the code, and the rest containing the value matching the semantics.
4. So `by_path` and `ParsepatchErrorCode::IOError` are removed - user owns the memory and manages it himself. He can mmap a file, then the `string_view`s will be within the map. Or he can read the file into a memory buffer. `by_path` assummes that the library owns memory, not the user. It is not flexible.
5. `fmt` is replaced by the `operator<<` functions allowing output to the standard lib.
6. We use `parsepatch` namespace.
7. Some static functions moved from the class into `ScannerUtils` namespace.
8. `parse_numbers` returns `Result<NumbersT>`. `NumbersT` is a struct with semantic names for each component, not a tuple.
9. Parsing is done not by calling a static method, but by creating an object and reusing it.
10. Debug output can be enabled by assigning a pointer to a stream to `tracing` variable.
11. JSON-based tester was refatored. JSON structs have been separated from the `Patch` and `Diff` event listeners.
12. Testing is done using my `fileTestSuite` framework. Tests are moved into the separate repository.


### How to use
0. You need a compiler supporting C++20 and `std::expected`. If you don't have `std::expected`, you can install https://github.com/TartanLlama/expected or https://github.com/martinmoene/expected-lite or https://github.com/RishabhRD/expected .
1. Build and install into the system using CMake. CPack can be used to build native the packages.
2. Import into your CMake project:

```cmake
find_package(ParsePatch)
target_link_libraries("${PROJECT_NAME}" PRIVATE ParsePatch::libParsePatch)
```

Or you can use pkg-config

```bash
pkg-config --libs ParsePatch
```

3. `#include <ParsePatch.hpp>`
4. Subclass `Diff` and `Patch` and implement the event listener callbacks. 

```c++
struct DiffImpl: public ParsePatch::Diff {
	virtual void set_info(const std::string_view old_name, const std::string_view new_name, ParsePatch::FileOp op, std::optional<std::vector<ParsePatch::BinaryHunk>> binary_sizes, std::optional<FileMode> file_mode) override {
		...
	}

	virtual void add_line(uint32_t old_line, uint32_t new_line, std::string_view &&line) override {}

	virtual void new_hunk() override {}

	virtual void close() override {}
};

struct PatchImpl: public ParsePatch::Patch {
	DiffImpl diff{};
	virtual ParsePatch::Diff* new_diff() override {
		return &diff;
	}

	virtual void close() override {}
};
```

### Testing
There are 2 kinds of tests.
1. Basic tests testing low-level blocks. For them you need https://github.com/google/googletest installed in the system.
2. Testing against the dataset of etalon parses serialized into JSON. For them you need https://github.com/nlohmann/json and https://codeberg.org/fileTestSuite/fileTestSuite.c and their dependencies installed and the submodules populated.

Testing against JSON dataset can be done for example using

```bash
testabs-runner /usr/lib/x86_64-linux-gnu/libTestAbsBackend_GTest.so /usr/lib/x86_64-linux-gnu/libFileTestSuite_runner.so ./tests/json/libjson_tests.so ../tests/json/testDataset
```

where:

* `testabs-runner` is the runner glue executable.
* `libTestAbsBackend_GTest.so` is the TestAbs backend using GoogleTest. Installed as a part of [`TestAbs`](https://github.com/fileTestSuite/TestAbs.cpp) (dependency of `fileTestSuite`), can be replaced by backends to other testing frameworks, depending on your needs.
* `libFileTestSuite_runner.so` is the tester library for TestAbs, part of `fileTestSuite.c`, converts files into test cases.
* `../tests/json/testDataset` - path to testing dataset following the `fileTestSuite` spec. Fetched as a submodule.
