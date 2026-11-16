#pragma once

#include "engine/render/material.h"
#include "engine/render/post_effect.h"

#include <cstddef>
#include <string>
#include <vector>

namespace labrador
{
	// Retained authoring output and values, independent of the device objects
	// each backend builds from them. Each table has a distinct handle identity,
	// even when a later table reuses the same storage.
	class MaterialTable
	{
	public:
		MaterialTable();
		MaterialTable(const MaterialTable&) = delete;
		MaterialTable& operator=(const MaterialTable&) = delete;

		MaterialHandle add(const MaterialSource& source, MaterialKind expected,
			bool device_created, bool frame_begun);
		void remove_last() noexcept;
		std::size_t size() const;
		MaterialSource source(std::size_t index) const;

		MaterialHandle selected() const;
		bool has_selection() const;
		void select(MaterialHandle handle, bool frame_begun);
		void clear_selection();
		void set_values(MaterialHandle handle, const PostValues& values);
		const PostValues& values() const;

	private:
		struct StoredMaterial
		{
			MaterialKind kind;
			std::vector<unsigned char> bytes;
			std::string name;
			PostValues values;
		};

		std::size_t checked_index(MaterialHandle handle) const;
		const std::uint64_t identity_;
		std::vector<StoredMaterial> materials_;
		MaterialHandle selected_;
	};
}
