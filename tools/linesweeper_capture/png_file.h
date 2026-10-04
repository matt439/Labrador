#pragma once

#include <filesystem>
#include <vector>

namespace capture
{
	// Writes a frame Renderer::read_back_buffer handed over - tightly packed
	// 8-bit RGBA, top row first, width * height * 4 bytes - as a PNG of its
	// red, green and blue.
	//
	// THE ALPHA CHANNEL IS DROPPED, deliberately. A screenshot is what a
	// display shows, and a display does not show the back buffer's alpha: it
	// is whatever the blend equation left behind, and an image viewer or a web
	// page would composite it against their own background as transparency
	// the game never had.
	//
	// THROUGH WIC, the codec Windows already has, as
	// tests/render/golden_image.cpp does - so this costs no dependency (T9).
	// It is a writer of its own rather than a share of that one because that
	// one lives in a doctest executable and an anonymous namespace, and making
	// it public would be test code becoming a dependency of a tool. Lossless
	// either way: what is written is the bytes that were read back, less the
	// alpha.
	//
	// COM must be initialised on the calling thread; Application::initialize
	// does that. Throws std::runtime_error naming the file and the step that
	// failed, and std::invalid_argument for a buffer that is not the size the
	// dimensions say.
	void write_png(const std::filesystem::path& path, int width, int height,
		const std::vector<unsigned char>& rgba);
}
