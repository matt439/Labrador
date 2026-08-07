#pragma once

#include "engine/core/handle.h"
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// How a resource is reached through the smart pointer that owns it. The
// primary template covers everything spelling it get() - unique_ptr,
// shared_ptr - and COM's ComPtr spells it Get(), so the D3D-facing code
// specialises this where COM is already in scope rather than dragging
// <wrl/client.h> in here.
template <typename Storage>
struct RegistryStorage
{
	static auto pointer(const Storage& storage) { return storage.get(); }
};

// A store of named resources, and one lookup contract for every kind of
// resource: whichever way you ask, you get something live or a throw naming
// what was missing. It never returns nullptr and never inserts on a miss, so a
// misspelt name fails where it is written rather than null-dereferencing a
// frame later somewhere else.
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
// what a device loss does to textures and fonts - empties the slot but keeps
// the mapping, so a handle resolved before the loss still names the right
// resource once the reload refills it. That is the whole reason a handle is a
// slot index and not a pointer: the pointer changes across a device restore
// and the slot does not.
//
// The registry holds names, not meanings. What a name refers to is the
// caller's business - which is what lets a game keep its own resource types
// in one of these without the engine having to learn their vocabulary.
template <typename Resource, typename Storage = std::unique_ptr<Resource>>
class Registry
{
public:
	using handle = Handle<Resource>;

	// `kind` names the resource type in error messages: "Texture",
	// "SpriteFont". It is stored, not copied, so it must outlive the
	// registry - pass a literal.
	explicit Registry(const char* kind) : kind_(kind) {}

	// Fills the name's slot, claiming one if the name is new. Re-adding a name
	// reuses its slot rather than appending, which is what keeps handles valid
	// across a reload.
	void add(const std::string& name, Storage resource)
	{
		const auto it = this->indices_.find(name);
		if (it != this->indices_.end())
		{
			this->entries_[static_cast<size_t>(it->second)].resource =
				std::move(resource);
			return;
		}

		const int index = static_cast<int>(this->entries_.size());
		this->indices_.emplace(name, index);
		this->entries_.push_back(entry{ std::move(resource), name });
	}

	// The load-time half. Throws std::out_of_range if nothing ever added the
	// name: resolving a name no loader produced is a content bug, and this is
	// the line that should report it.
	//
	// Resolving is deliberately separate from reading, so a name can be turned
	// into a handle once and the handle kept for the object's lifetime.
	handle resolve(const std::string& name) const
	{
		const auto it = this->indices_.find(name);
		if (it == this->indices_.end())
		{
			throw std::out_of_range(
				std::string(this->kind_) + " '" + name + "' was never loaded.");
		}
		return handle(it->second);
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

		const entry& slot = this->entries_[index];
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

	// True only if the name is loaded now. A name whose slot has been released
	// reads as absent, because a released resource is indistinguishable from a
	// loaded one at every call site that only checks for presence.
	bool contains(const std::string& name) const
	{
		const auto it = this->indices_.find(name);
		return it != this->indices_.end() &&
			RegistryStorage<Storage>::pointer(
				this->entries_[static_cast<size_t>(it->second)].resource) !=
			nullptr;
	}

	// Empties every slot and keeps every name, so handles survive the round
	// trip through a device loss. Until the reload refills them, get() on any
	// of these throws saying the resource was released - which is the honest
	// answer and the one that names itself (T6).
	void release_all()
	{
		for (entry& slot : this->entries_)
		{
			slot.resource = Storage{};
		}
	}

private:
	// The name is kept beside the resource purely so a throw can say which
	// resource it was about. Nothing on the draw path reads it.
	struct entry
	{
		Storage resource;
		std::string name;
	};

	const char* kind_ = nullptr;
	std::map<std::string, int> indices_;
	std::vector<entry> entries_;
};
