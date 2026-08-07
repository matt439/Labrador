#pragma once

#include "engine/core/handle.h"
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Named elements stored by value, resolved to handles once and then read by
// index. The same names-in, handles-out bargain as Registry, for the case
// where the elements are part of a larger asset rather than assets in their
// own right: a sprite sheet's frames and animation strips.
//
// Registry is the one to reach for when the entries have independent
// lifetimes - when something can be released and reloaded underneath a live
// handle. A NameTable is filled once by a loader and is const thereafter, so
// it stores elements directly: one allocation for the lot, contiguous, and no
// indirection on the read.
//
// Because it never grows after loading, references into it are stable, which
// is what lets a caller hold a `const Element&` (or a pointer into one, as
// SpriteFrame's RECT is) across a frame.
template <typename Element>
class NameTable
{
public:
	// `kind` names the element type in error messages: "sprite frame",
	// "animation strip". Stored, not copied - pass a literal.
	explicit NameTable(const char* kind) : _kind(kind) {}

	// Load-time only. A repeated name replaces the earlier element and keeps
	// its index, so no handle resolved from this table is ever left dangling.
	void add(const std::string& name, Element element)
	{
		const auto it = this->_indices.find(name);
		if (it != this->_indices.end())
		{
			this->_elements[static_cast<size_t>(it->second)] =
				std::move(element);
			return;
		}

		this->_indices.emplace(name, static_cast<int>(this->_elements.size()));
		this->_elements.push_back(std::move(element));
	}

	// Throws std::out_of_range naming the element and the kind if the table
	// has no such name - a sheet asked for a frame it does not contain is a
	// content bug, and it should say so at load rather than draw nothing.
	Handle<Element> resolve(const std::string& name) const
	{
		const auto it = this->_indices.find(name);
		if (it == this->_indices.end())
		{
			throw std::out_of_range(std::string("no ") + this->_kind +
				" named '" + name + "'");
		}
		return Handle<Element>(it->second);
	}

	// The per-frame read: a bounds check and an index. Throws rather than
	// return a reference to nothing.
	const Element& get(Handle<Element> element_handle) const
	{
		const size_t index = static_cast<size_t>(element_handle.index());
		if (!element_handle.valid() || index >= this->_elements.size())
		{
			throw std::out_of_range(std::string("unresolved ") + this->_kind +
				" handle");
		}
		return this->_elements[index];
	}

	bool contains(const std::string& name) const
	{
		return this->_indices.find(name) != this->_indices.end();
	}

private:
	const char* _kind = nullptr;
	std::map<std::string, int> _indices;
	std::vector<Element> _elements;
};
