#include <doctest/doctest.h>

#include "engine/audio/wav_file.h"
#include "engine/audio/xwb_file.h"
#include "tests/scratch_file.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	using labrador::WaveBankData;
	using labrador::WaveData;
	using labrador::WaveEncoding;
	using labrador::read_wav_file;
	using labrador::read_xwb_file;
	using labrador_tests::ScratchFile;
	using labrador_tests::put_u32_le;
	using Bytes = std::vector<unsigned char>;

	void append_u16(Bytes& bytes, uint16_t value)
	{
		bytes.push_back(static_cast<unsigned char>(value & 0xFFu));
		bytes.push_back(static_cast<unsigned char>(value >> 8));
	}

	void append_u32(Bytes& bytes, uint32_t value)
	{
		for (std::size_t index = 0; index < 4; ++index)
		{
			bytes.push_back(static_cast<unsigned char>((value >> (index * 8)) & 0xFFu));
		}
	}

	void put_u16(Bytes& bytes, std::size_t at, uint16_t value)
	{
		REQUIRE(at + 2 <= bytes.size());
		bytes[at] = static_cast<unsigned char>(value & 0xFFu);
		bytes[at + 1] = static_cast<unsigned char>(value >> 8);
	}

	// A standard PCM fmt payload, independent of the reader's chunk walk.
	Bytes format(uint16_t channels = 1, uint32_t rate = 8000, uint16_t bits = 16)
	{
		Bytes bytes;
		append_u16(bytes, 1);
		append_u16(bytes, channels);
		append_u32(bytes, rate);
		const uint16_t alignment = static_cast<uint16_t>(channels * (bits / 8));
		append_u32(bytes, rate * alignment);
		append_u16(bytes, alignment);
		append_u16(bytes, bits);
		return bytes;
	}

	Bytes pcm16(const std::vector<int16_t>& samples)
	{
		Bytes bytes;
		for (const int16_t sample : samples)
		{
			append_u16(bytes, static_cast<uint16_t>(sample));
		}
		return bytes;
	}

	struct Chunk
	{
		std::string name;
		Bytes contents;
	};

	Bytes wav(const std::vector<Chunk>& chunks)
	{
		Bytes bytes = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E' };
		for (const Chunk& chunk : chunks)
		{
			REQUIRE(chunk.name.size() == 4);
			bytes.insert(bytes.end(), chunk.name.begin(), chunk.name.end());
			append_u32(bytes, static_cast<uint32_t>(chunk.contents.size()));
			bytes.insert(bytes.end(), chunk.contents.begin(), chunk.contents.end());
			// Padding is alignment, not PCM, and need not contain a zero.
			if ((chunk.contents.size() & 1u) != 0) bytes.push_back(0xA5u);
		}
		put_u32_le(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
		return bytes;
	}

	Bytes sampler(uint32_t start, uint32_t end, uint32_t type = 0,
		uint32_t fraction = 0, uint32_t play_count = 0, uint32_t loop_count = 1)
	{
		Bytes bytes(36 + 24 * static_cast<std::size_t>(loop_count), 0);
		put_u32_le(bytes, 28, loop_count);
		if (loop_count != 0)
		{
			put_u32_le(bytes, 40, type);
			put_u32_le(bytes, 44, start);
			put_u32_le(bytes, 48, end);
			put_u32_le(bytes, 52, fraction);
			put_u32_le(bytes, 56, play_count);
		}
		return bytes;
	}

	WaveData read_wave(const Bytes& bytes)
	{
		const ScratchFile file("pcm_fixture.wav", bytes);
		return read_wav_file(file.path());
	}

	void require_refused(const Bytes& bytes)
	{
		const ScratchFile file("refused_pcm.wav", bytes);
		try
		{
			read_wav_file(file.path());
			FAIL("the malformed WAV was accepted");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find(file.path()) != std::string::npos);
		}
	}
}

TEST_CASE("PCM source WAVs decode to the exact samples retained by the packaged XWB")
{
	const WaveBankData bank = read_xwb_file("./content/tones.xwb");
	REQUIRE(bank.waves.size() == 3);
	for (const WaveData& expected : bank.waves)
	{
		CAPTURE(expected.name);
		const WaveData actual = read_wav_file("./content/" + expected.name + ".wav");
		CHECK(actual.name.empty());
		CHECK(actual.encoding == WaveEncoding::pcm16);
		CHECK(actual.channels == expected.channels);
		CHECK(actual.sample_rate == expected.sample_rate);
		CHECK(actual.samples == expected.samples);
		CHECK(actual.frames() == expected.frames());
		CHECK(actual.loop_start == 0);
		CHECK(actual.loop_length == 0);
	}
}

TEST_CASE("PCM16 preserves signed endpoints and stereo frame interleaving at both rate limits")
{
	const std::vector<int16_t> expected = { -32768, 32767, -1, 0, 1, -12345 };
	for (const uint16_t channels : { uint16_t{ 1 }, uint16_t{ 2 } })
	{
		for (const uint32_t rate : { uint32_t{ 8000 }, uint32_t{ 96000 } })
		{
			CAPTURE(channels);
			CAPTURE(rate);
			const WaveData actual = read_wave(wav({ { "fmt ", format(channels, rate) },
				{ "data", pcm16(expected) } }));
			CHECK(actual.channels == channels);
			CHECK(actual.sample_rate == static_cast<int>(rate));
			CHECK(actual.encoding == WaveEncoding::pcm16);
			CHECK(actual.samples == expected);
			CHECK(actual.frames() == expected.size() / channels);
		}
	}
}

TEST_CASE("unsigned PCM8 expands exactly and excludes its odd data padding")
{
	const WaveData mono = read_wave(wav({ { "fmt ", format(1, 22050, 8) },
		{ "data", { 0, 128, 255 } } }));
	const std::vector<int16_t> expected_mono = { -32768, 0, 32512 };
	CHECK(mono.encoding == WaveEncoding::pcm8);
	CHECK(mono.samples == expected_mono);
	CHECK(mono.frames() == 3);
	const WaveData stereo = read_wave(wav({ { "fmt ", format(2, 48000, 8) },
		{ "data", { 0, 255, 128, 127 } } }));
	const std::vector<int16_t> expected_stereo = { -32768, 32512, 0, -256 };
	CHECK(stereo.samples == expected_stereo);
	CHECK(stereo.frames() == 2);
}

TEST_CASE("one-frame mono and stereo PCM WAVs can carry a whole-frame loop")
{
	for (const uint16_t channels : { uint16_t{ 1 }, uint16_t{ 2 } })
	{
		std::vector<int16_t> expected = { -32768, 32767 };
		expected.resize(channels);
		const WaveData actual = read_wave(wav({ { "fmt ", format(channels) },
			{ "data", pcm16(expected) }, { "smpl", sampler(0, 0) } }));
		CHECK(actual.frames() == 1);
		CHECK(actual.samples == expected);
		CHECK(actual.loop_start == 0);
		CHECK(actual.loop_length == 1);
	}
}

TEST_CASE("PCM fmt extensions and unknown odd chunks do not become audio samples")
{
	Bytes extended = format();
	append_u16(extended, 0);
	const std::vector<int16_t> expected = { -12, 56 };
	const WaveData actual = read_wave(wav({ { "JUNK", { 1, 2, 3 } },
		{ "fmt ", extended }, { "LIST", { 4 } }, { "data", pcm16(expected) },
		{ "note", { 7, 8, 9 } } }));
	CHECK(actual.samples == expected);
	CHECK(actual.frames() == 2);
	CHECK(actual.name.empty());
}

TEST_CASE("RIFF chunk ordering does not change PCM or loop interpretation")
{
	const std::vector<int16_t> expected = { -17, 24, 53, -90 };
	const WaveData actual = read_wave(wav({ { "data", pcm16(expected) },
		{ "smpl", sampler(1, 3) }, { "fmt ", format() } }));
	CHECK(actual.samples == expected);
	CHECK(actual.loop_start == 1);
	CHECK(actual.loop_length == 3);
}

TEST_CASE("WAV refuses every truncated prefix even if the RIFF size is rewritten to match")
{
	const Bytes complete = wav({ { "fmt ", format(2) },
		{ "data", pcm16({ -32768, 32767, -1, 0 }) } });
	for (std::size_t size = 0; size < complete.size(); ++size)
	{
		CAPTURE(size);
		Bytes shortened(complete.begin(), complete.begin() + static_cast<std::ptrdiff_t>(size));
		require_refused(shortened);
		if (size >= 12)
		{
			put_u32_le(shortened, 4, static_cast<uint32_t>(size - 8));
			require_refused(shortened);
		}
	}
}

TEST_CASE("WAV refuses inconsistent RIFF sizes, chunk overruns and a missing alignment byte")
{
	const Bytes valid = wav({ { "fmt ", format() }, { "data", pcm16({ 1, 2 }) } });
	for (const uint32_t declared : { uint32_t{ 0 }, uint32_t{ 3 },
		static_cast<uint32_t>(valid.size() - 9), static_cast<uint32_t>(valid.size() - 7),
		std::numeric_limits<uint32_t>::max() })
	{
		Bytes changed = valid;
		put_u32_le(changed, 4, declared);
		require_refused(changed);
	}
	Bytes oversized = valid;
	put_u32_le(oversized, 16, std::numeric_limits<uint32_t>::max());
	require_refused(oversized);
	Bytes partial_header = valid;
	partial_header.insert(partial_header.end(), { 'J', 'U', 'N', 'K', 0, 0, 0 });
	put_u32_le(partial_header, 4, static_cast<uint32_t>(partial_header.size() - 8));
	require_refused(partial_header);
	Bytes no_padding = wav({ { "fmt ", format(1, 8000, 8) }, { "data", { 1, 2, 3 } } });
	no_padding.pop_back();
	put_u32_le(no_padding, 4, static_cast<uint32_t>(no_padding.size() - 8));
	require_refused(no_padding);
}

TEST_CASE("WAV requires its identifiers and one fmt and data chunk without duplicate known chunks")
{
	const Bytes description = format();
	const Bytes samples = pcm16({ 1, 2, 3, 4 });
	require_refused(wav({ { "data", samples } }));
	require_refused(wav({ { "fmt ", description } }));
	require_refused(wav({ { "fmt ", description }, { "fmt ", description }, { "data", samples } }));
	require_refused(wav({ { "fmt ", description }, { "data", samples }, { "data", samples } }));
	require_refused(wav({ { "fmt ", description }, { "data", samples },
		{ "smpl", sampler(1, 2) }, { "smpl", sampler(1, 2) } }));
	Bytes big_endian = wav({ { "fmt ", description }, { "data", samples } });
	big_endian[3] = 'X';
	require_refused(big_endian);
	Bytes other_form = wav({ { "fmt ", description }, { "data", samples } });
	other_form[8] = 'A';
	require_refused(other_form);
}

TEST_CASE("WAV refuses unsupported codecs, channels, rates and sample widths")
{
	const Bytes samples = pcm16({ 1, 2, 3, 4 });
	for (const uint16_t encoding : { uint16_t{ 2 }, uint16_t{ 3 }, uint16_t{ 0xFFFE } })
	{
		Bytes changed = format();
		put_u16(changed, 0, encoding);
		require_refused(wav({ { "fmt ", changed }, { "data", samples } }));
	}
	for (const uint16_t channels : { uint16_t{ 0 }, uint16_t{ 3 } })
	{
		require_refused(wav({ { "fmt ", format(channels) }, { "data", samples } }));
	}
	for (const uint32_t rate : { uint32_t{ 0 }, uint32_t{ 7999 }, uint32_t{ 96001 } })
	{
		require_refused(wav({ { "fmt ", format(1, rate) }, { "data", samples } }));
	}
	for (const uint16_t bits : { uint16_t{ 0 }, uint16_t{ 24 }, uint16_t{ 32 } })
	{
		require_refused(wav({ { "fmt ", format(1, 8000, bits) }, { "data", samples } }));
	}
}

TEST_CASE("WAV refuses inconsistent fmt extension, byte rate, block alignment and partial frames")
{
	const Bytes samples = pcm16({ 1, 2, 3, 4 });
	Bytes short_format = format();
	short_format.pop_back();
	require_refused(wav({ { "fmt ", short_format }, { "data", samples } }));
	for (const std::size_t extension_size : { std::size_t{ 1 }, std::size_t{ 3 } })
	{
		Bytes changed = format();
		changed.resize(16 + extension_size, 0);
		require_refused(wav({ { "fmt ", changed }, { "data", samples } }));
	}
	Bytes nonempty_extension = format();
	append_u16(nonempty_extension, 1);
	require_refused(wav({ { "fmt ", nonempty_extension }, { "data", samples } }));
	Bytes wrong_rate = format();
	put_u32_le(wrong_rate, 8, 15999);
	require_refused(wav({ { "fmt ", wrong_rate }, { "data", samples } }));
	Bytes wrong_alignment = format();
	put_u16(wrong_alignment, 12, 3);
	require_refused(wav({ { "fmt ", wrong_alignment }, { "data", samples } }));
	require_refused(wav({ { "fmt ", format() }, { "data", {} } }));
	require_refused(wav({ { "fmt ", format() }, { "data", { 1, 2, 3 } } }));
	require_refused(wav({ { "fmt ", format(2) }, { "data", { 1, 2, 3, 4, 5, 6 } } }));
}

TEST_CASE("a forward smpl loop uses inclusive frame endpoints for mono and stereo")
{
	for (const uint16_t channels : { uint16_t{ 1 }, uint16_t{ 2 } })
	{
		const std::vector<int16_t> samples(4 * channels, 17);
		const WaveData actual = read_wave(wav({ { "fmt ", format(channels) },
			{ "data", pcm16(samples) }, { "smpl", sampler(1, 3) } }));
		CHECK(actual.frames() == 4);
		CHECK(actual.loop_start == 1);
		CHECK(actual.loop_length == 3);
		CHECK(actual.samples == samples);
	}
	const WaveData one_frame = read_wave(wav({ { "fmt ", format() },
		{ "data", pcm16({ 1, 2, 3, 4 }) }, { "smpl", sampler(3, 3) } }));
	CHECK(one_frame.loop_start == 3);
	CHECK(one_frame.loop_length == 1);
	const WaveData no_loop = read_wave(wav({ { "fmt ", format() },
		{ "data", pcm16({ 1, 2, 3, 4 }) }, { "smpl", sampler(0, 0, 0, 0, 0, 0) } }));
	CHECK(no_loop.loop_start == 0);
	CHECK(no_loop.loop_length == 0);
}

TEST_CASE("smpl refuses truncated, multiple, unsupported and out-of-wave loops")
{
	const Bytes description = format();
	const Bytes samples = pcm16({ 1, 2, 3, 4 });
	for (const std::size_t size : { std::size_t{ 0 }, std::size_t{ 35 }, std::size_t{ 59 } })
	{
		Bytes shortened = sampler(1, 2);
		shortened.resize(size);
		require_refused(wav({ { "fmt ", description }, { "data", samples }, { "smpl", shortened } }));
	}
	for (const uint32_t type : { uint32_t{ 1 }, uint32_t{ 2 }, std::numeric_limits<uint32_t>::max() })
	{
		require_refused(wav({ { "fmt ", description }, { "data", samples }, { "smpl", sampler(1, 2, type) } }));
	}
	require_refused(wav({ { "fmt ", description }, { "data", samples }, { "smpl", sampler(1, 2, 0, 1) } }));
	require_refused(wav({ { "fmt ", description }, { "data", samples }, { "smpl", sampler(1, 2, 0, 0, 1) } }));
	require_refused(wav({ { "fmt ", description }, { "data", samples }, { "smpl", sampler(1, 2, 0, 0, 0, 2) } }));
	struct Endpoints
	{
		uint32_t start;
		uint32_t end;
	};
	const Endpoints rejected[] = {
		{ 2, 1 }, { 4, 4 }, { 0, 4 }, { 0, std::numeric_limits<uint32_t>::max() } };
	for (const Endpoints& endpoints : rejected)
	{
		require_refused(wav({ { "fmt ", description }, { "data", samples },
			{ "smpl", sampler(endpoints.start, endpoints.end) } }));
	}
}

TEST_CASE("smpl count and sampler-data sizes cannot wrap or consume another chunk")
{
	const Bytes description = format();
	const Bytes samples = pcm16({ 1, 2, 3, 4 });
	Bytes huge_count = sampler(1, 2);
	put_u32_le(huge_count, 28, std::numeric_limits<uint32_t>::max());
	require_refused(wav({ { "fmt ", description }, { "data", samples }, { "smpl", huge_count } }));
	for (const uint32_t private_size : { uint32_t{ 1 }, std::numeric_limits<uint32_t>::max() })
	{
		Bytes changed = sampler(1, 2);
		put_u32_le(changed, 32, private_size);
		require_refused(wav({ { "fmt ", description }, { "data", samples }, { "smpl", changed } }));
	}
	Bytes with_private_data = sampler(1, 2);
	put_u32_le(with_private_data, 32, 3);
	with_private_data.insert(with_private_data.end(), { 6, 7, 8 });
	const WaveData actual = read_wave(wav({ { "fmt ", description },
		{ "smpl", with_private_data }, { "data", samples } }));
	CHECK(actual.samples == std::vector<int16_t>{ 1, 2, 3, 4 });
	CHECK(actual.loop_start == 1);
	CHECK(actual.loop_length == 2);
}

TEST_CASE("a missing WAV retains the shared reader's named missing-file exception")
{
	const std::string path = "./content/no_such_pcm_source_for_wav_tests.wav";
	try
	{
		read_wav_file(path);
		FAIL("the missing WAV was read");
	}
	catch (const std::out_of_range& error)
	{
		CHECK(std::string(error.what()).find(path) != std::string::npos);
	}
}
