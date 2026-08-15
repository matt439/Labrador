#include "engine/render/resource_factory.h"

#include "engine/render/null/backend.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/texture_data.h"

#include <memory>
#include <string>
#include <tuple>

namespace artattack
{
	// The whole of this backend's share of loading content, and it keeps two
	// numbers.
	//
	// THE BYTES ARE READ AND THROWN AWAY, WHICH IS NOT THE SAME AS NOT READING
	// THEM. Everything up to this point has already happened: the file was
	// opened, the header parsed, the format recognised or rejected by name, the
	// mip chain measured. A test running against this backend gets every one of
	// those failures exactly as a client would - a missing texture, a cube map,
	// a truncated .spritefont - and the only thing it does not get is a
	// picture. That is why this is a backend rather than a stub in the loader:
	// the load path is the real one.
	void add_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& name,
		const TextureData& texture)
	{
		std::ignore = renderer;

		resources.impl()->add_texture(name,
			std::make_unique<NullTexture>(texture.width, texture.height));
	}
}
