#include "engine/render/resolution_manager.h"

#include <stdexcept>
#include <string>

namespace labrador
{
	namespace
	{
		// The four presets, each once. Every conversion below searches this.
		struct Preset
		{
			ScreenResolution resolution;
			Vector2I size;
			const char* name;
		};

		const Preset presets[] =
		{
			{ ScreenResolution::s_1280_720, Vector2I(1280, 720), "1280x720" },
			{ ScreenResolution::s_1920_1080, Vector2I(1920, 1080), "1920x1080" },
			{ ScreenResolution::s_2560_1440, Vector2I(2560, 1440), "2560x1440" },
			{ ScreenResolution::s_3840_2160, Vector2I(3840, 2160), "3840x2160" },
		};

		// The preset with this label, or null for a value outside the enum.
		const Preset* find_preset(ScreenResolution resolution)
		{
			for (const Preset& preset : presets)
			{
				if (preset.resolution == resolution)
				{
					return &preset;
				}
			}
			return nullptr;
		}

		Vector2I convert_resolution_to_ivec(ScreenResolution resolution)
		{
			const Preset* preset = find_preset(resolution);
			return preset != nullptr ? preset->size : Vector2I(-1, -1);
		}

		// The preset this size is exactly, and s_1280_720 for any other.
		ScreenResolution convert_ivec_to_resolution(const Vector2I& size)
		{
			for (const Preset& preset : presets)
			{
				if (preset.size == size)
				{
					return preset.resolution;
				}
			}
			return ScreenResolution::s_1280_720;
		}

		// The same, for a size in floats, which matches only a whole one.
		ScreenResolution convert_vec_to_resolution(const Vector2F& size)
		{
			for (const Preset& preset : presets)
			{
				if (Vector2F(preset.size) == size)
				{
					return preset.resolution;
				}
			}
			return ScreenResolution::s_1280_720;
		}
	}

	ResolutionManager::ResolutionManager()
	{
		// Through the setter, so the size cannot disagree with the preset it is
		// supposed to be the size of.
		this->set_resolution(this->resolution_);
	}

	Vector2I ResolutionManager::resolution_ivec() const
	{
		return this->size_;
	}

	Vector2F ResolutionManager::resolution_vec() const
	{
		return Vector2F(this->size_);
	}

	std::string ResolutionManager::resolution_string() const
	{
		return convert_resolution_to_string(this->resolution_);
	}

	ScreenResolution ResolutionManager::resolution() const
	{
		return this->resolution_;
	}

	void ResolutionManager::set_resolution(ScreenResolution resolution)
	{
		// Both members, from the one argument. Every other overload lands here,
		// so there is one place that decides what a preset means.
		this->resolution_ = resolution;
		this->size_ = convert_resolution_to_ivec(resolution);
	}

	void ResolutionManager::set_resolution(const Vector2F& resolution)
	{
		this->set_resolution(convert_vec_to_resolution(resolution));
	}

	void ResolutionManager::set_resolution(const Vector2I& resolution)
	{
		// Still coercing, deliberately unchanged: this overload has always meant
		// "pick the preset nearest this", it answers 1280x720 for anything it
		// does not recognise, and a client may be relying on exactly that. The
		// non-coercing path is set_resolution_exactly, and the reason it is a
		// separate function rather than a change of heart here is that these two
		// are different questions with the same arguments.
		this->set_resolution(convert_ivec_to_resolution(resolution));
	}

	void ResolutionManager::set_resolution_exactly(const Vector2I& size)
	{
		if (size.x <= 0 || size.y <= 0)
		{
			throw std::invalid_argument("ResolutionManager::"
				"set_resolution_exactly - a resolution of " +
				std::to_string(size.x) + "x" + std::to_string(size.y) +
				" has no positive extent, and everything above this divides by "
				"it.");
		}

		this->size_ = size;

		// The label follows only if this size really is a preset, and the round
		// trip is what makes that test exact rather than approximate:
		// convert_ivec_to_resolution answers s_1280_720 for everything it does
		// not recognise, so converting the answer back and comparing is the
		// difference between "this size IS 720p" and "this size has just been
		// replaced by 720p". A size with no name keeps the last named one.
		const ScreenResolution candidate = convert_ivec_to_resolution(size);
		if (convert_resolution_to_ivec(candidate) == size)
		{
			this->resolution_ = candidate;
		}
	}

	std::string ResolutionManager::convert_resolution_to_string(
		ScreenResolution resolution)
	{
		const Preset* preset = find_preset(resolution);
		return std::string(preset != nullptr ? preset->name : "-1x-1");
	}
}
