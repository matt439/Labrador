#pragma once

#include <cstdint>

namespace labrador
{
	// A stream of pseudo-random numbers that is the same on every platform the
	// engine builds for: the same seed gives the same numbers on Windows and in
	// a browser, in Debug and in Release.
	//
	// THAT IS WHAT THE STANDARD LIBRARY DOES NOT PROMISE. Its engines are
	// specified to the bit, but its distributions are not:
	// std::uniform_int_distribution and std::uniform_real_distribution are
	// free to turn the same engine output into different numbers, and MSVC's
	// library and the browser build's libc++ do. A game that seeds a level, a
	// deal or a replay from <random> gets a different one on each platform.
	// Every step here is integer arithmetic or a single correctly rounded
	// float operation, so there is nothing for a library or a compiler to
	// choose.
	//
	// The generator is SplitMix32: a counter advanced by a fixed odd constant,
	// and the bits come from mixing it. The state is therefore a POSITION in
	// one sequence rather than an opaque register. Copying a Random copies the
	// draw that comes next, so a snapshot of a game holding one restores its
	// future as well as its past; and a default-constructed one, at position
	// zero, is as good a start as any other. The sequence repeats after 2^32
	// draws.
	//
	// NOT FOR ANYTHING AN ADVERSARY MAY PREDICT. The next number follows from
	// any one previous number.
	//
	// A value, like everything else that carries game state: no hidden global
	// stream, no clock. Seeding from the time is the game's choice, made where
	// the game can see it, because a clock is exactly what a replay cannot
	// have.
	class Random
	{
	public:
		Random() = default;
		explicit Random(std::uint32_t seed) : position_(seed) {}

		// The next 32 bits.
		std::uint32_t next();

		// An integer from `minimum` to `maximum`, both included, with every
		// value equally likely: draws that would favour some values are
		// discarded rather than reduced, so a range may use more than one
		// draw. Throws std::invalid_argument if `minimum` is above `maximum`.
		int uniform_int(int minimum, int maximum);

		// A float at least 0 and below 1, from the top 24 bits of one draw:
		// every value is a multiple of 2^-24, which is every float in that
		// range a 24-bit significand can tell apart, and none rounds up to 1.
		float unit();

		// A float from `minimum` to `maximum`: minimum + (maximum - minimum)
		// times unit(). Rounding can return `maximum` itself, so treat the
		// range as closed. Throws std::invalid_argument if either end or the
		// distance between them is not finite, or `minimum` is above
		// `maximum`.
		float uniform_float(float minimum, float maximum);

		// True with the given probability: unit() below it. Zero or less is
		// never and one or more is always, and each uses a draw like any
		// other, so the stream advances the same whatever the probability.
		// Throws std::invalid_argument for NaN.
		bool chance(float probability);

		// Where the stream is: what a save or a replay records, and what a
		// Random constructed from it continues from.
		std::uint32_t position() const { return this->position_; }

		bool operator==(const Random& other) const = default;

	private:
		std::uint32_t position_ = 0;
	};
}
