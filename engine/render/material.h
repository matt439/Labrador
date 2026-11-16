#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace labrador
{
	enum class MaterialKind
	{
		d3d11,
		d3d12,
		spirv,
		glsl,
		null,
	};

	// Build output from labrador_add_material. The selected backend determines
	// the kind: Direct3D bytecode, SPIR-V, NUL-terminated GLSL, or empty null
	// data. name identifies the authored source in a runtime diagnostic.
	// Renderer::add_material copies both spans; their storage need not survive it.
	struct MaterialSource
	{
		MaterialKind kind = MaterialKind::null;
		std::span<const unsigned char> bytes;
		std::string_view name;
	};

	class MaterialTable;

	// A material registered with one Renderer. Default construction is
	// unresolved. Handles survive resize and device restoration and expire
	// with their renderer; another renderer refuses them.
	class MaterialHandle
	{
	public:
		MaterialHandle() = default;

		bool valid() const
		{
			return this->index_ >= 0;
		}
		int index() const
		{
			return this->index_;
		}

		friend bool operator==(MaterialHandle, MaterialHandle) = default;

	private:
		friend class MaterialTable;
		MaterialHandle(int index, std::uint64_t owner)
			: index_(index), owner_(owner) {}

		int index_ = -1;
		std::uint64_t owner_ = 0;
	};
}
