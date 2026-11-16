#include "engine/audio/wav_file.h"
#include "engine/audio/wave_data_checks.h"

#include "engine/core/byte_reader.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace labrador
{
	namespace
	{
		constexpr uint32_t riff_signature = 0x46464952u;
		constexpr uint32_t wave_signature = 0x45564157u;
		constexpr uint32_t format_signature = 0x20746D66u;
		constexpr uint32_t data_signature = 0x61746164u;
		constexpr uint32_t sampler_signature = 0x6C706D73u;

		struct Chunk
		{
			size_t at = 0;
			uint32_t size = 0;
		};

		struct LoopRegion
		{
			uint32_t start = 0;
			uint32_t length = 0;
		};

		[[noreturn]] void refuse(const std::string& path, const std::string& problem)
		{
			throw std::runtime_error("WAV '" + path + "': " + problem);
		}

		const char* chunk_name(uint32_t signature)
		{
			switch (signature)
			{
			case format_signature: return "fmt";
			case data_signature: return "data";
			case sampler_signature: return "smpl";
			default: return "unknown";
			}
		}

		uint16_t u16_at(const std::vector<unsigned char>& bytes, size_t at)
		{
			return static_cast<uint16_t>(static_cast<uint16_t>(bytes[at]) |
				(static_cast<uint16_t>(bytes[at + 1]) << 8));
		}

		uint32_t u32_at(const std::vector<unsigned char>& bytes, size_t at)
		{
			return static_cast<uint32_t>(bytes[at]) |
				(static_cast<uint32_t>(bytes[at + 1]) << 8) |
				(static_cast<uint32_t>(bytes[at + 2]) << 16) |
				(static_cast<uint32_t>(bytes[at + 3]) << 24);
		}

		LoopRegion read_loop(const std::vector<unsigned char>& bytes,
			const Chunk& sampler, size_t frames, const std::string& path)
		{
			if (sampler.size < 36) refuse(path, "smpl chunk has a truncated header");
			const uint32_t count = u32_at(bytes, sampler.at + 28);
			const uint32_t private_size = u32_at(bytes, sampler.at + 32);
			if (count > 1) refuse(path, "smpl chunk has more than one loop");
			const size_t records = static_cast<size_t>(count) * 24;
			const size_t available = static_cast<size_t>(sampler.size) - 36;
			if (records > available || static_cast<size_t>(private_size) != available - records)
			{
				refuse(path, "smpl chunk size disagrees with its loop and sampler-data sizes");
			}
			if (count == 0) return {};
			const size_t loop = sampler.at + 36;
			if (u32_at(bytes, loop + 4) != 0 || u32_at(bytes, loop + 16) != 0 ||
				u32_at(bytes, loop + 20) != 0)
			{
				refuse(path, "smpl supports only one forward integral infinite loop");
			}
			const uint32_t start = u32_at(bytes, loop + 8);
			const uint32_t end = u32_at(bytes, loop + 12);
			if (start > end || static_cast<size_t>(end) >= frames)
			{
				refuse(path, "smpl loop region is outside the wave");
			}
			return LoopRegion{ start, end - start + 1 };
		}
	}

	WaveData read_wav_file(const std::string& path)
	{
		const std::vector<unsigned char> bytes = read_file_bytes(path);
		if (bytes.size() < 12) refuse(path, "truncated RIFF/WAVE header");
		ByteReader reader(bytes, path);
		if (reader.read_u32() != riff_signature) refuse(path, "expected little-endian RIFF");
		const uint32_t riff_size = reader.read_u32();
		if (static_cast<size_t>(riff_size) != bytes.size() - 8)
		{
			refuse(path, "RIFF size disagrees with the file size");
		}
		if (reader.read_u32() != wave_signature) refuse(path, "RIFF form is not WAVE");

		std::optional<Chunk> format;
		std::optional<Chunk> data;
		std::optional<Chunk> sampler;
		while (reader.remaining() != 0)
		{
			if (reader.remaining() < 8) refuse(path, "truncated chunk header");
			const uint32_t signature = reader.read_u32();
			const uint32_t size = reader.read_u32();
			const Chunk chunk{ reader.position(), size };
			if (static_cast<size_t>(size) > reader.remaining())
			{
				refuse(path, std::string(chunk_name(signature)) + " chunk is truncated");
			}
			std::optional<Chunk>* selected = nullptr;
			if (signature == format_signature) selected = &format;
			else if (signature == data_signature) selected = &data;
			else if (signature == sampler_signature) selected = &sampler;
			if (selected != nullptr)
			{
				if (selected->has_value()) refuse(path, std::string("duplicate ") + chunk_name(signature) + " chunk");
				*selected = chunk;
			}
			reader.skip(size);
			if ((size & 1u) != 0)
			{
				if (reader.remaining() == 0) refuse(path, std::string("missing pad byte after ") + chunk_name(signature) + " chunk");
				reader.skip(1);
			}
		}
		if (!format) refuse(path, "missing fmt chunk");
		if (!data) refuse(path, "missing data chunk");
		if (format->size != 16 && format->size != 18)
		{
			refuse(path, "fmt chunk must contain 16 or 18 bytes of PCM format");
		}
		const size_t at = format->at;
		if (u16_at(bytes, at) != 1) refuse(path, "fmt encoding is not PCM (format tag 1)");
		if (format->size == 18 && u16_at(bytes, at + 16) != 0)
		{
			refuse(path, "fmt PCM extension size must be zero");
		}
		const uint16_t channels = u16_at(bytes, at + 2);
		const uint32_t rate = u32_at(bytes, at + 4);
		const uint32_t byte_rate = u32_at(bytes, at + 8);
		const uint16_t alignment = u16_at(bytes, at + 12);
		const uint16_t bits = u16_at(bytes, at + 14);
		if (channels != 1 && channels != 2) refuse(path, "fmt supports only mono or stereo PCM");
		if (rate < 8000 || rate > 96000) refuse(path, "fmt sample rate must be 8000..96000 Hz");
		if (bits != 8 && bits != 16) refuse(path, "fmt supports only unsigned 8-bit or signed 16-bit PCM");
		const uint32_t frame_bytes = static_cast<uint32_t>(channels) * (bits / 8u);
		if (alignment != frame_bytes) refuse(path, "fmt block alignment disagrees with channels and sample width");
		if (byte_rate != rate * frame_bytes) refuse(path, "fmt byte rate disagrees with rate and block alignment");
		if (data->size == 0 || data->size % frame_bytes != 0) refuse(path, "data must contain nonempty whole PCM frames");
		const size_t frames = data->size / frame_bytes;
		const size_t sample_count = data->size / (bits / 8u);
		if (!detail::portable_pcm_size_fits(sample_count, frames))
		{
			refuse(path, "decoded PCM exceeds the audio API size limits");
		}
		const LoopRegion loop = sampler ? read_loop(bytes, *sampler, frames, path) : LoopRegion{};

		WaveData wave;
		wave.encoding = bits == 8 ? WaveEncoding::pcm8 : WaveEncoding::pcm16;
		wave.channels = static_cast<int>(channels);
		wave.sample_rate = static_cast<int>(rate);
		wave.loop_start = loop.start;
		wave.loop_length = loop.length;
		wave.samples.reserve(sample_count);
		for (size_t sample = 0; sample < sample_count; ++sample)
		{
			const size_t position = data->at + sample * (bits / 8u);
			const int16_t value = bits == 8
				? static_cast<int16_t>((static_cast<int>(bytes[position]) - 128) * 256)
				: static_cast<int16_t>(u16_at(bytes, position));
			wave.samples.push_back(value);
		}
		return wave;
	}
}
