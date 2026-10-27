#include <doctest/doctest.h>

#include "engine/audio/xwb_file.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Reading an .xwb, with no audio device.
//
// AGAINST REAL BANKS FOR THE HAPPY PATHS, and against banks built here for the
// arithmetic and the unhappy ones - the shape tests/render/dds_file_tests.cpp
// has. content/tones.xwb is the audio sample's own bank, packed by xwbtool from
// the three WAVs beside it, so reading it back sample for sample against those
// WAVs is a check against a writer this repository did not write.
// content/ping_adpcm.xwb is one of those WAVs through an MS-ADPCM encoder
// (tests/audio/content/make_adpcm_bank.py says which and how), and what it
// decodes to is held to the source and to a fingerprint of the decode
// Windows' own MS-ADPCM codec produces from the same blocks.

namespace
{
	using labrador::WaveBankData;
	using labrador::WaveData;
	using labrador::WaveEncoding;
	using labrador::read_xwb_file;

	class ScratchFile
	{
	public:
		ScratchFile(const std::string& name,
			const std::vector<unsigned char>& bytes)
			: path_(name)
		{
			std::ofstream file(this->path_, std::ios::binary);
			REQUIRE(file.good());
			if (!bytes.empty())
			{
				file.write(reinterpret_cast<const char*>(bytes.data()),
					static_cast<std::streamsize>(bytes.size()));
			}
		}

		~ScratchFile() { std::remove(this->path_.c_str()); }

		ScratchFile(const ScratchFile&) = delete;
		ScratchFile& operator=(const ScratchFile&) = delete;

		const std::string& path() const { return this->path_; }

	private:
		std::string path_;
	};

	std::vector<unsigned char> file_bytes(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		REQUIRE(file.good());
		const std::streamoff size = file.tellg();
		std::vector<unsigned char> bytes(static_cast<size_t>(size));
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char*>(bytes.data()), size);
		return bytes;
	}

	uint32_t u32_at(const std::vector<unsigned char>& bytes, size_t at)
	{
		return static_cast<uint32_t>(bytes[at]) |
			(static_cast<uint32_t>(bytes[at + 1]) << 8) |
			(static_cast<uint32_t>(bytes[at + 2]) << 16) |
			(static_cast<uint32_t>(bytes[at + 3]) << 24);
	}

	void put_u32(std::vector<unsigned char>& bytes, size_t at, uint32_t value)
	{
		for (size_t i = 0; i < 4; i++)
		{
			bytes[at + i] =
				static_cast<unsigned char>((value >> (8 * i)) & 0xFFu);
		}
	}

	// The samples in a 16-bit PCM WAV's data chunk. The source WAVs are
	// written by Python's wave module, so the chunks are the plain ones.
	std::vector<int16_t> wav_samples(const std::string& path)
	{
		const std::vector<unsigned char> bytes = file_bytes(path);
		size_t at = 12;
		while (at + 8 <= bytes.size())
		{
			const uint32_t size = u32_at(bytes, at + 4);
			if (bytes[at] == 'd' && bytes[at + 1] == 'a' &&
				bytes[at + 2] == 't' && bytes[at + 3] == 'a')
			{
				std::vector<int16_t> samples;
				for (size_t i = 0; i + 1 < size; i += 2)
				{
					samples.push_back(static_cast<int16_t>(static_cast<uint16_t>(
						bytes[at + 8 + i] | (bytes[at + 9 + i] << 8))));
				}
				return samples;
			}
			at += 8 + size + (size & 1u);
		}
		FAIL("no data chunk in " << path);
		return {};
	}

	// The mini wave format, field by field.
	uint32_t mini_format(uint32_t tag, uint32_t channels, uint32_t rate,
		uint32_t block_align, bool sixteen_bit)
	{
		return tag | (channels << 2) | (rate << 5) | (block_align << 23) |
			(sixteen_bit ? 0x80000000u : 0u);
	}

	const uint32_t tag_pcm = 0;
	const uint32_t tag_xma = 1;
	const uint32_t tag_adpcm = 2;
	const uint32_t tag_wma = 3;

	struct Entry
	{
		Entry(const std::string& entry_name, uint32_t entry_format,
			const std::vector<unsigned char>& entry_data)
			: name(entry_name), format(entry_format), data(entry_data)
		{

		}

		std::string name;
		uint32_t format = 0;
		std::vector<unsigned char> data;
		uint32_t duration = 0;
		uint32_t loop_start = 0;
		uint32_t loop_length = 0;
	};

	// A bank laid out as `xwbtool -nc -f` lays one out: a 52-byte header,
	// 96 bytes of bank data, 24-byte entries, an empty seek table, 64-byte
	// names, then the wave data.
	std::vector<unsigned char> build_bank(const std::vector<Entry>& entries,
		bool named = true)
	{
		const uint32_t count = static_cast<uint32_t>(entries.size());
		const uint32_t entries_at = 52 + 96;
		const uint32_t names_at = entries_at + 24 * count;
		const uint32_t names_length = named ? 64 * count : 0;
		const uint32_t data_at = names_at + names_length;
		uint32_t data_length = 0;
		for (const Entry& entry : entries)
		{
			data_length += static_cast<uint32_t>(entry.data.size());
		}

		std::vector<unsigned char> bytes(data_at + data_length, 0);
		put_u32(bytes, 0, 0x444E4257);
		put_u32(bytes, 4, 46);
		put_u32(bytes, 8, 44);
		const std::vector<uint32_t> segments = { 52, 96, entries_at,
			24 * count, names_at, 0, names_at, names_length, data_at,
			data_length };
		for (size_t i = 0; i < segments.size(); i++)
		{
			put_u32(bytes, 12 + 4 * i, segments[i]);
		}

		put_u32(bytes, 52, named ? 0x00010000u : 0u);
		put_u32(bytes, 56, count);
		bytes[60] = 'b';
		put_u32(bytes, 124, 24);
		put_u32(bytes, 128, named ? 64u : 0u);
		put_u32(bytes, 132, 4);

		uint32_t offset = 0;
		for (uint32_t i = 0; i < count; i++)
		{
			const Entry& entry = entries[i];
			const size_t at = entries_at + 24 * i;
			put_u32(bytes, at, entry.duration << 4);
			put_u32(bytes, at + 4, entry.format);
			put_u32(bytes, at + 8, offset);
			put_u32(bytes, at + 12, static_cast<uint32_t>(entry.data.size()));
			put_u32(bytes, at + 16, entry.loop_start);
			put_u32(bytes, at + 20, entry.loop_length);
			if (named)
			{
				for (size_t c = 0; c < entry.name.size(); c++)
				{
					bytes[names_at + 64 * i + c] =
						static_cast<unsigned char>(entry.name[c]);
				}
			}
			for (size_t b = 0; b < entry.data.size(); b++)
			{
				bytes[data_at + offset + b] = entry.data[b];
			}
			offset += static_cast<uint32_t>(entry.data.size());
		}
		return bytes;
	}

	WaveBankData read_bank(const std::vector<unsigned char>& bytes)
	{
		const ScratchFile file("scratch.xwb", bytes);
		return read_xwb_file(file.path());
	}

	// What a refusal says, so a case can check that it names the problem.
	std::string refusal(const std::vector<unsigned char>& bytes)
	{
		const ScratchFile file("refused.xwb", bytes);
		try
		{
			read_xwb_file(file.path());
		}
		catch (const std::runtime_error& error)
		{
			return error.what();
		}
		FAIL("the bank was read rather than refused");
		return {};
	}

	bool contains(const std::string& text, const std::string& part)
	{
		return text.find(part) != std::string::npos;
	}

	// The 7-byte mono MS-ADPCM block header, then its nibble bytes.
	std::vector<unsigned char> adpcm_block(unsigned char predictor,
		int16_t delta, int16_t sample_1, int16_t sample_2,
		const std::vector<unsigned char>& nibbles, size_t block_size)
	{
		std::vector<unsigned char> block = { predictor };
		for (const int16_t value : { delta, sample_1, sample_2 })
		{
			const uint16_t bits = static_cast<uint16_t>(value);
			block.push_back(static_cast<unsigned char>(bits & 0xFFu));
			block.push_back(static_cast<unsigned char>(bits >> 8));
		}
		block.insert(block.end(), nibbles.begin(), nibbles.end());
		block.resize(block_size, 0);
		return block;
	}

	// A stored tag_adpcm alignment of zero is a 22-byte mono block: 7 bytes of
	// header and 15 of nibbles, so 32 samples.
	const size_t mono_block = 22;
}

TEST_CASE("the audio sample's bank reads back as the WAVs it was packed from")
{
	const WaveBankData bank = read_xwb_file("./content/tones.xwb");

	CHECK(bank.name == "tones");
	REQUIRE(bank.waves.size() == 3);

	const std::vector<std::string> names = { "ping", "chime", "drone" };
	for (size_t i = 0; i < names.size(); i++)
	{
		CAPTURE(names[i]);
		const WaveData& wave = bank.waves[i];
		CHECK(wave.name == names[i]);
		CHECK(wave.encoding == WaveEncoding::pcm16);
		CHECK(wave.channels == 1);
		CHECK(wave.sample_rate == 22050);
		CHECK(wave.loop_length == 0);

		// Sample for sample, which is the whole claim: the reader found every
		// entry's bytes where xwbtool put them.
		const std::vector<int16_t> source =
			wav_samples("./content/" + names[i] + ".wav");
		CHECK(wave.frames() == source.size());
		CHECK(wave.samples == source);
	}
}

TEST_CASE("an MS-tag_adpcm bank decodes to its source within the codec's error")
{
	const WaveBankData bank = read_xwb_file("./content/ping_adpcm.xwb");
	REQUIRE(bank.waves.size() == 1);
	const WaveData& wave = bank.waves[0];

	CHECK(wave.name == "ping");
	CHECK(wave.encoding == WaveEncoding::adpcm);
	CHECK(wave.channels == 1);
	CHECK(wave.sample_rate == 22050);

	// Eight 500-sample blocks hold 4000, and the entry's duration says the
	// sound is the source's 3528: the padding in the last block is not sound.
	const std::vector<int16_t> source = wav_samples("./content/ping.wav");
	REQUIRE(wave.frames() == source.size());

	// A block starts with two samples stored verbatim.
	CHECK(wave.samples[0] == source[0]);
	CHECK(wave.samples[1] == source[1]);

	// tag_adpcm is lossy, so the comparison with the source is a bound: the
	// largest error is 454 on a peak of 9830 for this encode.
	int largest_error = 0;
	for (size_t i = 0; i < source.size(); i++)
	{
		const int error = wave.samples[i] - source[i];
		largest_error = std::max(largest_error, error < 0 ? -error : error);
	}
	CHECK(largest_error < 1024);

	// And exactly: an FNV-1a hash of the decoded little-endian bytes, which is
	// the hash of what Windows' MS-ADPCM codec (msacm32) decodes from the same
	// blocks, trimmed to the same 3528 samples.
	uint32_t hash = 2166136261u;
	for (const int16_t sample : wave.samples)
	{
		const uint16_t bits = static_cast<uint16_t>(sample);
		for (const uint32_t byte : { bits & 0xFFu, static_cast<uint32_t>(bits >> 8) })
		{
			hash = (hash ^ byte) * 16777619u;
		}
	}
	CHECK(hash == 0x17937C62u);
	CHECK(wave.samples[500] == -2763);
	CHECK(wave.samples[503] == 4648);
}

TEST_CASE("MS-tag_adpcm arithmetic, one block at a time")
{
	SUBCASE("the header's samples come first, older first, then the nibbles")
	{
		// Predictor 0 is (256, 0): predict the last sample. A nibble of 7
		// adds seven steps and adapts the step by 614/256; -1 subtracts one.
		const std::vector<unsigned char> block =
			adpcm_block(0, 16, 100, 50, { 0x7F }, mono_block);
		const WaveBankData bank = read_bank(build_bank({ { "a",
			mini_format(tag_adpcm, 1, 8000, 0, false), block } }));
		const std::vector<int16_t>& samples = bank.waves[0].samples;

		REQUIRE(samples.size() == 32);
		CHECK(samples[0] == 50);
		CHECK(samples[1] == 100);
		CHECK(samples[2] == 100 + 7 * 16);
		CHECK(samples[3] == 212 - 38);         // 16 * 614 / 256 = 38
		CHECK(samples[31] == 174);             // nibbles of zero hold it
	}

	SUBCASE("a negative prediction truncates toward zero, as a division")
	{
		// Predictor 3 is (192, 64): -1 * 192 / 256 is -0.75, which a
		// division makes 0 and an arithmetic shift would make -1.
		const std::vector<unsigned char> block =
			adpcm_block(3, 16, -1, 0, {}, mono_block);
		const WaveBankData bank = read_bank(build_bank({ { "a",
			mini_format(tag_adpcm, 1, 8000, 0, false), block } }));

		CHECK(bank.waves[0].samples[0] == 0);
		CHECK(bank.waves[0].samples[1] == -1);
		CHECK(bank.waves[0].samples[2] == 0);
	}

	SUBCASE("a prediction past full scale clamps")
	{
		const std::vector<unsigned char> block =
			adpcm_block(0, 1000, 32000, 0, { 0x78 }, mono_block);
		const WaveBankData bank = read_bank(build_bank({ { "a",
			mini_format(tag_adpcm, 1, 8000, 0, false), block } }));

		CHECK(bank.waves[0].samples[2] == 32767);
		// -8 steps of 1000 * 614 / 256 = 2398 from the clamped value.
		CHECK(bank.waves[0].samples[3] == 32767 - 8 * 2398);
	}

	SUBCASE("stereo interleaves the header per channel and the nibbles per byte")
	{
		// Each header field is left then right; in a nibble byte the high
		// nibble is the left channel's.
		std::vector<unsigned char> block = { 0, 0 };
		const std::vector<int16_t> header = { 16, 16, 10, -10, 20, -20 };
		for (const int16_t value : header)
		{
			const uint16_t bits = static_cast<uint16_t>(value);
			block.push_back(static_cast<unsigned char>(bits & 0xFFu));
			block.push_back(static_cast<unsigned char>(bits >> 8));
		}
		block.push_back(0x1F);
		block.resize(2 * mono_block, 0);

		const WaveBankData bank = read_bank(build_bank({ { "a",
			mini_format(tag_adpcm, 2, 8000, 0, false), block } }));
		const WaveData& wave = bank.waves[0];

		REQUIRE(wave.frames() == 32);
		const std::vector<int16_t> first = { 20, -20, 10, -10, 26, -26, 26, -26 };
		CHECK(std::vector<int16_t>(wave.samples.begin(),
			wave.samples.begin() + 8) == first);
	}

	SUBCASE("the duration trims the last block's padding")
	{
		Entry entry = { "a", mini_format(tag_adpcm, 1, 8000, 0, false),
			adpcm_block(0, 16, 0, 0, {}, mono_block) };
		entry.duration = 5;
		CHECK(read_bank(build_bank({ entry })).waves[0].frames() == 5);

		// A duration the blocks cannot hold says nothing usable.
		entry.duration = 33;
		CHECK(read_bank(build_bank({ entry })).waves[0].frames() == 32);
	}
}

TEST_CASE("eight-bit tag_pcm widens to sixteen exactly")
{
	const WaveBankData bank = read_bank(build_bank({ { "a",
		mini_format(tag_pcm, 2, 11025, 2, false), { 0, 128, 255, 64 } } }));
	const WaveData& wave = bank.waves[0];

	CHECK(wave.encoding == WaveEncoding::pcm8);
	CHECK(wave.channels == 2);
	CHECK(wave.sample_rate == 11025);
	CHECK(wave.frames() == 2);
	CHECK(wave.samples == std::vector<int16_t>{ -32768, 0, 32512, -16384 });
}

TEST_CASE("entries are found by offset, carry their loop, and may be unnamed")
{
	Entry first = { "first", mini_format(tag_pcm, 1, 8000, 2, true),
		{ 1, 0, 2, 0 } };
	Entry second = { "second", mini_format(tag_pcm, 1, 8000, 2, true),
		{ 3, 0, 4, 0, 5, 0 } };
	second.loop_start = 1;
	second.loop_length = 2;

	const WaveBankData named = read_bank(build_bank({ first, second }));
	REQUIRE(named.waves.size() == 2);
	CHECK(named.name == "b");
	CHECK(named.waves[0].samples == std::vector<int16_t>{ 1, 2 });
	CHECK(named.waves[1].name == "second");
	CHECK(named.waves[1].samples == std::vector<int16_t>{ 3, 4, 5 });
	CHECK(named.waves[1].loop_start == 1);
	CHECK(named.waves[1].loop_length == 2);

	// A streaming bank lays its entries out the same way; only how a player
	// would fetch them differs, and this reader holds the whole file.
	std::vector<unsigned char> streaming = build_bank({ first, second });
	put_u32(streaming, 52, 0x00010001);
	CHECK(read_bank(streaming).waves[1].samples ==
		std::vector<int16_t>{ 3, 4, 5 });

	// No name table is a bank that can be read and not resolved against.
	const WaveBankData unnamed =
		read_bank(build_bank({ first, second }, false));
	CHECK(unnamed.waves[0].name.empty());
	CHECK(unnamed.waves[1].samples == std::vector<int16_t>{ 3, 4, 5 });
}

TEST_CASE("a bank this engine cannot play says which way")
{
	const Entry pcm = { "tone", mini_format(tag_pcm, 1, 8000, 2, true),
		{ 1, 0, 2, 0 } };

	SUBCASE("nothing there")
	{
		CHECK_THROWS_AS(read_xwb_file("./content/no_such.xwb"),
			std::out_of_range);
	}

	SUBCASE("there, and not a wave bank")
	{
		CHECK(contains(refusal(std::vector<unsigned char>(64, 0x41)),
			"WBND"));
	}

	SUBCASE("a big-endian bank")
	{
		std::vector<unsigned char> bytes = build_bank({ pcm });
		put_u32(bytes, 0, 0x57424E44);
		CHECK(contains(refusal(bytes), "big-endian"));
	}

	SUBCASE("another version")
	{
		std::vector<unsigned char> bytes = build_bank({ pcm });
		put_u32(bytes, 8, 43);
		CHECK(contains(refusal(bytes), "version 43"));
	}

	SUBCASE("truncated inside the header")
	{
		std::vector<unsigned char> bytes = build_bank({ pcm });
		bytes.resize(8);
		CHECK(contains(refusal(bytes), "ends after 8 bytes"));
	}

	SUBCASE("truncated inside the wave data")
	{
		std::vector<unsigned char> bytes = build_bank({ pcm });
		bytes.pop_back();
		CHECK(contains(refusal(bytes), "wave data"));
		CHECK(contains(refusal(bytes), "truncated"));
	}

	SUBCASE("more entries than the metadata holds")
	{
		std::vector<unsigned char> bytes = build_bank({ pcm });
		put_u32(bytes, 56, 1000000);
		CHECK(contains(refusal(bytes), "1000000 entries"));
	}

	SUBCASE("compact entries")
	{
		std::vector<unsigned char> bytes = build_bank({ pcm });
		put_u32(bytes, 52, 0x00030000);
		CHECK(contains(refusal(bytes), "compact"));
	}

	SUBCASE("xWMA")
	{
		const Entry wma = { "music", mini_format(tag_wma, 2, 44100, 0, false),
			{ 0, 0, 0, 0 } };
		const std::string message = refusal(build_bank({ pcm, wma }));
		CHECK(contains(message, "xWMA"));
		CHECK(contains(message, "entry 1 (\"music\")"));
	}

	SUBCASE("tag_xma")
	{
		const Entry xma = { "music", mini_format(tag_xma, 2, 44100, 0, false),
			{ 0, 0, 0, 0 } };
		CHECK(contains(refusal(build_bank({ xma })), "tag_xma"));
	}

	SUBCASE("a play region past the wave data")
	{
		std::vector<unsigned char> bytes = build_bank({ pcm });
		put_u32(bytes, 52 + 96 + 8, 2);              // play offset
		const std::string message = refusal(bytes);
		CHECK(contains(message, "entry 0 (\"tone\")"));
		CHECK(contains(message, "bytes 2 to 6"));
	}

	SUBCASE("a loop past the end of the wave")
	{
		Entry looped = pcm;
		looped.loop_start = 1;
		looped.loop_length = 2;
		CHECK(contains(refusal(build_bank({ looped })), "loops frames 1 to 3"));
	}

	SUBCASE("tag_pcm whose block alignment disagrees with its format")
	{
		const Entry wrong = { "tone", mini_format(tag_pcm, 2, 8000, 2, true),
			{ 1, 0, 2, 0 } };
		CHECK(contains(refusal(build_bank({ wrong })), "block alignment"));
	}

	SUBCASE("MS-tag_adpcm that stops part way through a block")
	{
		std::vector<unsigned char> block =
			adpcm_block(0, 16, 0, 0, {}, mono_block);
		block.pop_back();
		const Entry part = { "tone", mini_format(tag_adpcm, 1, 8000, 0, false),
			block };
		CHECK(contains(refusal(build_bank({ part })), "whole number"));
	}

	SUBCASE("MS-tag_adpcm naming a predictor that does not exist")
	{
		const Entry bad = { "tone", mini_format(tag_adpcm, 1, 8000, 0, false),
			adpcm_block(7, 16, 0, 0, {}, mono_block) };
		CHECK(contains(refusal(build_bank({ bad })), "predictor 7"));
	}

	SUBCASE("MS-tag_adpcm in more than two channels")
	{
		const Entry wide = { "tone", mini_format(tag_adpcm, 3, 8000, 0, false),
			std::vector<unsigned char>(3 * mono_block, 0) };
		CHECK(contains(refusal(build_bank({ wide })), "3 channels"));
	}
}
