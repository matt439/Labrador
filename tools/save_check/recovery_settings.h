#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace save_example
{
	enum class LoadStatus { missing, current, migrated, recovery_required };
	enum class SaveStatus { prepared, saved, failed, refused };
	enum class RecoveryChoice { previous_valid, defaults };

	struct LoadReport
	{
		LoadStatus status = LoadStatus::missing;
		int volume = 80;
		bool can_save = true;
		bool retry_available = false;
		bool recovery_available = false;
		std::string message;
	};

	struct SaveReport
	{
		SaveStatus status;
		std::string message;
	};

	// Game-owned policy: one writer per directory, no concurrent instances/tabs.
	// Loading/migrating writes nothing. Unknown/duplicate fields, malformed or
	// unsupported data block replacement; recovery copies to a fresh directory.
	// Two versioned slots retain the last acknowledged valid generation while
	// the inactive one is staged/published/synced. No power-loss or universal
	// filesystem transaction guarantee is made.
	class SettingsStore
	{
	public:
		explicit SettingsStore(std::filesystem::path directory, int defaults = 80);
		SettingsStore(const SettingsStore&) = delete;
		SettingsStore& operator=(const SettingsStore&) = delete;
		SettingsStore(SettingsStore&&) = default;
		SettingsStore& operator=(SettingsStore&&) = default;

		// Borrowed until the next operation on this store.
		const LoadReport& loaded() const { return this->report_; }

		// Prepares only the inactive generation. Success is not an acknowledgement:
		// native callers complete(true) after this checked publication; browser
		// callers do so only after their asynchronous FS.syncfs reports success.
		SaveReport prepare(int volume);
		SaveReport retry();

		// A failure retains the pending attempt and last acknowledged generation.
		// Retry never promotes an unconfirmed file into the previous-copy role.
		SaveReport complete(bool persistence_succeeded);

		// Chooses validated previous settings or defaults in memory for a new,
		// non-existing directory. It writes nothing until prepare is explicit.
		// Every original, including unrecognised bytes, stays at the old path.
		SettingsStore recovery_copy(const std::filesystem::path& fresh_directory,
			RecoveryChoice choice) const;

	private:
		struct Record { int version; int generation; int volume; };
		using Snapshot = std::array<std::optional<std::string>, 4>;

		std::filesystem::path directory_;
		int defaults_;
		int active_ = -1;
		int generation_ = 0;
		bool blocked_ = false;
		bool published_ = false;
		int pending_target_ = -1;
		std::optional<Record> pending_;
		Snapshot expected_;
		LoadReport report_;

		std::filesystem::path path(int index) const;
		static Record decode(const std::string& bytes, const std::filesystem::path& path);
		static std::string encode(const Record& record);
		static std::optional<std::string> read(const std::filesystem::path& path);
		bool unchanged();
		SaveReport publish_pending();
		SaveReport refuse(std::string message);
	};
}
