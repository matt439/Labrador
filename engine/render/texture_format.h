#pragma once

namespace artattack
{
	// The pixel layouts this engine's content actually arrives in, named in the
	// engine's own vocabulary rather than in any one API's.
	//
	// WHY THIS EXISTS AT ALL. A font's atlas is inside its .spritefont, as raw
	// bytes and a format number, so the file has to say which layout those bytes
	// are in - and the file says it as a DXGI_FORMAT, which is a number the
	// engine may not repeat outside engine/render/<backend>/. This enum is the
	// translation, and the backend that consumes it is the only thing that turns
	// one of these back into an API constant.
	//
	// THREE ENTRIES, BECAUSE THAT IS WHAT MakeSpriteFont EMITS. Its
	// /TextureFormat switch offers Auto, Rgba32, Bgra4444 and CompressedMono,
	// which is r8g8b8a8_unorm, b4g4r4a4_unorm and bc2_unorm - Auto being a
	// choice between them rather than a fourth. Anything else in a file is
	// rejected by name (T6) rather than passed through to fail as a device
	// error, and this list grows when a real file needs it to (T1).
	//
	// bc2_unorm is the one to watch on a second backend. Desktop GL reaches it
	// through EXT_texture_compression_s3tc, which is universally present but is
	// still an extension; GLES 3.0 has no S3TC at all. Every font in both
	// clients is bc2_unorm today, so a backend that cannot decode it has no
	// text - which is the sort of thing worth knowing before the port rather
	// than during it.
	enum class TextureFormat
	{
		r8g8b8a8_unorm,
		b4g4r4a4_unorm,
		bc2_unorm,
	};
}
