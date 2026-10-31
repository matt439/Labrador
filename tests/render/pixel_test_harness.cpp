#include "tests/render/pixel_test_harness.h"

#include "engine/render/block_decode.h"
#include "engine/render/resource_factory.h"
#include "engine/render/sprite_font_file.h"
#include "tests/render/golden_image.h"

#include <doctest/doctest.h>

#include <ostream>
#include <vector>

using namespace labrador;

namespace render_tests::pixel
{
	bool Pixel::operator==(const Pixel& other) const
	{
		return this->r == other.r && this->g == other.g &&
			this->b == other.b && this->a == other.a;
	}

	std::ostream& operator<<(std::ostream& stream, const Pixel& pixel)
	{
		return stream << "rgba(" << static_cast<int>(pixel.r) << ", "
			<< static_cast<int>(pixel.g) << ", " << static_cast<int>(pixel.b)
			<< ", " << static_cast<int>(pixel.a) << ")";
	}

	// Two by two, one colour, one level. For the case that cares which bytes a
	// name is holding rather than what an atlas looks like.
	TextureData flat_texture(unsigned char red, unsigned char green,
		unsigned char blue)
	{
		TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.format = TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(texture_level(texture.format, 2, 2, 0));
		texture.pixels.resize(texture.levels[0].size);

		for (size_t byte = 0; byte < texture.pixels.size(); byte += 4)
		{
			texture.pixels[byte] = red;
			texture.pixels[byte + 1] = green;
			texture.pixels[byte + 2] = blue;
			texture.pixels[byte + 3] = 255;
		}

		return texture;
	}

	Harness::Harness()
	{
		// FOUR VIEWS OF CAPACITY, WHICH IS THE SAME NUMBER null_tests.cpp
		// asks for, and it is not four because anything here draws four.
		// A capacity is a property of the layout - four for split-screen,
		// one for a sample - and the two files asking for the same one is
		// what lets a case be written in the same vocabulary in both. With
		// one, no multi-view path - D3D11's per-view deferred contexts,
		// GL's per-view record-and-replay, view order against call order -
		// would reach a rasteriser.
		this->renderer_.create_device(this->window_.handle(), buffer_side,
			buffer_side, 4);
		this->renderer_.set_resources(&this->resources_);

		// content/ is copied beside the executable by this folder's
		// CMakeLists, and ctest runs from the build directory.
		load_texture_asset(this->renderer_, this->resources_,
			"./content/", "quad");
		this->quad = this->resources_.resolve_texture("quad");

		// The font is a copy of the one every sample uses rather than a
		// reference to it, for the same reason quad.dds is not a sample
		// texture: a sample that restyles itself must not be able to move a
		// pin in here. It is monospaced and bold, which is not decoration -
		// every character in it steps the pen by the same whole number of
		// pixels, so the advance cases below can be exact rather than
		// approximate.
		load_font_asset(this->renderer_, this->resources_, "./content/",
			"jetbrains_mono_bold_16");
		this->font = this->resources_.resolve_sprite_font(
			"jetbrains_mono_bold_16");

		// AND ITS ATLAS IS THEN RE-LOADED DECODED, by the engine's own
		// decoder (block_decode.h), under the name resource_factory.h gives
		// it, into the slot the font already holds.
		//
		// THE TEXT CASES ARE ABOUT WHERE INK LANDS, and a device's BC2
		// rounding is not where. A BC2 block's two interpolated colours
		// are a third and two thirds of the way between its endpoints, no
		// specification fixes how a decoder rounds them, and the decoders
		// this file has run on do not agree: decoding the atlas
		// themselves, this machine's GPU writes a fringe texel's green as
		// 80 where SwiftShader writes 85, and WARP is as far from the GPU
		// - one channel, every text frame, and nearly twice that where two
		// glyphs' fringes overlap and the blend compounds it. Decoded once
		// here, every device samples the same bytes. Sampling a
		// block-compressed texture is still held to pixels, by the BC2
		// case below, with blocks whose texels are endpoints and so decode
		// exactly everywhere.
		add_texture_asset(this->renderer_, this->resources_,
			"font:jetbrains_mono_bold_16",
			decode_block_compressed(read_sprite_font_file(
				"./content/jetbrains_mono_bold_16.spritefont").atlas));
	}

	DrawList Harness::begin(int views)
	{
		this->renderer_.begin_frame();
		this->renderer_.set_view_count(views);
		return this->renderer_.view(0);
	}

	DrawList Harness::view(int index)
	{ return this->renderer_.view(index); }

	Renderer& Harness::renderer()
	{ return this->renderer_; }

	RenderResources& Harness::resources()
	{ return this->resources_; }

	void Harness::end()
	{
		this->read_frame();

		// And the frame itself is checked against the image of it that is
		// in the repository - the one statement in this file that holds one
		// backend to another rather than holding each separately to a
		// relationship they can both get wrong in the same direction
		// (tests/render/golden_image.h says why that gap exists at all).
		//
		// Here rather than in each case, so that the coverage grows with
		// the file instead of with a list somebody has to remember to
		// extend: a case added tomorrow is golden tomorrow.
		golden::check_frame(static_cast<int>(this->buffer_.x),
			static_cast<int>(this->buffer_.y), this->pixels_);
	}

	void Harness::end_not_comparable()
	{
		this->read_frame();
	}

	void Harness::resize_window(int width, int height)
	{
		REQUIRE(this->window_.resize(width, height));
	}

	Vector2F Harness::buffer_size() const
	{ return this->buffer_; }

	size_t Harness::byte_count() const
	{ return this->pixels_.size(); }

	Pixel Harness::at(int x, int y) const
	{
		const size_t stride = static_cast<size_t>(this->buffer_.x) * 4;
		const size_t offset =
			static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
		// Guarded rather than asserted unconditionally: ink_bounds walks
		// every pixel of every text case, and an assertion per read would
		// bury the file's real assertion count under four thousand.
		if (offset + 3 >= this->pixels_.size())
		{
			REQUIRE_MESSAGE(false, "pixel (", x, ", ", y,
				") is outside the back buffer");
			return {};
		}
		return { this->pixels_[offset], this->pixels_[offset + 1],
			this->pixels_[offset + 2], this->pixels_[offset + 3] };
	}

	std::vector<Pixel> Harness::snapshot() const
	{
		std::vector<Pixel> frame;
		frame.reserve(buffer_side * buffer_side);
		for (int y = 0; y < buffer_side; y++)
		{
			for (int x = 0; x < buffer_side; x++)
			{
				frame.push_back(this->at(x, y));
			}
		}
		return frame;
	}

	RectangleI Harness::ink_bounds() const
	{
		int left = buffer_side;
		int top = buffer_side;
		int right = -1;
		int bottom = -1;

		for (int y = 0; y < buffer_side; y++)
		{
			for (int x = 0; x < buffer_side; x++)
			{
				if (this->at(x, y) == black_pixel)
				{
					continue;
				}
				if (x < left) { left = x; }
				if (y < top) { top = y; }
				if (x > right) { right = x; }
				if (y > bottom) { bottom = y; }
			}
		}

		if (right < 0)
		{
			return RectangleI::zero;
		}
		// Right and bottom are exclusive here, as they are everywhere else
		// in this file - RectangleI is (x, y, width, height).
		return RectangleI(left, top, right - left + 1, bottom - top + 1);
	}

	bool Harness::row_has_ink(int y) const
	{
		for (int x = 0; x < buffer_side; x++)
		{
			if (!(this->at(x, y) == black_pixel))
			{
				return true;
			}
		}
		return false;
	}

	Vector2F Harness::measure(const std::wstring& text) const
	{
		return this->resources_.measure_text(this->font, text);
	}

	RectangleI Harness::whole()
	{ return RectangleI(0, 0, 2, 2); }

	RectangleI Harness::red_texel()
	{ return RectangleI(0, 0, 1, 1); }

	RectangleI Harness::white_texel()
	{ return RectangleI(1, 1, 1, 1); }

	void Harness::read_frame()
	{
		this->renderer_.submit();
		this->renderer_.read_back_buffer(this->pixels_);
		this->buffer_ = this->renderer_.back_buffer_size();
	}
}
