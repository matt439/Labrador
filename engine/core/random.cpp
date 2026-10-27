#include "engine/core/random.h"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace labrador
{
	std::uint32_t Random::next()
	{
		// The golden-ratio increment and Mulberry's mixing constants: the
		// SplitMix32 that LineSweeper's rules and particles already used,
		// draw for draw, so a game moving onto this keeps its sequences.
		this->position_ += 0x9E3779B9u;

		std::uint32_t value = this->position_;
		value = (value ^ (value >> 16)) * 0x21F0AAADu;
		value = (value ^ (value >> 15)) * 0x735A2D97u;

		return value ^ (value >> 15);
	}

	int Random::uniform_int(int minimum, int maximum)
	{
		if (minimum > maximum)
		{
			throw std::invalid_argument("Random::uniform_int: minimum " +
				std::to_string(minimum) + " is above maximum " +
				std::to_string(maximum) + ".");
		}

		// The span as a 64-bit count, because the widest range holds 2^32
		// values and does not fit in 32 bits.
		const std::uint64_t span = static_cast<std::uint64_t>(
			static_cast<std::int64_t>(maximum) -
			static_cast<std::int64_t>(minimum)) + 1u;

		if (span > UINT32_MAX)
		{
			return static_cast<int>(static_cast<std::int64_t>(minimum) +
				static_cast<std::int64_t>(this->next()));
		}

		// Rejection: of the 2^32 draws, the lowest 2^32 mod span would land
		// one extra time on the low end of the range, so they are drawn again.
		// 0u - span is 2^32 - span in unsigned arithmetic.
		const std::uint32_t range = static_cast<std::uint32_t>(span);
		const std::uint32_t threshold = (0u - range) % range;
		std::uint32_t draw = this->next();
		while (draw < threshold)
		{
			draw = this->next();
		}

		return static_cast<int>(static_cast<std::int64_t>(minimum) +
			static_cast<std::int64_t>(draw % range));
	}

	float Random::unit()
	{
		return static_cast<float>(this->next() >> 8) /
			static_cast<float>(1u << 24);
	}

	float Random::uniform_float(float minimum, float maximum)
	{
		if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
			!std::isfinite(maximum - minimum) || minimum > maximum)
		{
			throw std::invalid_argument("Random::uniform_float: the range " +
				std::to_string(minimum) + " to " + std::to_string(maximum) +
				" is not a finite range with minimum at most maximum.");
		}

		return minimum + (maximum - minimum) * this->unit();
	}

	bool Random::chance(float probability)
	{
		if (std::isnan(probability))
		{
			throw std::invalid_argument(
				"Random::chance: the probability is NaN.");
		}

		return this->unit() < probability;
	}
}
