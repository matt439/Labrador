#include "engine/render/material_table.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace labrador
{
	namespace
	{
		std::uint64_t next_material_identity()
		{
			static std::atomic<std::uint64_t> next{ 1 };
			std::uint64_t identity = next.load(std::memory_order_relaxed);
			for (;;)
			{
				if (identity == std::numeric_limits<std::uint64_t>::max())
				{
					throw std::length_error("Material tables have exhausted their handle identities.");
				}
				if (next.compare_exchange_weak(identity, identity + 1,
					std::memory_order_relaxed))
				{
					return identity;
				}
			}
		}

		void check_source(const MaterialSource& source, MaterialKind expected)
		{
			if (source.kind != expected)
			{
				throw std::invalid_argument("add_material: " + std::string(source.name) +
					" was built for another render backend. Rebuild it with "
					"labrador_add_material for this configuration.");
			}
			if (source.name.empty() || source.name.find('\0') != std::string_view::npos)
			{
				throw std::invalid_argument("add_material needs a nonempty source name "
					"without NUL characters for diagnostics.");
			}
			const std::span<const unsigned char> bytes = source.bytes;
			bool valid = false;
			switch (expected)
			{
			case MaterialKind::null:
				valid = bytes.empty();
				break;
			case MaterialKind::d3d11:
			case MaterialKind::d3d12:
				valid = bytes.size() >= 4 && bytes[0] == 'D' && bytes[1] == 'X' &&
					bytes[2] == 'B' && bytes[3] == 'C';
				break;
			case MaterialKind::spirv:
				valid = bytes.size() >= 20 && bytes.size() % 4 == 0 &&
					bytes[0] == 0x03 && bytes[1] == 0x02 &&
					bytes[2] == 0x23 && bytes[3] == 0x07;
				break;
			case MaterialKind::glsl:
				valid = bytes.size() > 1 && bytes.back() == 0 &&
					std::find(bytes.begin(), bytes.end() - 1, 0) == bytes.end() - 1;
				break;
			}
			if (!valid)
			{
				throw std::invalid_argument("add_material: " + std::string(source.name) +
					" has empty or malformed build output. Generate it with "
					"labrador_add_material for this backend.");
			}
		}
	}

	MaterialTable::MaterialTable()
		: identity_(next_material_identity()) {}

	MaterialHandle MaterialTable::add(const MaterialSource& source,
		MaterialKind expected, bool device_created, bool frame_begun)
	{
		if (!device_created || frame_begun)
		{
			throw std::logic_error("add_material requires create_device and must "
				"run outside a frame, before begin_frame or after end_frame.");
		}
		check_source(source, expected);
		if (this->materials_.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			throw std::length_error("add_material has exhausted its handle indices.");
		}
		StoredMaterial stored{ source.kind,
			std::vector<unsigned char>(source.bytes.begin(), source.bytes.end()),
			std::string(source.name), {} };
		const int index = static_cast<int>(this->materials_.size());
		this->materials_.push_back(std::move(stored));
		return MaterialHandle(index, this->identity_);
	}

	void MaterialTable::remove_last() noexcept
	{
		this->materials_.pop_back();
	}

	std::size_t MaterialTable::size() const
	{
		return this->materials_.size();
	}

	MaterialSource MaterialTable::source(std::size_t index) const
	{
		const StoredMaterial& stored = this->materials_.at(index);
		return MaterialSource{ stored.kind, stored.bytes, stored.name };
	}

	std::size_t MaterialTable::checked_index(MaterialHandle handle) const
	{
		if (!handle.valid() || handle.owner_ != this->identity_ ||
			static_cast<std::size_t>(handle.index()) >= this->materials_.size())
		{
			throw std::invalid_argument("The material handle is unresolved or belongs "
				"to another Renderer. Use a handle returned by this renderer's add_material.");
		}
		return static_cast<std::size_t>(handle.index());
	}

	MaterialHandle MaterialTable::selected() const
	{
		return this->selected_;
	}

	bool MaterialTable::has_selection() const
	{
		return this->selected_.valid();
	}

	void MaterialTable::select(MaterialHandle handle, bool frame_begun)
	{
		if (frame_begun)
		{
			throw std::logic_error("set_post_material must run outside a frame, "
				"before begin_frame or after end_frame.");
		}
		this->checked_index(handle);
		this->selected_ = handle;
	}

	void MaterialTable::clear_selection()
	{
		this->selected_ = MaterialHandle();
	}

	void MaterialTable::set_values(MaterialHandle handle, const PostValues& values)
	{
		const std::size_t index = this->checked_index(handle);
		if (handle != this->selected_)
		{
			throw std::invalid_argument("set_post_values requires the selected material. "
				"Call set_post_material with this handle first.");
		}
		for (const float value : values.values)
		{
			if (!std::isfinite(value))
			{
				throw std::invalid_argument("set_post_values requires finite material values.");
			}
		}
		this->materials_[index].values = values;
	}

	const PostValues& MaterialTable::values() const
	{
		return this->materials_[this->checked_index(this->selected_)].values;
	}
}
