#ifndef REGISTRY_H
#define REGISTRY_H

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// How a resource is reached through the handle that owns it. The primary
// template covers everything spelling it get() - unique_ptr, shared_ptr - and
// COM's ComPtr spells it Get(), so the D3D-facing code specialises this where
// COM is already in scope rather than dragging <wrl/client.h> in here.
template <typename Handle>
struct RegistryHandle
{
	static auto pointer(const Handle& handle) { return handle.get(); }
};

// A store of named resources, and one lookup contract for every kind of
// resource: get() either returns something live or throws saying what was
// missing. It never returns nullptr and never inserts on a miss, so a
// misspelt name fails where it is written rather than null-dereferencing a
// frame later somewhere else.
//
// The registry holds names, not meanings. What a name refers to is the
// caller's business - which is what lets a game keep its own resource types
// in one of these without the engine having to learn their vocabulary.
template <typename Resource, typename Handle = std::unique_ptr<Resource>>
class Registry
{
public:
	// `kind` names the resource type in error messages: "Texture",
	// "SpriteFont". It is stored, not copied, so it must outlive the
	// registry - pass a literal.
	explicit Registry(const char* kind) : _kind(kind) {}

	void add(const std::string& name, Handle resource)
	{
		this->_resources[name] = std::move(resource);
	}

	// Never returns nullptr: an absent or released resource throws
	// std::out_of_range naming both the kind and the name asked for.
	Resource* get(const std::string& name) const
	{
		const auto it = this->_resources.find(name);
		if (it == this->_resources.end())
		{
			throw std::out_of_range(
				std::string(this->_kind) + " '" + name + "' was never loaded.");
		}

		Resource* resource = RegistryHandle<Handle>::pointer(it->second);
		if (resource == nullptr)
		{
			throw std::out_of_range(
				std::string(this->_kind) + " '" + name + "' has been released.");
		}
		return resource;
	}

	bool contains(const std::string& name) const
	{
		return this->_resources.find(name) != this->_resources.end();
	}

	// Erase rather than null out: a key mapped to nullptr is indistinguishable
	// from a loaded resource at every call site that only checks for presence.
	void clear() { this->_resources.clear(); }

	auto begin() { return this->_resources.begin(); }
	auto end() { return this->_resources.end(); }

private:
	const char* _kind = nullptr;
	std::map<std::string, Handle> _resources;
};

#endif // !REGISTRY_H
