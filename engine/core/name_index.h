#pragma once

#include <map>
#include <optional>
#include <string>

namespace labrador::detail
{
	// The name-to-slot map Registry and NameTable share. Each keeps its own
	// element storage and its own error words; this keeps the names, and the
	// one rule that a name whose element could not be stored is not kept.
	class NameIndex
	{
	public:
		// The slot `name` holds, or nothing for a name never claimed.
		std::optional<int> find(const std::string& name) const
		{
			const std::map<std::string, int>::const_iterator it =
				this->indices_.find(name);
			if (it == this->indices_.end())
			{
				return std::nullopt;
			}
			return it->second;
		}

		// Gives a new `name` slot `index`, then calls `store`, which stores
		// that slot's element. If `store` throws, the name is not retained,
		// so a failed add leaves the table as it was.
		template <typename Store>
		void claim(const std::string& name, int index, Store store)
		{
			const std::map<std::string, int>::iterator inserted =
				this->indices_.emplace(name, index).first;
			try
			{
				store();
			}
			catch (...)
			{
				this->indices_.erase(inserted);
				throw;
			}
		}

	private:
		std::map<std::string, int> indices_;
	};
}
