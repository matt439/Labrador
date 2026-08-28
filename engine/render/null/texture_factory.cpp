#include "engine/render/resource_factory.h"

#include "engine/render/null/backend.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/texture_data.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace labrador
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
		// THE ONE THING THE RENDERER IS ASKED HERE, and it is asked because
		// there is nothing else to ask it. The other four backends refuse this
		// call before create_device because they have to - an ID3D11Device, an
		// ID3D12Device, a WGL context or a VkDevice is null and there is
		// nothing to build on - and a backend with no device would sail
		// through, hand back a resolvable handle and draw with it. That is the wrong way round: this is the configuration
		// a client is most likely to be tested in, so a rule it cannot enforce
		// is a rule that reaches a shipping build unbroken. The seam states the
		// ordering (resource_factory.h) and all five keep it, each throwing a
		// runtime_error that names the texture.
		if (!renderer.impl()->device_created)
		{
			throw std::runtime_error("Texture '" + name + "' was loaded before "
				"create_device.");
		}

		resources.impl()->add_texture(name,
			std::make_unique<NullTexture>(texture.width, texture.height));
	}
}
