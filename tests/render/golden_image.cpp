#include "tests/render/golden_image.h"

#include <doctest/doctest.h>

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
	using Microsoft::WRL::ComPtr;

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
	// PNG, through the codec Windows already has
	// ---------------------------------------------------------------------

	// A golden set is worth having only if a person can look at it, and a
	// change to one is reviewable only if the review tool can draw it. PNG is
	// what renders inline in a diff; the same 64x64 frame raw is sixteen
	// kilobytes nobody can read and nobody will check. WIC is the in-box
	// encoder and decoder, so this costs no dependency - the format edge is
	// bought (T9) - and it is the same lossless path in both directions,
	// 32bpp RGBA in and 32bpp RGBA out, with no colour management anywhere on
	// it. A golden file therefore holds the bytes read_back_buffer handed over,
	// and a comparison against one is a comparison against those bytes.
	IWICImagingFactory* wic_factory()
	{
		static ComPtr<IWICImagingFactory> factory =
			[]() -> ComPtr<IWICImagingFactory>
			{
				// RPC_E_CHANGED_MODE means something in this process got here
				// first and chose the other apartment, which WIC does not mind.
				const HRESULT initialised =
					CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				if (FAILED(initialised) && initialised != RPC_E_CHANGED_MODE)
				{
					return nullptr;
				}

				ComPtr<IWICImagingFactory> created;
				if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
					CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&created))))
				{
					return nullptr;
				}
				return created;
			}();

		return factory.Get();
	}

	bool write_png(const std::filesystem::path& path, int width, int height,
		const std::vector<unsigned char>& rgba, std::string& error)
	{
		IWICImagingFactory* factory = wic_factory();
		if (factory == nullptr)
		{
			error = "WIC is unavailable";
			return false;
		}

		std::error_code ignored;
		std::filesystem::create_directories(path.parent_path(), ignored);

		ComPtr<IWICStream> stream;
		if (FAILED(factory->CreateStream(&stream)) ||
			FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)))
		{
			error = "could not open it for writing";
			return false;
		}

		ComPtr<IWICBitmapEncoder> encoder;
		ComPtr<IWICBitmapFrameEncode> frame;
		ComPtr<IPropertyBag2> properties;
		if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr,
				&encoder)) ||
			FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
			FAILED(encoder->CreateNewFrame(&frame, &properties)) ||
			FAILED(frame->Initialize(properties.Get())) ||
			FAILED(frame->SetSize(static_cast<UINT>(width),
				static_cast<UINT>(height))))
		{
			error = "could not start a PNG frame";
			return false;
		}

		// The PNG encoder chooses its own storage format and does not have to
		// choose the one this buffer is in - on this Windows it answers BGRA to
		// a request for RGBA - so the frame is handed a bitmap that knows what
		// it holds rather than raw bytes, and WIC does whatever swizzle its
		// choice implies.
		//
		// Lossless either way, and not on trust: every format the PNG encoder
		// negotiates to from a 32bpp RGBA source carries eight bits per channel
		// and an alpha channel, so read_png returns exactly these bytes - and
		// the check-mode run that follows every regeneration is what says so,
		// because it compares the frame against the file just written from it.
		// A build where that stopped being true would fail fifty-two times over,
		// once per image in the set.
		WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
		ComPtr<IWICBitmap> source;
		if (FAILED(frame->SetPixelFormat(&format)) ||
			FAILED(factory->CreateBitmapFromMemory(static_cast<UINT>(width),
				static_cast<UINT>(height), GUID_WICPixelFormat32bppRGBA,
				static_cast<UINT>(width) * 4,
				static_cast<UINT>(rgba.size()),
				const_cast<BYTE*>(rgba.data()), &source)))
		{
			error = "could not describe the frame to the PNG encoder";
			return false;
		}

		if (FAILED(frame->WriteSource(source.Get(), nullptr)) ||
			FAILED(frame->Commit()) ||
			FAILED(encoder->Commit()))
		{
			error = "could not write the pixels";
			return false;
		}

		return true;
	}

	bool read_png(const std::filesystem::path& path, int& width, int& height,
		std::vector<unsigned char>& rgba, std::string& error)
	{
		IWICImagingFactory* factory = wic_factory();
		if (factory == nullptr)
		{
			error = "WIC is unavailable";
			return false;
		}

		ComPtr<IWICBitmapDecoder> decoder;
		if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr,
			GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
		{
			error = "it is not there, or not readable";
			return false;
		}

		ComPtr<IWICBitmapFrameDecode> frame;
		ComPtr<IWICFormatConverter> converter;
		if (FAILED(decoder->GetFrame(0, &frame)) ||
			FAILED(factory->CreateFormatConverter(&converter)) ||
			FAILED(converter->Initialize(frame.Get(),
				GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr,
				0.0, WICBitmapPaletteTypeCustom)))
		{
			error = "it would not decode as 32bpp RGBA";
			return false;
		}

		UINT decoded_width = 0;
		UINT decoded_height = 0;
		if (FAILED(converter->GetSize(&decoded_width, &decoded_height)))
		{
			error = "it would not say how big it is";
			return false;
		}

		const UINT stride = decoded_width * 4;
		rgba.resize(static_cast<size_t>(stride) * decoded_height);
		if (FAILED(converter->CopyPixels(nullptr, stride,
			static_cast<UINT>(rgba.size()), rgba.data())))
		{
			error = "its pixels would not copy out";
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
	std::filesystem::path golden_path(const std::string& slug)
	{
		return std::filesystem::path(LABRADOR_GOLDEN_DIR) / (slug + ".png");
	}

	// Beside the executable, because ctest runs from the build directory and a
	// failing run should leave its evidence where a CI job can collect it
	// without knowing anything about the source tree.
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
	bool dumping()
	{
		return GetEnvironmentVariableW(L"LABRADOR_GOLDEN_DUMP", nullptr, 0) != 0;
	}

	// ---------------------------------------------------------------------
	// How far a channel may move before it is a difference
	// ---------------------------------------------------------------------

	// ONE SET HAS TO SERVE TWO RASTERISERS, and that is the whole of why this
	// number is not zero.
	//
	// RenderPixelTests runs against a hardware adapter on a machine with a
	// driver and against whatever adapter a build machine offers, which is not
	// the same rasteriser and cannot be made to be one. There is no arrangement
	// that lets one checked-in set be exact for both: the OpenGL backend cannot
	// run on a software rasteriser at all - Windows' fallback for GL is the 1.1
	// GDI implementation - so a set generated on one would be inexact for every
	// GL run in exchange for being exact in CI. Something has to give, and the
	// honest place is here, stated with the measurement that set it.
	//
	// WHAT CI'S RASTERISER IS, THIS FILE DOES NOT KNOW AND SHOULD NOT CLAIM TO.
	// It said WARP until a job log was read: the x64-release job passes
	// RenderPixelTests with the WARP fallback compiled out, so the runner has a
	// DXGI adapter that is not flagged software and nothing in a log names it
	// (.github/workflows/ci.yml carries the evidence). The number below is
	// therefore an allowance against a rasteriser this repository has measured
	// ONE candidate for, and the candidate is the strictest one available here.
	//
	// MEASURED, NOT GUESSED. Held against the same set, this machine's GPU and
	// its WARP differ on 19 of the 47 frames. Eighteen of those 19 draw text, so
	// their pixels came out of the block-compressed font atlas through a
	// filter; the nineteenth is a blend case. The worst channel across all of
	// them is 7 and most are 5, and the other twenty-eight frames - the ones
	// carrying flat texels through no filter, the six multi-view ones among
	// them - are identical to the byte. THE SET WAS 47 WHEN THAT WAS MEASURED
	// AND IS 50 NOW, so 19 and 28 no longer sum to it: the two read-back-then-
	// present frames and the re-loaded-name frame joined afterwards and are in
	// neither number. All three carry flat texels through no filter, which is
	// the half of the set the measurement found identical. That is the shape of a decode-and-filter
	// difference, and neither term is one this engine decides - pixel_tests.cpp
	// has said from the beginning that "the coverage value at any one pixel is
	// a fact about the compressor", which is why its text cases are written as
	// relationships in the first place.
	//
	// AND IT COSTS ALMOST NOTHING IT WAS BUYING. The drift this file exists to
	// catch is a hand-copied term going wrong in one backend and not the other
	// - a pixels-to-clip constant, an index winding, a camera prologue, a blend
	// factor. Every one of those moves a glyph edge from 0 to 200-odd or blacks
	// a destination out entirely. Nothing in that class hides under 8.
	//
	// IF A THIRD RASTERISER EVER NEEDS MORE THAN THIS, the answer is to look at
	// the images and take the compression out of the test atlas, not to raise
	// the number. A tolerance that grows to fit whatever failed is not a
	// tolerance, and this one is already the only inexact statement in the
	// pixel contract.
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
