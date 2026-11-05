#pragma once

#include "engine/core/handle.h"
#include "engine/core/name_index.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace labrador
{
	// How a resource is reached through the smart pointer that owns it. The
	// primary template covers everything spelling it get() - unique_ptr,
	// shared_ptr. A storage type that spells it otherwise specialises this
	// where its own header is already in scope, rather than this header
	// including it.
	template <typename Storage>
	struct RegistryStorage
	{
		using pointer_type = decltype(std::declval<const Storage&>().get());

		static pointer_type pointer(const Storage& storage)
		{
			return storage.get();
		}
	};

	// A store of named resources. Reading through get() returns something live
	// or throws rather than returning nullptr. Neither get() nor resolve()
	// inserts a missing name, so a misspelt name fails where it is written
	// rather than null-dereferencing a frame later somewhere else.
	//
	// There are two ways to ask, and the difference between them is the point:
	//
	//     resolve(name) -> Handle       once, at load
	//     get(handle)   -> Resource*    per frame - no string, no map, no compare
	//
	// Per-frame code touches no string-keyed map (PHILOSOPHY T7, T8), so per-frame
	// code never sees a name. get(name) remains for load-time callers that have
	// one and want the resource immediately.
	//
	// A name owns its slot for the registry's lifetime. Releasing a resource -
	// for example, texture storage on device loss - empties the slot but keeps
	// the mapping. A handle resolved before release still names that slot,
	// and reads the replacement once a reload refills it. The handle is a slot
	// index rather than a pointer: refilling can replace the resource without
	// changing the slot.
	//
	// The registry holds names, not meanings. What a name refers to is the
	// caller's business - which is what lets a game keep its own resource types
	// in one of these without the engine having to learn their vocabulary.
	template <typename Resource, typename Storage = std::unique_ptr<Resource>>
	class Registry
	{
	public:
		// Lowercase as the standard library's member types are (value_type):
		// a member named Handle would redeclare the template it names.
		using handle = Handle<Resource>;

		// `kind` names the resource type in error messages: "Texture",
		// "SpriteFont". It is stored, not copied, so it must outlive the
		// registry - pass a literal.
		explicit Registry(const char* kind) : kind_(kind) {}

		// Fills the name's slot, claiming one if the name is new. Re-adding a name
		// reuses its slot rather than appending, which is what keeps handles valid
		// across a reload.
		// If storing a new entry throws, its name is not retained.
		void add(const std::string& name, Storage resource)
		{
			const std::optional<int> found = this->names_.find(name);
			if (found)
			{
				this->entries_[static_cast<size_t>(*found)].resource =
					std::move(resource);
				return;
			}

			this->names_.claim(name, static_cast<int>(this->entries_.size()),
				[this, &name, &resource]()
				{
					this->entries_.push_back(Entry{ std::move(resource), name });
				});
		}

		// The load-time half. Throws std::out_of_range if nothing ever added the
		// name: resolving a name no loader produced is a content bug, and this is
		// the line that should report it.
		//
		// Resolving is deliberately separate from reading, so a name can be turned
		// into a handle once and the handle kept for the object's lifetime.
		handle resolve(const std::string& name) const
		{
			const std::optional<int> found = this->names_.find(name);
			if (!found)
			{
				throw std::out_of_range(
					std::string(this->kind_) + " '" + name + "' was never loaded.");
			}
			return handle(*found);
		}

		// The per-frame half: a bounds check and an indexed load. Never returns
		// nullptr - an unresolved handle, or one whose slot has been released,
		// throws instead.
		Resource* get(handle resource_handle) const
		{
			const size_t index = static_cast<size_t>(resource_handle.index());
			if (!resource_handle.valid() || index >= this->entries_.size())
			{
				throw std::out_of_range(std::string(this->kind_) +
					" was read through an unresolved handle.");
			}

			const Entry& slot = this->entries_[index];
			Resource* resource = RegistryStorage<Storage>::pointer(slot.resource);
			if (resource == nullptr)
			{
				throw std::out_of_range(std::string(this->kind_) + " '" +
					slot.name + "' has been released.");
			}
			return resource;
		}

		Resource* get(const std::string& name) const
		{
			return this->get(this->resolve(name));
		}

		// True only if the name currently holds a resource. Missing names and
		// empty or released slots answer false. A released name keeps its slot
		// and can still resolve to its handle; get() throws until it is refilled.
		bool contains(const std::string& name) const
		{
			const std::optional<int> found = this->names_.find(name);
			return found &&
				RegistryStorage<Storage>::pointer(
					this->entries_[static_cast<size_t>(*found)].resource) !=
				nullptr;
		}

		// Empties every slot and keeps every name, so handles survive the round
		// trip through a device loss. Until the reload refills them, get() on any
		// of these throws saying the resource was released - which is the honest
		// answer and the one that names itself (T6).
		void release_all()
		{
			for (Entry& slot : this->entries_)
			{
				slot.resource = Storage{};
			}
		}

	private:
		// The name is kept beside the resource purely so a throw can say which
		// resource it was about. Nothing on the draw path reads it.
		struct Entry
		{
			Storage resource;
			std::string name;
		};

		const char* kind_ = nullptr;
		detail::NameIndex names_;
		std::vector<Entry> entries_;
	};
}
