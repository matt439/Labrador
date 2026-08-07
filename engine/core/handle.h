#pragma once

namespace artattack
{
	// A name that has already been resolved.
	//
	// `Resource` is never stored - it is a phantom, there so that a handle to a
	// sprite frame cannot be passed where a handle to an animation strip belongs.
	// Both are otherwise an int, and an int silently means whatever the callee
	// thinks it means.
	//
	// The index is into whichever table produced the handle and is meaningless
	// against any other, so a handle travels with the table it came from. Tables
	// resolve names into these once, at load; per-frame code only carries them
	// (PHILOSOPHY T7, T8).
	template <typename Resource>
	class Handle
	{
	public:
		// Unresolved. Reading through one is a throw, not a read of slot zero -
		// the default has to be inert, because every draw object is
		// default-constructible and would otherwise start life pointing at
		// whatever happened to load first.
		Handle() = default;

		// Explicit, and only meaningful to the table that owns the slot. Callers
		// get handles from resolve(), not from arithmetic.
		explicit Handle(int index) : index_(index) {}

		bool valid() const { return this->index_ >= 0; }
		int index() const { return this->index_; }

		friend bool operator==(Handle a, Handle b) { return a.index_ == b.index_; }
		friend bool operator!=(Handle a, Handle b) { return a.index_ != b.index_; }

	private:
		int index_ = -1;
	};
}
