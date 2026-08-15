#pragma once

#include "engine/render/texture_data.h"

#include <string>

namespace labrador
{
	// Reads the .dds at `path`.
	//
	// WHAT IT REPLACED AND WHY. DirectXTK's CreateDDSTextureFromFile did this
	// and made the device texture in one call, which meant the only description
	// of what this engine's content actually is lived inside a library that
	// exists for one API. A second backend would have had to find another
	// reader and hope it agreed - about which fourCC is which format, about
	// whether DXT4's premultiplied alpha maps to BC2 or BC3, about the row
	// pitch of a compressed mip. Those are not opinions, but they are also not
	// obvious, and nothing in this tree wrote any of them down.
	//
	// WHAT IT DELIBERATELY DOES NOT DO, all of which DDSTextureLoader did and
	// no content here needs (T1): cube maps, volume textures, texture arrays,
	// the DX10 extended header, sRGB forcing, mip generation, and every format
	// outside texture_format.h. Each is a named throw rather than a silent
	// difference, so a file that needs one says so at load rather than drawing
	// something slightly wrong.
	//
	// Throws std::out_of_range naming the path if there is no file there, and
	// std::runtime_error naming the path AND what is wrong with it otherwise -
	// which is what CreateDDSTextureFromFile could not do, and why loading a
	// texture used to report every failure as an eight-digit HRESULT.
	TextureData read_dds_file(const std::string& path);
}
