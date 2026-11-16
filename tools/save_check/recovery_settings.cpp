#include "tools/save_check/recovery_settings.h"

#include "tools/save_check/recovery_io.h"
#include "engine/assets/json.h"
#include "engine/core/file_path.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace save_example
{
	std::filesystem::path SettingsStore::path(int index) const
	{
		static constexpr const char* names[] = {
			"settings.json", "settings-a.json", "settings-b.json", "settings-pending.json"
		};
		return this->directory_ / names[index];
	}

	std::optional<std::string> SettingsStore::read(const std::filesystem::path& path)
	{
		std::error_code error;
		const std::filesystem::file_status status = std::filesystem::symlink_status(path, error);
		if (status.type() == std::filesystem::file_type::not_found) return std::nullopt;
		if (error || !std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status))
		{
			throw std::runtime_error(labrador::path_to_utf8(path) + ": not a readable regular settings file.");
		}
		const std::uintmax_t size = std::filesystem::file_size(path);
		if (size > 4096) throw std::runtime_error(labrador::path_to_utf8(path) + ": settings exceed 4096 bytes.");
		std::ifstream input(path, std::ios::binary);
		input.exceptions(std::ios::failbit | std::ios::badbit);
		std::string bytes(static_cast<std::size_t>(size), '\0');
		if (!bytes.empty()) input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		if (input.peek() != std::char_traits<char>::eof())
		{
			throw std::runtime_error(labrador::path_to_utf8(path) + ": settings changed while reading.");
		}
		input.close();
		return bytes;
	}

	SettingsStore::Record SettingsStore::decode(const std::string& bytes,
		const std::filesystem::path& path)
	{
		const labrador::JsonDocument document = labrador::parse_json(bytes, labrador::path_to_utf8(path));
		const labrador::JsonValue root = document.root();
		const int version = root.integer("version");
		if (version != 1 && version != 2)
		{
			throw std::runtime_error(root.where() + ": unsupported settings version " + std::to_string(version) + ".");
		}
		std::set<std::string> seen;
		for (const std::string& name : root.keys())
		{
			const bool allowed = name == "version" || (version == 1 ? name == "volume" :
				(name == "generation" || name == "effects_volume"));
			if (!allowed || !seen.insert(name).second)
			{
				throw std::runtime_error(root.where() + ": unknown or duplicate setting '" + name + "'; file is preserved.");
			}
		}
		const int volume = root.integer(version == 1 ? "volume" : "effects_volume");
		if (volume < 0 || volume > 100)
		{
			throw std::runtime_error(root.where() + ": volume/migration input must be from 0 to 100.");
		}
		const int generation = version == 1 ? 0 : root.integer("generation");
		if (version == 2 && generation < 1)
		{
			throw std::runtime_error(root.where() + ": generation must be positive.");
		}
		// The only migration: version1's volume becomes version2's
		// effects_volume. It changes memory only, after complete validation.
		return { version, generation, volume };
	}

	std::string SettingsStore::encode(const Record& record)
	{
		// This fixed numeric schema needs no escaping or general JSON writer.
		return "{\"version\":2,\"generation\":" + std::to_string(record.generation) +
			",\"effects_volume\":" + std::to_string(record.volume) + "}\n";
	}

	SettingsStore::SettingsStore(std::filesystem::path directory, int defaults) :
		directory_(std::move(directory)), defaults_(defaults)
	{
		if (defaults < 0 || defaults > 100) throw std::invalid_argument("Default volume must be from 0 to 100.");
		this->report_.volume = defaults;
		std::optional<Record> best;
		bool ambiguous = false;
		std::string problems;
		for (int index = 0; index < 4; ++index)
		{
			try
			{
				this->expected_[index] = read(this->path(index));
				if (!this->expected_[index] || index == 3) continue;
				const Record record = decode(*this->expected_[index], this->path(index));
				if (index != 0 && record.version != 2) throw std::runtime_error("Generation slots require version2.");
				if (!best || record.generation > best->generation)
				{
					best = record;
					this->active_ = index;
					ambiguous = false;
				}
				else if (record.generation == best->generation && record.volume != best->volume)
				{
					ambiguous = true;
				}
			}
			catch (const std::exception& error)
			{
				this->blocked_ = true;
				problems += labrador::path_to_utf8(this->path(index)) + ": " + error.what() + "\n";
			}
		}
		if (best && !ambiguous)
		{
			this->generation_ = best->generation;
			this->report_.volume = best->volume;
			this->report_.recovery_available = true;
			this->report_.status = best->version == 1 ? LoadStatus::migrated : LoadStatus::current;
		}
		if (ambiguous)
		{
			this->blocked_ = true;
			this->report_.recovery_available = false;
			problems += "Conflicting settings have the same generation; choose defaults or inspect/export the originals.\n";
		}
		if (this->expected_[3] && !this->blocked_)
		{
			try
			{
				const Record draft = decode(*this->expected_[3], this->path(3));
				if (draft.version != 2 || this->generation_ == std::numeric_limits<int>::max() ||
					draft.generation != this->generation_ + 1)
				{
					throw std::runtime_error("The interrupted draft does not follow the loaded generation.");
				}
				this->pending_ = draft;
				this->pending_target_ = this->active_ == 1 ? 2 : 1;
				this->report_.retry_available = true;
				problems += "An interrupted validated draft is available. Retry it explicitly or copy recovery settings to a fresh directory.\n";
			}
			catch (const std::exception& error)
			{
				this->blocked_ = true;
				problems += error.what();
			}
		}
		this->report_.can_save = !this->blocked_ && !this->pending_;
		if (this->blocked_ || this->pending_) this->report_.status = LoadStatus::recovery_required;
		this->report_.message = problems.empty() ? (best ? "Validated settings loaded; no file was rewritten." :
			"No settings file exists; defaults are in memory only.") : problems;
	}

	SaveReport SettingsStore::refuse(std::string message)
	{
		this->report_.message = message;
		return { SaveStatus::refused, std::move(message) };
	}

	bool SettingsStore::unchanged()
	{
		try
		{
			for (int index = 0; index < 4; ++index)
			{
				if (read(this->path(index)) != this->expected_[index])
				{
					throw std::runtime_error("Settings changed outside this session; reload before retry/recovery.");
				}
			}
			return true;
		}
		catch (const std::exception& error)
		{
			this->blocked_ = true;
			this->report_.status = LoadStatus::recovery_required;
			this->report_.can_save = false;
			this->report_.retry_available = false;
			this->report_.message = error.what();
			return false;
		}
	}

	SaveReport SettingsStore::prepare(int volume)
	{
		if (volume < 0 || volume > 100) return this->refuse("Volume must be from 0 to 100.");
		if (this->blocked_ || !this->report_.can_save) return this->refuse("Ordinary saving is blocked. Retry a pending attempt or choose a fresh recovery directory.");
		if (!this->unchanged()) return this->refuse(this->report_.message);
		if (this->generation_ == std::numeric_limits<int>::max())
		{
			this->report_.can_save = false;
			this->report_.status = LoadStatus::recovery_required;
			return this->refuse("Generation exhausted; choose a fresh recovery directory.");
		}
		const Record candidate{ 2, this->generation_ + 1, volume };
		try
		{
			std::filesystem::create_directories(this->directory_);
			detail::write_new_file(this->path(3), encode(candidate));
			this->expected_[3] = read(this->path(3));
			this->pending_ = candidate;
			this->pending_target_ = this->active_ == 1 ? 2 : 1;
			this->report_.can_save = false;
			this->report_.retry_available = true;
			return this->publish_pending();
		}
		catch (const std::exception& error)
		{
			// A partial/unknown draft is preserved, not truncated on retry.
			this->blocked_ = true;
			this->report_.can_save = false;
			this->report_.retry_available = false;
			this->report_.status = LoadStatus::recovery_required;
			this->report_.message = error.what();
			return { SaveStatus::failed, this->report_.message };
		}
	}

	SaveReport SettingsStore::publish_pending()
	{
		if (this->published_) return { SaveStatus::prepared, "Candidate is waiting for persistence acknowledgement." };
		try
		{
			detail::publish_file(this->path(3), this->path(this->pending_target_));
			this->expected_[this->pending_target_] = read(this->path(this->pending_target_));
			this->expected_[3] = read(this->path(3));
			if (this->expected_[this->pending_target_] != encode(*this->pending_)) throw std::runtime_error("Published candidate did not read back exactly.");
			this->published_ = true;
			return { SaveStatus::prepared, "Candidate published in the inactive slot; not yet acknowledged saved." };
		}
		catch (const std::exception& error)
		{
			const std::string message = error.what();
			try
			{
				// ReplaceFileW failure can alter the inactive pathname. Rescan
				// it as inactive only; never promote it to the previous-copy role.
				this->expected_[this->pending_target_] = read(this->path(this->pending_target_));
				this->expected_[3] = read(this->path(3));
				if (this->expected_[this->pending_target_])
				{
					const Record inactive = decode(*this->expected_[this->pending_target_], this->path(this->pending_target_));
					if (inactive.version != 2) throw std::runtime_error("Inactive data is unrecognised.");
				}
				this->published_ = !this->expected_[3] && this->expected_[this->pending_target_] == encode(*this->pending_);
			}
			catch (const std::exception&)
			{
				this->blocked_ = true;
				this->report_.retry_available = false;
			}
			this->report_.status = LoadStatus::recovery_required;
			this->report_.message = message + " Any prior acknowledged generation was not replaced. Retry explicitly; path rollback is not promised.";
			return { SaveStatus::failed, this->report_.message };
		}
	}

	SaveReport SettingsStore::retry()
	{
		if (this->blocked_ || !this->pending_) return this->refuse("No validated pending attempt is available; reload or choose fresh recovery.");
		if (!this->unchanged()) return this->refuse(this->report_.message);
		return this->publish_pending();
	}

	SaveReport SettingsStore::complete(bool persistence_succeeded)
	{
		if (!this->pending_ || !this->published_) return this->refuse("There is no published pending save to acknowledge.");
		if (!persistence_succeeded)
		{
			this->report_.status = LoadStatus::recovery_required;
			this->report_.message = this->report_.recovery_available
				? "Persistence failed. The prior valid generation is retained; retry the pending attempt. New bytes may already have reached storage."
				: "Persistence failed. No prior valid file exists in this directory; retry the pending attempt. New bytes may already have reached storage.";
			return { SaveStatus::failed, this->report_.message };
		}
		if (!this->unchanged()) return this->refuse(this->report_.message);
		const bool had_previous = this->report_.recovery_available;
		this->active_ = this->pending_target_;
		this->generation_ = this->pending_->generation;
		this->report_.volume = this->pending_->volume;
		this->report_.status = LoadStatus::current;
		this->report_.can_save = true;
		this->report_.retry_available = false;
		this->report_.recovery_available = true;
		this->report_.message = had_previous ? "Save acknowledged; the prior generation is still available." :
			"First save acknowledged; no earlier valid file existed in this directory.";
		this->pending_.reset();
		this->pending_target_ = -1;
		this->published_ = false;
		return { SaveStatus::saved, this->report_.message };
	}

	SettingsStore SettingsStore::recovery_copy(const std::filesystem::path& fresh_directory,
		RecoveryChoice choice) const
	{
		if (std::filesystem::exists(fresh_directory) ||
			std::filesystem::absolute(fresh_directory).lexically_normal() == std::filesystem::absolute(this->directory_).lexically_normal())
		{
			throw std::invalid_argument("Recovery needs a different, non-existing directory; originals are never removed.");
		}
		if (choice == RecoveryChoice::previous_valid && !this->report_.recovery_available)
		{
			throw std::invalid_argument("No unambiguous validated previous settings are available.");
		}
		SettingsStore result(fresh_directory, this->defaults_);
		result.report_.volume = choice == RecoveryChoice::previous_valid ? this->report_.volume : this->defaults_;
		result.report_.message = "Recovery values are in memory only; explicitly save the fresh copy. Originals remain untouched.";
		return result;
	}
}
