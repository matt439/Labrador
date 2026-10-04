#pragma once

#include "engine/render/texture_data.h"

#include <string>

namespace labrador
{
	// Reads the .dds at `path`.
	//
	// THE ENGINE READS IT, NOT A BACKEND. A loader that also makes the device
	// texture, as DirectXTK's CreateDDSTextureFromFile does, puts the only
	// description of what this engine's content actually is inside a library
	// that exists for one API, and every other backend has to find another
	// reader and hope it agrees - about which fourCC is which format, about
	// whether DXT4's premultiplied alpha maps to BC2 or BC3, about the row
	// pitch of a compressed mip. Those are not opinions, but they are also not
	// obvious, and this reader is where they are written down, once.
	//
	// WHAT IT DELIBERATELY DOES NOT DO, all of which DDSTextureLoader does and
	// no content here needs (T1): cube maps, volume textures, texture arrays,
	// the DX10 extended header, sRGB forcing, mip generation, and every format
	// outside texture_format.h. FOUR OF THE SEVEN ARE NAMED THROWS - cube maps,
	// volume textures, the DX10 header and an unknown format - so a file that
	// needs one of those says so at load rather than drawing something slightly
	// wrong. A texture array meets the DX10 throw, because only that header can
	// describe one. The other two are absences rather than refusals: nothing
	// here forces sRGB or generates a mip chain, and a file gets the levels it
	// carries.
	//
	// Throws std::out_of_range naming the path if there is no file there, and
	// std::runtime_error naming the path AND what is wrong with it otherwise -
	// which is what CreateDDSTextureFromFile cannot do, and why routing through
	// it reports every failure to load a texture as an eight-digit HRESULT.
	TextureData read_dds_file(const std::string& path);
}
