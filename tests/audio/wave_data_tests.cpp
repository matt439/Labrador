#include <doctest/doctest.h>

#include "engine/audio/wave_data.h"
#include "engine/audio/wave_data_checks.h"

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
	using labrador::detail::validate_wave_bank_data;

	WaveBankData sample_bank()
	{
		WaveData wave;
		wave.name = "tone";
		wave.channels = 1;
		wave.sample_rate = 22050;
		wave.samples = { -32768, -123, 0, 32767, 125, -456, 789, 0 };
		WaveBankData bank;
		bank.name = "memory_bank";
		bank.waves.push_back(wave);
		return bank;
	}

	void check_unchanged(const WaveBankData& actual, const WaveBankData& expected)
	{
		CHECK(actual.name == expected.name);
		REQUIRE(actual.waves.size() == expected.waves.size());
		for (std::size_t index = 0; index < actual.waves.size(); ++index)
		{
			const WaveData& wave = actual.waves[index];
			const WaveData& before = expected.waves[index];
			CHECK(wave.name == before.name);
			CHECK(wave.encoding == before.encoding);
			CHECK(wave.channels == before.channels);
			CHECK(wave.sample_rate == before.sample_rate);
			CHECK(wave.loop_start == before.loop_start);
			CHECK(wave.loop_length == before.loop_length);
			CHECK(wave.samples == before.samples);
		}
	}

	std::string refusal(const WaveBankData& bank)
	{
		const WaveBankData before = bank;
		std::string message;
		bool refused = false;
		try
		{
			validate_wave_bank_data(bank);
		}
		catch (const std::invalid_argument& error)
		{
			refused = true;
			message = error.what();
		}
		REQUIRE(refused);
		check_unchanged(bank, before);
		return message;
	}

	void require_named_refusal(const WaveBankData& bank, const std::string& wave_name)
	{
		const std::string message = refusal(bank);
		CHECK(message.find("memory_bank") != std::string::npos);
		if (!wave_name.empty())
		{
			CHECK(message.find(wave_name) != std::string::npos);
		}
	}
}

TEST_CASE("memory wave data accepts the portable PCM profile and preserves provenance and samples")
{
	for (const int channels : { 1, 2 })
	{
		for (const int rate : { 8000, 22050, 44100, 48000, 96000 })
		{
			for (const WaveEncoding encoding : { WaveEncoding::pcm8,
				WaveEncoding::pcm16, WaveEncoding::adpcm })
			{
				WaveBankData bank = sample_bank();
				WaveData& wave = bank.waves[0];
				wave.channels = channels;
				wave.sample_rate = rate;
				wave.encoding = encoding;
				wave.loop_start = 1;
				wave.loop_length = 2;
				const WaveBankData before = bank;
				CAPTURE(channels);
				CAPTURE(rate);
				CAPTURE(static_cast<int>(encoding));
				CHECK_NOTHROW(validate_wave_bank_data(bank));
				check_unchanged(bank, before);
			}
		}
	}
}

TEST_CASE("memory wave banks preserve distinct names and mixed wave formats without normalizing them")
{
	WaveBankData bank = sample_bank();
	bank.name = "banque_café";
	bank.waves[0].name = "Ping";
	WaveData second = bank.waves[0];
	second.name = "ping";
	second.channels = 2;
	second.sample_rate = 96000;
	second.samples = { 1, -1, 32767, -32768 };
	bank.waves.push_back(second);
	const WaveBankData before = bank;
	CHECK_NOTHROW(validate_wave_bank_data(bank));
	check_unchanged(bank, before);
}

TEST_CASE("memory wave data accepts whole-wave and exact endpoint loop regions in frames")
{
	for (const int channels : { 1, 2 })
	{
		WaveBankData bank = sample_bank();
		WaveData& wave = bank.waves[0];
		wave.channels = channels;
		const uint32_t frames = static_cast<uint32_t>(wave.frames());
		wave.loop_start = 0;
		wave.loop_length = frames;
		CHECK_NOTHROW(validate_wave_bank_data(bank));
		wave.loop_start = frames - 1;
		wave.loop_length = 1;
		CHECK_NOTHROW(validate_wave_bank_data(bank));
		// No region means the whole wave, so the unused start is not clamped.
		wave.loop_start = std::numeric_limits<uint32_t>::max();
		wave.loop_length = 0;
		const WaveBankData before = bank;
		CHECK_NOTHROW(validate_wave_bank_data(bank));
		check_unchanged(bank, before);
		wave.samples.resize(static_cast<std::size_t>(channels));
		wave.loop_start = 0;
		wave.loop_length = 1;
		CHECK(wave.frames() == 1);
		CHECK_NOTHROW(validate_wave_bank_data(bank));
	}
}

TEST_CASE("memory wave banks refuse missing names embedded NULs duplicates and empty banks")
{
	SUBCASE("an unnamed bank")
	{
		WaveBankData bank = sample_bank();
		bank.name.clear();
		CHECK_FALSE(refusal(bank).empty());
	}
	SUBCASE("an embedded NUL in the bank name")
	{
		WaveBankData bank = sample_bank();
		bank.name = std::string("bad\0bank", 8);
		CHECK_FALSE(refusal(bank).empty());
	}
	SUBCASE("an unnamed wave")
	{
		WaveBankData bank = sample_bank();
		bank.waves[0].name.clear();
		require_named_refusal(bank, "");
	}
	SUBCASE("an embedded NUL in a wave name")
	{
		WaveBankData bank = sample_bank();
		bank.waves[0].name = std::string("bad\0wave", 8);
		require_named_refusal(bank, "");
	}
	SUBCASE("a duplicate wave name")
	{
		WaveBankData bank = sample_bank();
		const WaveData duplicate = bank.waves[0];
		bank.waves.push_back(duplicate);
		require_named_refusal(bank, "tone");
	}
	SUBCASE("a bank with no waves")
	{
		WaveBankData bank = sample_bank();
		bank.waves.clear();
		require_named_refusal(bank, "");
	}
}

TEST_CASE("memory wave data refuses unsupported channels and rates by name")
{
	for (const int channels : { -1, 0, 3, 8, std::numeric_limits<int>::max() })
	{
		WaveBankData bank = sample_bank();
		bank.waves[0].channels = channels;
		CAPTURE(channels);
		require_named_refusal(bank, "tone");
	}
	for (const int rate : { -1, 0, 7999, 96001, std::numeric_limits<int>::max() })
	{
		WaveBankData bank = sample_bank();
		bank.waves[0].sample_rate = rate;
		CAPTURE(rate);
		require_named_refusal(bank, "tone");
	}
}

TEST_CASE("memory wave data refuses unknown provenance empty samples and partial frames")
{
	SUBCASE("an unrecognized provenance encoding")
	{
		WaveBankData bank = sample_bank();
		bank.waves[0].encoding = static_cast<WaveEncoding>(73);
		require_named_refusal(bank, "tone");
	}
	SUBCASE("no samples")
	{
		WaveBankData bank = sample_bank();
		bank.waves[0].samples.clear();
		require_named_refusal(bank, "tone");
	}
	SUBCASE("a partial stereo frame")
	{
		WaveBankData bank = sample_bank();
		bank.waves[0].channels = 2;
		bank.waves[0].samples.pop_back();
		require_named_refusal(bank, "tone");
	}
}

TEST_CASE("memory wave data checks loop bounds without wrapping unsigned addition")
{
	struct Region
	{
		uint32_t start;
		uint32_t length;
	};
	for (const int channels : { 1, 2 })
	{
		WaveBankData bank = sample_bank();
		bank.waves[0].channels = channels;
		const uint32_t frames = static_cast<uint32_t>(bank.waves[0].frames());
		const uint32_t maximum = std::numeric_limits<uint32_t>::max();
		for (const Region region : { Region{ frames, 1 }, Region{ frames - 1, 2 },
			Region{ maximum, 1 }, Region{ 1, maximum }, Region{ maximum, maximum } })
		{
			bank.waves[0].loop_start = region.start;
			bank.waves[0].loop_length = region.length;
			CAPTURE(channels);
			CAPTURE(region.start);
			CAPTURE(region.length);
			require_named_refusal(bank, "tone");
		}
	}
}

TEST_CASE("portable PCM counts respect the native source buffer limit without allocating its payload")
{
	const size_t limit = labrador::detail::max_pcm_bytes / sizeof(int16_t);
	CHECK(labrador::detail::max_pcm_bytes == 2147483648u);
	CHECK(labrador::detail::portable_pcm_size_fits(limit, limit));
	CHECK_FALSE(labrador::detail::portable_pcm_size_fits(limit + 1, limit + 1));
	CHECK_FALSE(labrador::detail::portable_pcm_size_fits(std::numeric_limits<size_t>::max(), 1));
	CHECK_FALSE(labrador::detail::portable_pcm_size_fits(1,
		static_cast<size_t>(std::numeric_limits<int>::max()) + 1));
}
