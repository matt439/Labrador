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
	// of its traffic - every backend with a device consumes it, by a different
	// route and with a different answer about what it will take, and the null
	// backend never reads it, because it keeps a width and a height and throws
	// the bytes away.
	//
	// THE LIST IS WHAT THE CONTENT IS, AND NOTHING ELSE (T1). Art arrives
	// block-compressed as DXT4 or DXT5, and so as bc3_unorm, or uncompressed as
	// b8g8r8a8_unorm. A font atlas is a texture too and is bc2_unorm - the one
	// this repository ships is pinned as such by sprite_font_file_tests.cpp.
	// bc1_unorm and r8g8b8a8_unorm are here because they complete a switch over
	// what a .dds and a .spritefont can say rather than because a file says
	// them, and b4g4r4a4_unorm because MakeSpriteFont offers it. A file naming
	// anything else is rejected by name (T6) rather than passed through to fail
	// as a device error, and this list grows when a real file needs it to.
	//
	// THE BLOCK-COMPRESSED ENTRIES ARE THE ONES TO WATCH ON A BACKEND, and they
	// are not a corner of the content: compressed art is the common case and
	// every font atlas is one, so a backend that cannot sample them has no text
	// and, for most content, no art. The failure worth having is the one that
	// says which feature is missing rather than the one that draws nothing.
	// engine/render/gl/texture_factory.cpp names
	// GL_EXT_texture_compression_s3tc in its throw - universally present on a
	// desktop driver and absent from GLES 3.0 entirely - and
	// engine/render/vulkan/texture_factory.cpp names textureCompressionBC,
	// which the device is asked for when it is selected.
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
