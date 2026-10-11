#include "engine/audio/xwb_file.h"

#include "engine/core/byte_reader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// The layout below is XACT 3's, as DirectXTK's WaveBankReader.cpp reads it
// and xwbtool writes it. Every number in this file is a field of that format
// rather than a choice made here, except where a comment says otherwise.

namespace labrador
{
	namespace
	{
		const uint32_t SIGNATURE = 0x444E4257;             // "WBND"
		const uint32_t BIG_ENDIAN_SIGNATURE = 0x57424E44;  // "DNBW"
		const uint32_t HEADER_VERSION = 44;

		// The five regions the header points at, in the order it lists them.
		const size_t SEGMENT_COUNT = 5;
		const size_t BANK_DATA = 0;
		const size_t ENTRY_METADATA = 1;
		const size_t ENTRY_NAMES = 3;
		const size_t ENTRY_WAVE_DATA = 4;
		const std::array<const char*, SEGMENT_COUNT> SEGMENT_NAMES = {
			"bank data", "entry metadata", "seek tables", "entry names",
			"wave data" };

		// The bank data record: flags, count, a 64-byte name, three sizes, the
		// compact format and an 8-byte build time.
		const uint32_t BANK_DATA_SIZE = 96;
		const size_t BANK_NAME_LENGTH = 64;
		const uint32_t FLAG_ENTRY_NAMES = 0x00010000;
		const uint32_t FLAG_COMPACT = 0x00020000;

		// A standard entry: flags and duration, format, play region, loop
		// region - six DWORDs.
		const uint32_t ENTRY_SIZE = 24;

		// The mini wave format's codec tags.
		const uint32_t TAG_PCM = 0;
		const uint32_t TAG_XMA = 1;
		const uint32_t TAG_ADPCM = 2;
		const uint32_t TAG_WMA = 3;

		// A bank stores an ADPCM block alignment per channel and less this
		// much, so that a useful one fits the format's eight bits.
		const int ADPCM_BLOCK_ALIGN_OFFSET = 22;

		// MS-ADPCM's seven predictor pairs and its step adaptation table.
		// An .xwb does not carry the coefficients, because every encoder that
		// writes one uses these and DirectXTK fills them in when it builds the
		// WAVEFORMATEX; a file that wanted others could not say so.
		const std::array<int, 7> COEFFICIENT_1 = {
			256, 512, 0, 192, 240, 460, 392 };
		const std::array<int, 7> COEFFICIENT_2 = {
			0, -256, 0, 64, 0, -208, -232 };
		const std::array<int, 16> ADAPTATION = {
			230, 230, 230, 230, 307, 409, 512, 614,
			768, 614, 512, 409, 307, 230, 230, 230 };

		// The step's floor is the format's. Its ceiling is this reader's: a
		// corrupt block can grow the step by three times a nibble, and an
		// unbounded one overflows. Real content never comes near it, so the
		// bound changes no file an encoder wrote.
		const int64_t MINIMUM_DELTA = 16;
		const int64_t MAXIMUM_DELTA = 0x7FFFFFFF / 768;

		struct Segment
		{
			uint32_t offset = 0;
			uint32_t length = 0;
		};

		std::string entry_label(const std::string& path, uint32_t index,
			const std::string& name)
		{
			std::string label = path + ": entry " + std::to_string(index);
			if (!name.empty())
			{
				label += " (\"" + name + "\")";
			}
			return label;
		}

		// A fixed-width, NUL-padded name field. Bytes outside printable ASCII
		// are kept as they are; a name is compared, never displayed raw.
		std::string fixed_string(const std::vector<unsigned char>& bytes,
			size_t offset, size_t length)
		{
			std::string text;
			for (size_t i = 0; i < length; i++)
			{
				const unsigned char character = bytes[offset + i];
				if (character == 0)
				{
					break;
				}
				text += static_cast<char>(character);
			}
			return text;
		}

		uint32_t u32_at(const std::vector<unsigned char>& bytes, size_t at)
		{
			return static_cast<uint32_t>(bytes[at]) |
				(static_cast<uint32_t>(bytes[at + 1]) << 8) |
				(static_cast<uint32_t>(bytes[at + 2]) << 16) |
				(static_cast<uint32_t>(bytes[at + 3]) << 24);
		}

		int16_t i16_at(const unsigned char* at)
		{
			return static_cast<int16_t>(static_cast<uint16_t>(
				static_cast<uint32_t>(at[0]) |
				(static_cast<uint32_t>(at[1]) << 8)));
		}

		int16_t clamp_sample(int64_t value)
		{
			if (value > 32767)
			{
				return 32767;
			}
			if (value < -32768)
			{
				return -32768;
			}
			return static_cast<int16_t>(value);
		}

		// One MS-ADPCM block, appended to `out` as interleaved samples.
		//
		// The header is a predictor index, a step, and the two most recent
		// samples, each as one value per channel; the two samples are emitted
		// first, older one first. Then a nibble per sample, high nibble first,
		// channels interleaved. The prediction divides by 256 with C++'s
		// truncation toward zero, and the step and the clamp are as below,
		// because that is what Windows' own MS-ADPCM codec and FFmpeg's both
		// produce, sample for sample; AudioTests pins a hash of the first's
		// output. A shift would round negative predictions the other way.
		void decode_adpcm_block(const unsigned char* block, size_t block_align,
			int channels, const std::string& label, size_t block_index,
			std::vector<int16_t>& out)
		{
			const size_t count = static_cast<size_t>(channels);
			std::array<int, 2> predictor = { 0, 0 };
			std::array<int64_t, 2> delta = { 0, 0 };
			std::array<int64_t, 2> sample_1 = { 0, 0 };
			std::array<int64_t, 2> sample_2 = { 0, 0 };

			const unsigned char* at = block;
			for (size_t c = 0; c < count; c++)
			{
				predictor[c] = at[c];
				if (predictor[c] >= static_cast<int>(COEFFICIENT_1.size()))
				{
					throw std::runtime_error(label + " is MS-ADPCM whose block " +
						std::to_string(block_index) + " names predictor " +
						std::to_string(predictor[c]) + "; there are seven, "
						"numbered from zero.");
				}
			}
			at += count;
			for (size_t c = 0; c < count; c++, at += 2)
			{
				delta[c] = i16_at(at);
			}
			for (size_t c = 0; c < count; c++, at += 2)
			{
				sample_1[c] = i16_at(at);
			}
			for (size_t c = 0; c < count; c++, at += 2)
			{
				sample_2[c] = i16_at(at);
			}
			for (size_t c = 0; c < count; c++)
			{
				out.push_back(static_cast<int16_t>(sample_2[c]));
			}
			for (size_t c = 0; c < count; c++)
			{
				out.push_back(static_cast<int16_t>(sample_1[c]));
			}

			const size_t nibbles = (block_align - 7 * count) * 2;
			for (size_t i = 0; i < nibbles; i++)
			{
				const unsigned char byte = at[i / 2];
				const int nibble = (i % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
				const int signed_nibble = nibble >= 8 ? nibble - 16 : nibble;
				const size_t c = i % count;
				const size_t p = static_cast<size_t>(predictor[c]);

				const int64_t prediction =
					(sample_1[c] * COEFFICIENT_1[p] +
						sample_2[c] * COEFFICIENT_2[p]) / 256;
				const int16_t value =
					clamp_sample(prediction + signed_nibble * delta[c]);

				int64_t next = delta[c] * ADAPTATION[static_cast<size_t>(nibble)] / 256;
				next = next < MINIMUM_DELTA ? MINIMUM_DELTA : next;
				delta[c] = next > MAXIMUM_DELTA ? MAXIMUM_DELTA : next;
				sample_2[c] = sample_1[c];
				sample_1[c] = value;
				out.push_back(value);
			}
		}

		// The entry's bytes into `wave.samples`, by codec.
		void decode_entry(const std::vector<unsigned char>& bytes,
			size_t data_at, uint32_t data_length, uint32_t format,
			uint32_t duration, const std::string& label, WaveData& wave)
		{
			const uint32_t tag = format & 0x3u;
			const int channels = static_cast<int>((format >> 2) & 0x7u);
			const int sample_rate = static_cast<int>((format >> 5) & 0x3FFFFu);
			const int block_align = static_cast<int>((format >> 23) & 0xFFu);
			const bool sixteen_bit = ((format >> 31) & 0x1u) != 0;

			if (tag == TAG_WMA)
			{
				throw std::runtime_error(label + " is xWMA, which this engine "
					"does not decode. Re-pack it from PCM or ADPCM sources.");
			}
			if (tag == TAG_XMA)
			{
				throw std::runtime_error(label + " is XMA, the Xbox 360 codec, "
					"which this engine does not decode. Re-pack it from PCM or "
					"ADPCM sources.");
			}
			if (channels == 0 || sample_rate == 0)
			{
				throw std::runtime_error(label + " describes " +
					std::to_string(channels) + " channels at " +
					std::to_string(sample_rate) + " Hz, which is not a sound.");
			}

			wave.channels = channels;
			wave.sample_rate = sample_rate;
			const unsigned char* data = bytes.data() + data_at;

			if (tag == TAG_PCM)
			{
				const int bytes_per_sample = sixteen_bit ? 2 : 1;
				if (block_align != channels * bytes_per_sample)
				{
					throw std::runtime_error(label + " is " +
						std::to_string(bytes_per_sample * 8) + "-bit PCM in " +
						std::to_string(channels) + " channels with a block "
						"alignment of " + std::to_string(block_align) +
						"; that is " +
						std::to_string(channels * bytes_per_sample) + ".");
				}
				if (data_length % static_cast<uint32_t>(block_align) != 0)
				{
					throw std::runtime_error(label + " holds " +
						std::to_string(data_length) + " bytes of PCM, which is "
						"not a whole number of " + std::to_string(block_align) +
						"-byte frames.");
				}

				const size_t samples = data_length /
					static_cast<size_t>(bytes_per_sample);
				wave.samples.reserve(samples);
				if (sixteen_bit)
				{
					wave.encoding = WaveEncoding::pcm16;
					for (size_t i = 0; i < samples; i++)
					{
						wave.samples.push_back(i16_at(data + 2 * i));
					}
				}
				else
				{
					// Eight-bit PCM is unsigned around 128, and widening it
					// to sixteen is exact.
					wave.encoding = WaveEncoding::pcm8;
					for (size_t i = 0; i < samples; i++)
					{
						wave.samples.push_back(static_cast<int16_t>(
							(static_cast<int>(data[i]) - 128) * 256));
					}
				}
				return;
			}

			// A tag is two bits and the other three values are handled above,
			// so this cannot fire; it is spelt out so that it is never silent.
			if (tag != TAG_ADPCM)
			{
				throw std::runtime_error(label + " has codec tag " +
					std::to_string(tag) + ", which this engine does not read.");
			}
			if (channels > 2)
			{
				throw std::runtime_error(label + " is MS-ADPCM in " +
					std::to_string(channels) + " channels; this engine "
					"decodes mono and stereo, which is what XAudio2 plays.");
			}

			const size_t full_align = static_cast<size_t>(
				(block_align + ADPCM_BLOCK_ALIGN_OFFSET) * channels);
			const size_t samples_per_block = full_align * 2 /
				static_cast<size_t>(channels) - 12;
			if (data_length % full_align != 0)
			{
				throw std::runtime_error(label + " holds " +
					std::to_string(data_length) + " bytes of MS-ADPCM, which "
					"is not a whole number of " + std::to_string(full_align) +
					"-byte blocks. XAudio2 cannot play a part block either; "
					"re-encode it.");
			}

			wave.encoding = WaveEncoding::adpcm;
			const size_t blocks = data_length / full_align;
			wave.samples.reserve(blocks * samples_per_block *
				static_cast<size_t>(channels));
			for (size_t b = 0; b < blocks; b++)
			{
				decode_adpcm_block(data + b * full_align, full_align, channels,
					label, b, wave.samples);
			}

			// The last block is padded to a whole block, and the entry's
			// duration says where the sound actually ends. A duration of zero
			// or one longer than the blocks hold says nothing usable, and the
			// blocks are then the whole wave.
			const size_t frames = wave.frames();
			if (duration > 0 && duration < frames)
			{
				wave.samples.resize(static_cast<size_t>(duration) *
					static_cast<size_t>(channels));
			}
		}
	}

	WaveBankData read_xwb_file(const std::string& path)
	{
		const std::vector<unsigned char> bytes = read_file_bytes(path);
		ByteReader reader(bytes, path);

		const uint32_t signature = reader.read_u32();
		if (signature == BIG_ENDIAN_SIGNATURE)
		{
			throw std::runtime_error(path + " is a big-endian (Xbox 360) "
				"wave bank, which this engine does not read. Re-pack it for "
				"Windows.");
		}
		if (signature != SIGNATURE)
		{
			throw std::runtime_error(path +
				" is not a wave bank: it does not start with \"WBND\".");
		}

		std::ignore = reader.read_u32();                   // content version
		const uint32_t header_version = reader.read_u32();
		if (header_version != HEADER_VERSION)
		{
			throw std::runtime_error(path + " is a version " +
				std::to_string(header_version) + " wave bank; this engine "
				"reads version " + std::to_string(HEADER_VERSION) +
				", which is XACT 3's.");
		}

		// Every region is checked against the file before any is read, so a
		// truncated bank is refused by which part of it is missing.
		std::array<Segment, SEGMENT_COUNT> segments;
		for (size_t i = 0; i < SEGMENT_COUNT; i++)
		{
			segments[i].offset = reader.read_u32();
			segments[i].length = reader.read_u32();
			const uint64_t end = static_cast<uint64_t>(segments[i].offset) +
				segments[i].length;
			if (end > bytes.size())
			{
				throw std::runtime_error(path + ": its " + SEGMENT_NAMES[i] +
					" runs to byte " + std::to_string(end) + " of a " +
					std::to_string(bytes.size()) + "-byte file. It is "
					"truncated.");
			}
		}

		const Segment& bank_segment = segments[BANK_DATA];
		if (bank_segment.length < BANK_DATA_SIZE)
		{
			throw std::runtime_error(path + ": its bank data is " +
				std::to_string(bank_segment.length) + " bytes; it is " +
				std::to_string(BANK_DATA_SIZE) + ".");
		}
		const size_t bank_at = bank_segment.offset;
		const uint32_t flags = u32_at(bytes, bank_at);
		const uint32_t entry_count = u32_at(bytes, bank_at + 4);
		const std::string bank_name =
			fixed_string(bytes, bank_at + 8, BANK_NAME_LENGTH);
		const uint32_t entry_size = u32_at(bytes, bank_at + 72);
		const uint32_t name_size = u32_at(bytes, bank_at + 76);

		if ((flags & FLAG_COMPACT) != 0)
		{
			throw std::runtime_error(path + " uses compact entries, which "
				"this engine does not read. Re-pack it with xwbtool -nc.");
		}
		if (entry_size < ENTRY_SIZE)
		{
			throw std::runtime_error(path + " declares " +
				std::to_string(entry_size) + "-byte entries; a standard entry "
				"is " + std::to_string(ENTRY_SIZE) + ".");
		}

		// The count sizes everything after it, so it is held to the region
		// that has to contain it before anything is allocated.
		const Segment& entries = segments[ENTRY_METADATA];
		if (static_cast<uint64_t>(entry_count) * entry_size > entries.length)
		{
			throw std::runtime_error(path + " says it has " +
				std::to_string(entry_count) + " entries of " +
				std::to_string(entry_size) + " bytes, and its entry metadata "
				"is " + std::to_string(entries.length) + " bytes.");
		}

		const bool named = (flags & FLAG_ENTRY_NAMES) != 0;
		const Segment& names = segments[ENTRY_NAMES];
		if (named && (name_size == 0 ||
			static_cast<uint64_t>(entry_count) * name_size > names.length))
		{
			throw std::runtime_error(path + " says it names its " +
				std::to_string(entry_count) + " entries in " +
				std::to_string(name_size) + " bytes each, and its name table "
				"is " + std::to_string(names.length) + " bytes.");
		}

		const Segment& wave_data = segments[ENTRY_WAVE_DATA];

		WaveBankData bank;
		bank.name = bank_name;
		bank.waves.reserve(entry_count);
		for (uint32_t i = 0; i < entry_count; i++)
		{
			WaveData wave;
			if (named)
			{
				wave.name = fixed_string(bytes,
					names.offset + static_cast<size_t>(i) * name_size,
					name_size);
			}
			const std::string label = entry_label(path, i, wave.name);

			const size_t entry_at = entries.offset +
				static_cast<size_t>(i) * entry_size;
			const uint32_t duration = u32_at(bytes, entry_at) >> 4;
			const uint32_t format = u32_at(bytes, entry_at + 4);
			const uint32_t play_offset = u32_at(bytes, entry_at + 8);
			const uint32_t play_length = u32_at(bytes, entry_at + 12);
			wave.loop_start = u32_at(bytes, entry_at + 16);
			wave.loop_length = u32_at(bytes, entry_at + 20);

			const uint64_t play_end =
				static_cast<uint64_t>(play_offset) + play_length;
			if (play_end > wave_data.length)
			{
				throw std::runtime_error(label + " plays bytes " +
					std::to_string(play_offset) + " to " +
					std::to_string(play_end) + " of wave data that is " +
					std::to_string(wave_data.length) + " bytes long.");
			}

			decode_entry(bytes,
				static_cast<size_t>(wave_data.offset) + play_offset,
				play_length, format, duration, label, wave);

			const uint64_t loop_end =
				static_cast<uint64_t>(wave.loop_start) + wave.loop_length;
			if (wave.loop_length > 0 && loop_end > wave.frames())
			{
				throw std::runtime_error(label + " loops frames " +
					std::to_string(wave.loop_start) + " to " +
					std::to_string(loop_end) + " of a wave " +
					std::to_string(wave.frames()) + " frames long.");
			}

			bank.waves.push_back(std::move(wave));
		}

		return bank;
	}
}
