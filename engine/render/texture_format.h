#pragma once

namespace artattack
{
	// The pixel layouts this engine's content actually arrives in, named in the
	// engine's own vocabulary rather than in any one API's.
	//
	// WHY THIS EXISTS AT ALL. Both file kinds this engine loads carry a texture
	// and say what format it is in, and both say it as a DXGI number - a .dds
	// because that is the file format, a .spritefont because MakeSpriteFont
	// wrote one down. A DXGI number is a thing the engine may not repeat outside
	// engine/render/<backend>/. This enum is the translation, and the two
	// readers that produce one and the one backend that consumes one are the
	// whole of its traffic.
	//
	// THE LIST IS WHAT THE CONTENT IS, AND NOTHING ELSE (T1). Between this
	// repository and the client that consumes it there are 43 textures and two
	// fonts, and they are: 41 block-compressed images, all DXT4 or DXT5 and so
	// both bc3_unorm; two uncompressed images, both b8g8r8a8_unorm; and two font
	// atlases, both bc2_unorm. bc1_unorm and r8g8b8a8_unorm are here because
	// they complete a switch over what a .dds and a .spritefont can say rather
	// than because a file says them, and b4g4r4a4_unorm because MakeSpriteFont
	// offers it. A file naming anything else is rejected by name (T6) rather
	// than passed through to fail as a device error, and this list grows when a
	// real file needs it to.
	//
	// THE BLOCK-COMPRESSED ENTRIES ARE THE ONES TO WATCH ON A SECOND BACKEND,
	// and they are not a corner of the content - they are 43 of the 45 images
	// there are. Desktop GL reads them through EXT_texture_compression_s3tc,
	// which is universally present but is still an extension; GLES 3.0 has no
	// S3TC at all. A backend that skipped them would have no art and no text -
	// which is the sort of thing worth knowing before a port rather than during
	// one.
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
