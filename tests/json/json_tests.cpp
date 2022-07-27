#include <cstdint>

#include <iostream>
#include <optional>
#include <range/v3/all.hpp>
#include <string>
#include <vector>

#include "ParsePatch.hpp"
#include "json.hpp"

#include <fileTestSuite/fileTestSuite.h>
#include <fileTestSuite/fileTestSuiteRunner.hpp>

using namespace ParsePatch;

struct DiffImpl: public Diff {
	ParsedDiff *pd = nullptr;

	virtual void set_info(const std::string_view old_name, const std::string_view new_name, FileOp op, std::optional<std::vector<BinaryHunk>> binary_sizes, std::optional<FileMode> file_mode) override {
		switch(op.code) {
			case FileOpCode::New:
				pd->neo = true;
				pd->filename = new_name;
				pd->file_mode = {FileModeChange {.old = 0, .neo = op.something}};
				break;
			case FileOpCode::Deleted:
				pd->deleted = true;
				pd->filename = old_name;
				pd->file_mode = {FileModeChange {.old = op.something, .neo = 0}};
				break;
			case FileOpCode::Renamed:
				pd->filename = new_name;
				pd->renamed_from = old_name;
				break;
			default:
				pd->filename = new_name;
		}
		pd->binary = !(!binary_sizes);

		if(!pd->file_mode) {
			if(file_mode) {
				pd->file_mode = {FileModeChange {
					.old = file_mode->old,
					.neo = file_mode->neo,
				}};
			}
		}
	}

	virtual void add_line(uint32_t old_line, uint32_t new_line, std::string_view &&line) override {
		if(old_line == 0) {
			pd->hunks[pd->hunks.size() - 1].lines.emplace_back(LineChange {
				.line = new_line,
				.deleted = false,
				.data = std::string(line),
			});
		} else if(new_line == 0) {
			pd->hunks[pd->hunks.size() - 1].lines.emplace_back(LineChange {
				.line = old_line,
				.deleted = true,
				.data = std::string(line),
			});
		}
	}

	virtual void new_hunk() override {
		pd->hunks.emplace_back(Hunk {});
	}

	virtual void close() {
	}
};

struct PatchImpl: public Patch {
	ParsedPatch &pp;
	DiffImpl di;

	PatchImpl(ParsedPatch &pp): pp(pp) {
	}

	virtual Diff *new_diff() override {
		di.pd = &(pp.diffs.emplace_back(ParsedDiff {}));
		return &di;
	}

	virtual void close() override {
	}
};

std::ostream &operator<<(std::ostream &s, const LineChange &c) {
	return s << "line: " << c.line << ", deleted: " << c.deleted << ", data: " << c.data;
}

std::ostream &operator<<(std::ostream &s, const ParsedDiff &di) {
	s << "filename: " << di.filename << std::endl;
	s << "new: " << di.neo << ", deleted: " << di.deleted << ", binary: " << di.binary << std::endl;
	s << "copied_from: " << (di.copied_from ? *di.copied_from : "None") << ", renamed_from: " << (di.renamed_from ? *di.renamed_from : "None") << std::endl;

	if(di.file_mode) {
		s << "old mode: 0" << std::oct << (*di.file_mode).old << ", new mode: 0" << (*di.file_mode).neo << std::endl;
	}
	for(auto &hunk: di.hunks) {
		s << " @@" << std::endl;

		for(auto line: hunk.lines) {
			s << " - " << line << std::endl;
		}
	}
	return s;
}

std::ostream &operator<<(std::ostream &s, const ParsedPatch &pi) {
	for(auto &diff: pi.diffs) {
		s << "Diff:\n"
		  << diff << std::endl;
	}
	return s;
}

struct ParsePatchOutputBufferTester: public OutputBufferTester {
	ParsedPatch json_patch;
	ParsedPatch parsed_patch;
	PatchImpl pi {parsed_patch};

	ParsePatchOutputBufferTester(pseudospan &etalonSpan, FTSTestToolkit *ctx): OutputBufferTester(etalonSpan, ctx) {
		std::string_view fileView(reinterpret_cast<char *>(&etalonSpan[0]), etalonSpan.size());
		std::string fileContents {fileView};// fuck, we have to do this

		json_patch = decltype(json_patch)::fromJSONBuffer(fileView);

		/*JS::ParseContext parseContext(fileContents);
		if(parseContext.parseTo(json_patch) != JS::Error::NoError) {
			std::cerr << "failed to parse etalon as JSON" << std::endl;
			//return 1;
		}*/
	}
	virtual ~ParsePatchOutputBufferTester() = default;

	virtual pseudospan getEtalonChunkBoundariesAndUpdateCumLen(size_t len) {
	}
	virtual bool testChunkProcessing(unsigned char *producedChunk, unsigned len) {
		auto bufferChunkBounds = pseudospan(producedChunk, len);

		/*
		if(path == std::filesystem::path("./tests/patches/6f5fc0d644dd.patch")) {
			auto &modes = patch.diffs[0].file_mode;
			context->tk->expectEqual(modes->old, 0100644);
			context->tk->expectEqual(modes->neo, 0100755);
		}

		if(path == std::filesystem::path("./tests/patches/d8571fb523a0.patch")){
			auto &modes = patch.diffs[1].file_mode;
			context->tk->expectEqual(modes->neo, 0100644);
			context->tk->expectEqual(modes->old, 0);
		}

		if(path == std::filesystem::path("./tests/patches/d7a700707ddb.patch")) {
			std::cerr << patch << std::endl;
			for(auto diff: patch.diffs) {
				if(diff.filename == "layout/style/test/test_overscroll_behavior_pref.html") {
					auto &modes = diff.file_mode;
					context->tk->expectEqual(modes->neo, 0);
					context->tk->expectEqual(modes->old, 0100644);
				}
			}
		}
		*/

		//eprintln!("{:?}", patch);

		context->tk->expectEqual(json_patch.diffs.size(), parsed_patch.diffs.size());
		/*"Not the same length in patch {:?}: {} ({} expected)",
		path,
		patch.diffs.size(),
		json.diffs.size(),*/
		for(auto [cj, cp]: ranges::views::zip(json_patch.diffs, parsed_patch.diffs)) {
			context->tk->expectEqual(
				cj.filename, cp.filename /*,
				"Not the same filename: {} ({} expected)",
				cp.filename,
				cj.filename*/
			);
			context->tk->expectTrue(
				cj.copied_from == cp.renamed_from || cj.copied_from == cp.copied_from /*,
				"Not renamed/copied (in {}): {:?} {:?} {:?}",
				cp.filename,
				cj.copied_from,
				cp.renamed_from,
				cp.copied_from*/
			);
			context->tk->expectEqual(
				cj.hunks.size(), cp.hunks.size() /*,
				"Not the same length for hunks: {} ({} expected)",
				cp.hunks.size(),
				cj.hunks.size()*/
			);
			for(auto [hj, hp]: ranges::views::zip(cj.hunks, cp.hunks)) {
				context->tk->expectEqual(
					hj.lines.size(), hp.lines.size() /*,
					"Not the same length for changed lines: {} ({} expected)",
					hp.lines.size(),
					hj.lines.size()*/
				);
				for(auto [lj, lp]: ranges::views::zip(hj.lines, hp.lines)) {
					context->tk->expectEqual(lp.line, lj.line);
					context->tk->expectEqual(lp.deleted, lj.deleted);
					context->tk->expectEqual(lp.data, lj.data);
					/*,
					"Not the same line change: {:?} ({:?} expected",
					lp,
					lj*/
				}
			}
		}

		return true;
	}
	virtual void reset() {
	}
};

struct ParsePatchTestingContext: public TesteeSpecificContext {
	ParsePatchTestingContext()
		: TesteeSpecificContext() {
		//shouldSwapChallengeResponse = true;
	}
	ParsepatchError parsingError = {ParsepatchErrorCode::OK, 0};

	virtual ~ParsePatchTestingContext() = default;
	virtual void verifyAxillaryResults(FTSTestToolkit *ctx) override {
		ctx->tk->expectEqual(static_cast<uint8_t>(this->parsingError.code), static_cast<uint8_t>(ParsepatchErrorCode::OK));
	}

	virtual std::unique_ptr<OutputBufferTester> makeOutputBufferTester(pseudospan &etalonSpan, FTSTestToolkit *ctx) override {
		return std::unique_ptr<OutputBufferTester> {new ParsePatchOutputBufferTester(etalonSpan, ctx)};
	}

	virtual void executeTestee(pseudospan &challenge_data_span, OutputBufferTester &outBuffTester) override {
		auto &ppOutBuffTester = reinterpret_cast<ParsePatchOutputBufferTester &>(outBuffTester);

		//if path != std::filesystem::path("./tests/output/b8802b591ce2.json") {
		//	continue;
		//}

		/*
		auto path1 = std::filesystem::path("./tests/patches/") / path.stem().replace_extension("patch");

		std::cerr << "Parse patch " << path1 << std::endl;
		*/

		PatchReader r;
		std::string_view challenge_view(reinterpret_cast<char *>(&challenge_data_span[0]), challenge_data_span.size());
		parsingError = r.by_buf(challenge_view, ppOutBuffTester.pi);

		/*if(path.string().ends_with(".patch")) {
			std::cerr << "Patch:\n" << patch << std::endl;
		}*/

		//////////////////////
	}
};

TesteeSpecificContext *testeeSpecificContextFactory() {
	return new ParsePatchTestingContext();
}
