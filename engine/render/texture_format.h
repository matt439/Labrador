#pragma once

namespace labrador
{
	// The pixel layouts this engine's content actually arrives in, named in the
	// engine's own vocabulary rather than in any one API's.
	//
	// WHY THIS EXISTS AT ALL. Both file kinds this engine loads carry a texture
	// and say what format it is in, and both say it as a DXGI number - a .dds
	// because that is the file format, a .spritefont because MakeSpriteFont
	// wrote one down. A DXGI number is a thing the engine may not repeat outside
	// engine/render/<backend>/. This enum is the translation, and the two
	// readers that produce one and the backends that consume one are the whole
	// of its traffic - three of the four do, by different routes and with
	// different answers about what they will take, and the null backend reads
	// it never, because it keeps a width and a height and throws the bytes
	// away.
	//
	// THE LIST IS WHAT THE CONTENT IS, AND NOTHING ELSE (T1). Between this
	// repository and the client that consumes it there are 43 .dds files, and
	// they are: 41 block-compressed, all DXT4 or DXT5 and so all bc3_unorm; and
	// two uncompressed, both b8g8r8a8_unorm. A font atlas is a texture too and
	// is bc2_unorm - the one this repository ships is pinned as such by
	// sprite_font_file_tests.cpp. THE FONT COUNT IS NOT A CENSUS AND WAS ONCE
	// WRITTEN AS ONE: there are 32 .spritefont files in the client and three
	// copies of a single font here, most of them loaded by nothing. What is on
	// the render path is a handful. bc1_unorm and r8g8b8a8_unorm are here
	// because they complete a switch over what a .dds and a .spritefont can say
	// rather than because a file says them, and b4g4r4a4_unorm because
	// MakeSpriteFont offers it. A file naming anything else is rejected by name
	// (T6) rather than passed through to fail as a device error, and this list
	// grows when a real file needs it to.
	//
	// THE BLOCK-COMPRESSED ENTRIES WERE THE ONES TO WATCH ON A SECOND BACKEND,
	// and they are not a corner of the content - they are 41 of the 43 .dds
	// there are, and every font atlas besides. That was written before the port
	// and the port has happened: engine/render/gl/texture_factory.cpp queries
	// GL_EXT_texture_compression_s3tc once and names it in the throw if it is
	// absent, which is universally present on a desktop driver and absent from
	// GLES 3.0 entirely. A backend that skipped them would have no art and no
	// text, so the failure worth having is the one that says which extension is
	// missing rather than the one that draws nothing.
	enum class TextureFormat
	{
		r8g8b8a8_unorm,
		b8g8r8a8_unorm,
		b4g4r4a4_unorm,
		bc1_unorm,
		bc2_unorm,
		bc3_unorm,
	};
}
