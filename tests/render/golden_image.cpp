#include "tests/render/golden_image.h"

#include <doctest/doctest.h>
#include <lodepng.h>

#if defined(_WIN32)
#include <Windows.h>
#endif

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
	// ---------------------------------------------------------------------
	// What the frame being checked is called
	// ---------------------------------------------------------------------

	// A doctest case name is an English sentence and a file name is not, so the
	// sentence becomes the file name by a rule with no exceptions in it:
	// lower-case, every run of anything that is not a letter or a digit becomes
	// one dash, and no dash at either end.
	//
	// The rule is lossy on purpose - two names differing only in punctuation
	// produce one slug - and that is safe for one reason: doctest already
	// refuses two cases with the same name, and a pair that differed only in a
	// comma would be a pair nobody could tell apart in a test log either.
	std::string slugify(const std::string& text)
	{
		std::string slug;
		slug.reserve(text.size());
		bool pending_dash = false;

		for (char character : text)
		{
			const bool is_digit = character >= '0' && character <= '9';
			const bool is_lower = character >= 'a' && character <= 'z';
			const bool is_upper = character >= 'A' && character <= 'Z';

			if (!is_digit && !is_lower && !is_upper)
			{
				pending_dash = true;
				continue;
			}

			if (pending_dash && !slug.empty())
			{
				slug.push_back('-');
			}
			pending_dash = false;
			slug.push_back(is_upper
				? static_cast<char>(character - 'A' + 'a')
				: character);
		}

		return slug;
	}

	// Which case is running, which subcases it is inside, and how many frames
	// each of those contexts has already read back.
	struct FrameNaming
	{
		std::string case_name;
		std::vector<std::string> subcases;
		std::vector<std::pair<std::string, int>> counts;
	};

	FrameNaming& naming()
	{
		static FrameNaming state;
		return state;
	}

	int& counter_for(const std::string& slug)
	{
		for (std::pair<std::string, int>& entry : naming().counts)
		{
			if (entry.first == slug)
			{
				return entry.second;
			}
		}
		naming().counts.emplace_back(slug, 0);
		return naming().counts.back().second;
	}

	// doctest tells a listener which case is running, and nothing else does.
	//
	// The alternative is a name handed to check_frame by every call site, which
	// is precisely the list this file exists so that nobody maintains: a case
	// added tomorrow is golden tomorrow, and a case that is renamed says so by
	// failing to find its image rather than by silently checking the old one.
	//
	// RESET ON REENTER AS WELL AS ON START, and that is not tidiness. A case
	// with two subcases runs its body twice, so a frame drawn OUTSIDE the
	// subcases is read back twice - it is the same frame both times and must
	// find the same name, not a second ordinal. THE ONE CASE SHAPED LIKE THAT
	// TODAY HAS ONE SUBCASE, NOT TWO: "read_back_buffer hands back exactly
	// back_buffer_size" reads its frame outside its single SUBCASE, so doctest
	// runs the body once and this reset has never yet been needed. It stays
	// because the shape it guards is one line away - a second subcase in that
	// case - and the failure would be an image quietly checked against the
	// wrong name.
	struct GoldenNames : doctest::IReporter
	{
		explicit GoldenNames(const doctest::ContextOptions&) {}

		void test_case_start(const doctest::TestCaseData& data) override
		{
			naming().case_name = data.m_name;
			naming().subcases.clear();
			naming().counts.clear();
		}

		void test_case_reenter(const doctest::TestCaseData& data) override
		{
			this->test_case_start(data);
		}

		void subcase_start(const doctest::SubcaseSignature& signature) override
		{
			naming().subcases.emplace_back(signature.m_name.c_str());
		}

		void subcase_end() override
		{
			if (!naming().subcases.empty())
			{
				naming().subcases.pop_back();
			}
		}

		void report_query(const doctest::QueryData&) override {}
		void test_run_start() override {}
		void test_run_end(const doctest::TestRunStats&) override {}
		void test_case_end(const doctest::CurrentTestCaseStats&) override {}
		void test_case_exception(const doctest::TestCaseException&) override {}
		void log_assert(const doctest::AssertData&) override {}
		void log_message(const doctest::MessageData&) override {}
		void test_case_skipped(const doctest::TestCaseData&) override {}
	};

	DOCTEST_REGISTER_LISTENER("labrador-golden-names", 0, GoldenNames);

	std::string frame_name()
	{
		std::string slug = slugify(naming().case_name);
		for (const std::string& subcase : naming().subcases)
		{
			slug += "--";
			slug += slugify(subcase);
		}

		// The ordinal appears from the second frame of a context onward, so the
		// common case - one case, one frame - keeps the sentence it came from
		// and nothing else. Counted per (case, subcase) rather than per case,
		// which is what stops a two-subcase body numbering its frames 1 and 2
		// on the first pass and 3 and 4 on the second.
		int& count = counter_for(slug);
		count++;
		if (count > 1)
		{
			slug += "--";
			slug += std::to_string(count);
		}
		return slug;
	}

	// ---------------------------------------------------------------------
	// PNG, through lodepng
	// ---------------------------------------------------------------------

	// A golden set is worth having only if a person can look at it, and a
	// change to one is reviewable only if the review tool can draw it. PNG is
	// what renders inline in a diff; the same 64x64 frame raw is sixteen
	// kilobytes nobody can read and nobody will check.
	//
	// BOUGHT, BECAUSE IT IS A FORMAT EDGE (T9), and one codec on every platform
	// rather than each platform's own. A browser has no codec this file can
	// call synchronously, and two codecs reading one set would be two claims
	// about what the set holds. lodepng is a test
	// dependency only (vcpkg.json), and nothing the engine ships links it.
	//
	// LOSSLESS BOTH WAYS, AND NOT ON TRUST. The encoder may store a frame in
	// whatever PNG colour type holds it exactly - RGB when every alpha is 255,
	// a palette when there are few enough colours - and read_png asks
	// lodepng::decode for 8-bit RGBA whatever it chose, with no colour
	// management anywhere on the path. The check-mode run that follows every regeneration compares each
	// frame against the file just written from it, so a build where that
	// stopped being true would fail once per image in the set.
	bool read_file(const std::filesystem::path& path,
		std::vector<unsigned char>& bytes)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			return false;
		}
		bytes.assign(std::istreambuf_iterator<char>(file),
			std::istreambuf_iterator<char>());
		return !file.bad();
	}

	bool write_png(const std::filesystem::path& path, int width, int height,
		const std::vector<unsigned char>& rgba, std::string& error)
	{
		std::vector<unsigned char> encoded;
		const unsigned failed = lodepng::encode(encoded, rgba,
			static_cast<unsigned>(width), static_cast<unsigned>(height));
		if (failed != 0)
		{
			error = lodepng_error_text(failed);
			return false;
		}

		std::error_code ignored;
		std::filesystem::create_directories(path.parent_path(), ignored);

		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(encoded.data()),
			static_cast<std::streamsize>(encoded.size()));
		if (!file)
		{
			error = "could not write it";
			return false;
		}
		return true;
	}

	bool read_png(const std::filesystem::path& path, int& width, int& height,
		std::vector<unsigned char>& rgba, std::string& error)
	{
		std::vector<unsigned char> encoded;
		if (!read_file(path, encoded))
		{
			error = "it is not there, or not readable";
			return false;
		}

		unsigned decoded_width = 0;
		unsigned decoded_height = 0;
		const unsigned failed = lodepng::decode(rgba, decoded_width,
			decoded_height, encoded, LCT_RGBA, 8);
		if (failed != 0)
		{
			error = std::string("it would not decode: ") +
				lodepng_error_text(failed);
			return false;
		}

		width = static_cast<int>(decoded_width);
		height = static_cast<int>(decoded_height);
		return true;
	}

	// ---------------------------------------------------------------------
	// Where the images live
	// ---------------------------------------------------------------------

	// LABRADOR_GOLDEN_DIR is the source folder, baked in by this folder's
	// CMakeLists, and it is deliberately NOT the copy-beside-the-executable
	// arrangement content/ uses. content/ is copied because loading it through
	// a relative path is part of what those cases exercise; a golden set is
	// exercised by nothing and is only ever read by this file and by a person.
	// A baked path means a regeneration writes where the images have to be
	// reviewed, and a deleted image is missing on the next run instead of
	// lingering in a build directory nobody cleaned.
	//
	// IN A BROWSER IT IS "/golden", the same folder embedded in the module at
	// link time, because a page has no file system but the one it carries. It
	// is still the source folder's contents, read rather than copied from a
	// build directory, and a change to an image relinks the page.
	std::filesystem::path golden_path(const std::string& slug)
	{
		return std::filesystem::path(LABRADOR_GOLDEN_DIR) / (slug + ".png");
	}

	// Beside the executable, because ctest runs from the build directory and a
	// failing run should leave its evidence where a CI job can collect it
	// without knowing anything about the source tree. In a browser that is the
	// page's memory, and tests/render/web/run_page.mjs copies the folder out
	// beside the page when the run ends, so a rejected frame lands in the same
	// place on both.
	std::filesystem::path actual_path(const std::string& slug)
	{
		return std::filesystem::path("./golden-actual") /
			(slug + ".actual.png");
	}

	// Whether this run is a regeneration rather than a check.
	//
	// An environment variable rather than a command-line flag, and the reason
	// is tests/render/test_main.cpp: it is DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
	// and it is shared with RenderTests, so doctest owns argv and taking it
	// back to add one option would change the entry point of a second
	// executable that has no golden set. The variable reads the same from
	// ctest, from CMake and from a shell, which is all a regeneration needs.
	//
	// NEVER IN A BROWSER. A page cannot see the environment of the shell that
	// started it, and what it writes stays in its memory, so a regeneration
	// there would write a set nobody could review. The set is regenerated on
	// the desktop, and the browser is held to it.
	bool dumping()
	{
#if defined(_WIN32)
		return GetEnvironmentVariableW(L"LABRADOR_GOLDEN_DUMP", nullptr, 0) != 0;
#else
		return false;
#endif
	}

	// ---------------------------------------------------------------------
	// How far a channel may move before it is a difference
	// ---------------------------------------------------------------------

	// ONE SET HAS TO SERVE SEVERAL RASTERISERS, and that is the whole of why
	// this number is not zero.
	//
	// RenderPixelTests runs against a hardware adapter on a machine with a
	// driver and against whatever a build machine offers, which is not the
	// same rasteriser and cannot be made to be one. There is no arrangement
	// that lets one checked-in set be exact for both: the OpenGL backend cannot
	// run on a software rasteriser at all - Windows' fallback for GL is the 1.1
	// GDI implementation - so a set generated on one would be inexact for every
	// GL run in exchange for being exact in CI. Something has to give, and the
	// honest place is here, stated with the measurement that set it.
	//
	// WHAT CI'S RASTERISERS ARE. The Direct3D jobs draw on an adapter no log
	// names: x64-release passes RenderPixelTests with the WARP fallback
	// compiled out, so the runner has a DXGI adapter that is not flagged
	// software, and nothing in ctest's output describes it
	// (.github/workflows/ci.yml carries the evidence). The wasm job's is named,
	// because tests/render/web/run_page.mjs prints it: Chrome's SwiftShader,
	// behind ANGLE's Vulkan path.
	//
	// MEASURED, NOT GUESSED, with this number set to zero, against the set of
	// fifty-eight. This machine's GPU writes every frame byte for byte through
	// all four desktop backends and through Chrome's ANGLE. SwiftShader writes
	// every frame byte for byte too. WARP differs on two frames, both of them
	// blend cases, by 1. So 8 is eight times what any rasteriser measured here
	// needs, and the margin is for the one nobody has measured: CI's Direct3D
	// adapter.
	//
	// THE TEXT FRAMES ARE AMONG THE EXACT ONES BECAUSE THEIR ATLAS IS DECODED
	// BEFORE A DEVICE SEES IT. Decoded by each device, the font's BC2 atlas put
	// the same three rasterisers apart on every text frame - in green, by 5
	// where a glyph's fringe sampled an interpolated colour and by 9 where two
	// fringes overlap and the blend compounds it - because no specification
	// fixes how a decoder rounds a colour a third of the way between two
	// endpoints. That was the only term here that needed an allowance, it is
	// not one this engine decides, and pixel_tests.cpp's decoded_bc2 is where
	// it is taken out. The device's own BC2 decoder is still held to pixels,
	// by a case whose blocks decode exactly on every decoder.
	//
	// AND IT COSTS ALMOST NOTHING IT WAS BUYING. The drift this file exists to
	// catch is a hand-copied term going wrong in one backend and not the other
	// - a pixels-to-clip constant, an index winding, a camera prologue, a blend
	// factor. Every one of those moves a glyph edge from 0 to 200-odd or blacks
	// a destination out entirely. Nothing in that class hides under 8.
	//
	// IF A RASTERISER EVER NEEDS MORE THAN THIS, the answer is to look at the
	// images and find the term that moved, not to raise the number. A
	// tolerance that grows to fit whatever failed is not a tolerance, and this
	// one is already the only inexact statement in the pixel contract. The
	// direction it should move is down, once CI's Direct3D adapter has been
	// measured against the set.
	constexpr int ALLOWED_CHANNEL_DRIFT = 8;
}

void golden::check_frame(int width, int height,
	const std::vector<unsigned char>& rgba)
{
	const std::string slug = frame_name();
	const std::filesystem::path expected_file = golden_path(slug);

	// Every failure below leaves through the one CHECK_MESSAGE at the bottom,
	// carrying a report built here. doctest's message macros splice their
	// arguments into an expression of their own, so a string assembled with +
	// inside one of them will not compile - which is a good enough reason to
	// have a single place that reports and a single place that writes out the
	// frame somebody is going to want to look at.
	std::string report;

	if (dumping())
	{
		// AN IMAGE WHOSE PIXELS DID NOT MOVE IS LEFT AS IT IS, byte for byte,
		// so that what a regeneration changes in the tree is exactly the set
		// of frames that changed. Re-encoding every file would show a reviewer
		// fifty-eight modified images whenever the encoder differed from the
		// one that wrote them, with no way to tell which ones to look at.
		int existing_width = 0;
		int existing_height = 0;
		std::vector<unsigned char> existing;
		std::string unread;
		if (read_png(expected_file, existing_width, existing_height, existing,
				unread) &&
			existing_width == width && existing_height == height &&
			existing == rgba)
		{
			return;
		}

		std::string error;
		if (!write_png(expected_file, width, height, rgba, error))
		{
			report = "could not write " + expected_file.string() + ": " + error;
		}
		CHECK_MESSAGE(report.empty(), report);
		return;
	}

	std::string error;
	int expected_width = 0;
	int expected_height = 0;
	std::vector<unsigned char> expected;
	const bool loaded = read_png(expected_file, expected_width,
		expected_height, expected, error);

	const size_t byte_count = static_cast<size_t>(width) *
		static_cast<size_t>(height) * 4;

	// A frame with no image to check against is a frame nobody has ever looked
	// at, which is the state this file exists to end (T6). Loud, and carrying
	// the one command that fixes it.
	if (!loaded)
	{
		report = "no golden image for this frame: " + expected_file.string() +
			" (" + error + "). Regenerate the set with LABRADOR_GOLDEN_DUMP=1"
			" and review every image the regeneration changes.";
	}
	else if (expected_width != width || expected_height != height ||
		rgba.size() != byte_count || expected.size() != byte_count)
	{
		// A backend that drew the right picture at the wrong size has not drawn
		// the right picture, and comparing the two byte for byte from here on
		// would read off the end of one of them.
		report = "golden " + slug + " is " + std::to_string(expected_width) +
			"x" + std::to_string(expected_height) + " (" +
			std::to_string(expected.size()) + " bytes), the frame is " +
			std::to_string(width) + "x" + std::to_string(height) + " (" +
			std::to_string(rgba.size()) + " bytes)";
	}
	else
	{
		size_t differing = 0;
		size_t beyond = 0;
		size_t first = byte_count;
		int worst = 0;

		for (size_t index = 0; index < byte_count; index += 4)
		{
			int delta = 0;
			for (size_t channel = 0; channel < 4; channel++)
			{
				const int difference =
					static_cast<int>(rgba[index + channel]) -
					static_cast<int>(expected[index + channel]);
				const int magnitude = difference < 0 ? -difference : difference;
				if (magnitude > delta)
				{
					delta = magnitude;
				}
			}

			if (delta == 0)
			{
				continue;
			}

			differing++;
			if (delta > worst)
			{
				worst = delta;
			}
			if (delta > ALLOWED_CHANNEL_DRIFT)
			{
				beyond++;
				if (first == byte_count)
				{
					first = index;
				}
			}
		}

		if (beyond != 0)
		{
			const size_t pixel = first / 4;
			const size_t x = pixel % static_cast<size_t>(width);
			const size_t y = pixel / static_cast<size_t>(width);

			// Every number a reader wants before opening the two images: how
			// much of the frame moved, how far any one channel moved, and where
			// to start looking. One pixel by one level is a filter or a fill
			// rule; half the frame by 255 is a transform.
			report = "golden mismatch: " + slug + "\n  " +
				std::to_string(beyond) + " of " +
				std::to_string(byte_count / 4) + " pixels differ by more than " +
				std::to_string(ALLOWED_CHANNEL_DRIFT) + " (" +
				std::to_string(differing) +
				" differ at all), worst channel by " + std::to_string(worst) +
				"\n  first at (" + std::to_string(x) + ", " +
				std::to_string(y) + "): golden rgba(" +
				std::to_string(expected[first]) + ", " +
				std::to_string(expected[first + 1]) + ", " +
				std::to_string(expected[first + 2]) + ", " +
				std::to_string(expected[first + 3]) + "), frame rgba(" +
				std::to_string(rgba[first]) + ", " +
				std::to_string(rgba[first + 1]) + ", " +
				std::to_string(rgba[first + 2]) + ", " +
				std::to_string(rgba[first + 3]) + ")\n  golden " +
				expected_file.string();
		}
	}

	if (!report.empty())
	{
		std::string ignored;
		if (write_png(actual_path(slug), width, height, rgba, ignored))
		{
			report += "\n  frame  " + actual_path(slug).string();
		}
	}

	CHECK_MESSAGE(report.empty(), report);
}
