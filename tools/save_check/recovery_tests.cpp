// Every path here is below a directory this process creates exclusively.
#include "tools/save_check/recovery_settings.h"
#include "tools/save_check/recovery_io.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <unistd.h>
#else
#include <Windows.h>
#endif

#if defined(__EMSCRIPTEN__)
// NODERAWFS runs on the host. libc++'s POSIX weakly_canonical cannot parse
// a Windows TMP value containing backslashes; Node resolves the owned test
// child to an absolute host path before any recursive cleanup instead.
EM_JS(int, save_fixture_owned_path, (const char* name, int remember), {
	try {
		const fs = require('node:fs');
		const paths = require('node:path');
		const child = UTF8ToString(name);
		if (remember) Module['saveFixtureParent'] = fs.realpathSync(process.cwd());
		const parent = Module['saveFixtureParent'];
		const resolved = fs.realpathSync(child);
		return !fs.lstatSync(child).isSymbolicLink() &&
			fs.realpathSync(process.cwd()) === parent &&
			paths.dirname(resolved) === parent &&
			paths.basename(resolved) === child && child.startsWith('labrador-save-recovery-') ? 1 : 0;
	} catch (_) { return 0; }
});
#endif

namespace
{
	int assertions = 0;

	void expect(bool condition, const char* message)
	{
		++assertions;
		if (!condition) throw std::runtime_error(message);
	}

	template <typename Action>
	void throws(Action action, const char* message)
	{
		bool caught = false;
		try { action(); }
		catch (const std::exception&) { caught = true; }
		expect(caught, message);
	}

	class OwnedDirectory
	{
	public:
		OwnedDirectory()
		{
#if defined(__EMSCRIPTEN__)
			this->parent_ = ".";
			const unsigned long process = static_cast<unsigned long>(::getpid());
#else
			this->parent_ = std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());
			const unsigned long process = GetCurrentProcessId();
#endif
			const std::string stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
			for (int attempt = 0; attempt < 100; ++attempt)
			{
				const std::filesystem::path candidate = this->parent_ /
					("labrador-save-recovery-" + std::to_string(process) + "-" + stamp + "-" + std::to_string(attempt));
				if (std::filesystem::create_directory(candidate))
				{
#if defined(__EMSCRIPTEN__)
					this->root_ = candidate.filename();
					if (!save_fixture_owned_path(this->root_.c_str(), 1)) throw std::runtime_error("Owned Node fixture path could not be verified.");
#else
					this->root_ = std::filesystem::weakly_canonical(candidate);
					if (this->root_.parent_path() != this->parent_) throw std::runtime_error("Owned test directory escaped its temporary parent.");
#endif
					return;
				}
			}
			throw std::runtime_error("Could not create a unique owned temporary test directory.");
		}
		~OwnedDirectory()
		{
			std::error_code error;
			// Verify the resolved absolute target before recursive removal. No
			// pre-existing directory is ever emptied or reused by this fixture.
#if defined(__EMSCRIPTEN__)
			if (!this->root_.empty() && save_fixture_owned_path(this->root_.c_str(), 0))
			{
				std::filesystem::remove_all(this->root_, error);
			}
#else
			const std::filesystem::path resolved = std::filesystem::weakly_canonical(this->root_, error);
			if (!error && !this->root_.empty() && resolved == this->root_ && resolved.parent_path() == this->parent_)
			{
				std::filesystem::remove_all(resolved, error);
			}
#endif
		}
		const std::filesystem::path& root() const { return this->root_; }
	private:
		std::filesystem::path parent_;
		std::filesystem::path root_;
	};

	void put(const std::filesystem::path& path, const std::string& bytes)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output.exceptions(std::ios::failbit | std::ios::badbit);
		output << bytes;
		output.close();
	}

	std::string get(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input) throw std::runtime_error("Cannot read test file.");
		return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
	}

	std::string record(int generation, int volume)
	{
		return "{\"version\":2,\"generation\":" + std::to_string(generation) + ",\"effects_volume\":" + std::to_string(volume) + "}\n";
	}

	void save(save_example::SettingsStore& store, int volume)
	{
		expect(store.prepare(volume).status == save_example::SaveStatus::prepared, "Save must prepare.");
		expect(store.complete(true).status == save_example::SaveStatus::saved, "Save must acknowledge explicitly.");
	}
}

int main()
{
	using namespace save_example;
	try
	{
		OwnedDirectory owned;
		const std::filesystem::path root = owned.root();
		{
			const std::filesystem::path directory = root / "missing";
			SettingsStore store(directory);
			expect(store.loaded().status == LoadStatus::missing && store.loaded().volume == 80, "Missing means in-memory defaults.");
			expect(!std::filesystem::exists(directory), "Loading must not create directories.");
			save(store, 37);
			expect(SettingsStore(directory).loaded().volume == 37, "Current data must reload.");
			const std::string prior = get(directory / "settings-a.json");
			expect(store.prepare(-1).status == SaveStatus::refused && store.prepare(101).status == SaveStatus::refused,
				"Out-of-range requests are refused without changing files.");
			expect(get(directory / "settings-a.json") == prior && !std::filesystem::exists(directory / "settings-pending.json"),
				"Refused values never stage a write.");
			expect(store.prepare(52).status == SaveStatus::prepared, "Inactive generation must stage.");
			expect(store.loaded().volume == 37, "Preparation is not a saved acknowledgement.");
			expect(store.complete(false).status == SaveStatus::failed, "Failed persistence is explicit.");
			expect(store.loaded().volume == 37 && store.loaded().retry_available, "Failure retains acknowledged state and retry.");
			expect(get(directory / "settings-a.json") == prior, "Failed flush must not touch acknowledged bytes.");
			expect(store.prepare(18).status == SaveStatus::refused, "A pending attempt must not rotate the previous-copy role.");
			expect(store.retry().status == SaveStatus::prepared, "Pending sync can retry without resnapshotting.");
			expect(store.complete(true).status == SaveStatus::saved, "Retry success can be acknowledged.");
			expect(store.loaded().volume == 52 && get(directory / "settings-a.json") == prior, "Retry preserves previous generation.");
		}
		{
			const std::filesystem::path directory = root / "migration";
			const std::string legacy = "{\"version\":1,\"volume\":26}\n";
			put(directory / "settings.json", legacy);
			SettingsStore store(directory);
			expect(store.loaded().status == LoadStatus::migrated && store.loaded().volume == 26, "Version1 must migrate in memory.");
			expect(get(directory / "settings.json") == legacy, "Migration must not rewrite the legacy save.");
			save(store, 26);
			expect(get(directory / "settings.json") == legacy, "Explicit version2 save must retain the legacy data too.");
		}
		int broken_case = 0;
		for (const std::string& broken : { std::string("{broken"), std::string("{\"version\":9,\"keep\":true}"),
			std::string("{\"version\":1,\"volume\":101}"), std::string("{\"version\":1,\"volume\":37,\"other\":1}"),
			std::string("{\"version\":1,\"volume\":37,\"volume\":38}"),
			std::string("{\"version\":2,\"generation\":0,\"effects_volume\":37}"),
			std::string("{\"version\":2,\"generation\":3,\"effects_volume\":true}") })
		{
			const std::filesystem::path directory = root / ("blocked-" + std::to_string(++broken_case));
			put(directory / "settings-a.json", record(2, 41));
			put(directory / "settings.json", broken);
			SettingsStore store(directory);
			expect(!store.loaded().can_save && store.loaded().recovery_available, "Unrecognised or failed migration data blocks writes but retains a valid recovery choice.");
			expect(store.prepare(72).status == SaveStatus::refused, "Normal save cannot overwrite unrecognised data.");
			expect(get(directory / "settings.json") == broken, "Malformed/future/extra/duplicate data stays byte-identical.");
			SettingsStore fresh = store.recovery_copy(directory / "recovered", RecoveryChoice::previous_valid);
			expect(fresh.loaded().volume == 41 && !std::filesystem::exists(directory / "recovered"), "Recovery starts in memory at a fresh path.");
			save(fresh, 41);
			SettingsStore defaults = fresh.recovery_copy(directory / "defaults", RecoveryChoice::defaults);
			expect(defaults.loaded().volume == 80, "Chained recovery preserves the game's defaults, not the prior recovery value.");
			expect(get(directory / "settings.json") == broken, "Explicit recovery preserves every original.");
			throws([&store, &directory]() { store.recovery_copy(directory / "recovered", RecoveryChoice::defaults); }, "Recovery never overwrites an existing directory.");
		}
		{
			const std::filesystem::path directory = root / "interrupted";
			put(directory / "settings-a.json", record(5, 31));
			put(directory / "settings-pending.json", record(6, 48));
			SettingsStore store(directory);
			expect(store.loaded().volume == 31 && store.loaded().retry_available && !store.loaded().can_save, "An interrupted draft requires explicit retry.");
			expect(store.retry().status == SaveStatus::prepared && store.complete(true).status == SaveStatus::saved, "Validated interrupted draft can resume.");
			expect(SettingsStore(directory).loaded().volume == 48, "Retried interrupted save reloads.");
			put(directory / "settings-pending.json", "{partial");
			SettingsStore partial(directory);
			expect(!partial.loaded().can_save && !partial.loaded().retry_available, "Partial drafts are not silently truncated.");
			expect(partial.retry().status == SaveStatus::refused && get(directory / "settings-pending.json") == "{partial", "Partial draft bytes remain for recovery.");
		}
		{
			const std::filesystem::path directory = root / "exclusive-write";
			std::filesystem::create_directory(directory);
			const std::filesystem::path draft = directory / "owned-draft.json";
			detail::write_new_file(draft, "original");
			throws([&draft]() { detail::write_new_file(draft, "replacement"); }, "Real exclusive-open collision must fail.");
			expect(get(draft) == "original", "Failed exclusive open preserves existing bytes.");
			std::filesystem::remove(draft);
			detail::write_new_file(draft, "retried");
			expect(get(draft) == "retried", "Explicit retry after resolving the collision works.");
		}
#if !defined(__EMSCRIPTEN__)
		{
			const std::filesystem::path directory = root / "locked-interrupted-draft";
			put(directory / "settings-a.json", record(5, 31));
			put(directory / "settings-pending.json", record(6, 48));
			SettingsStore store(directory);
			const HANDLE locked = CreateFileW((directory / "settings-pending.json").c_str(), GENERIC_READ,
				FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			expect(locked != INVALID_HANDLE_VALUE, "Owned interrupted draft must lock.");
			const SaveReport failed = store.retry();
			CloseHandle(locked);
			expect(failed.status == SaveStatus::failed, "Cannot acknowledge an interrupted draft whose flush open failed.");
			expect(get(directory / "settings-a.json") == record(5, 31), "Failed draft flush leaves the acknowledged generation intact.");
			expect(store.retry().status == SaveStatus::prepared && store.complete(true).status == SaveStatus::saved,
				"Interrupted draft retries after the lock is released.");
		}
		{
			const std::filesystem::path directory = root / "locked-replacement";
			put(directory / "settings-a.json", record(5, 37));
			put(directory / "settings-b.json", record(4, 28));
			SettingsStore store(directory);
			const HANDLE locked = CreateFileW((directory / "settings-b.json").c_str(), GENERIC_READ,
				FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			expect(locked != INVALID_HANDLE_VALUE, "Owned inactive slot must lock for the real replacement failure.");
			const SaveReport failed = store.prepare(64);
			CloseHandle(locked);
			expect(failed.status == SaveStatus::failed, "Windows replacement denied by sharing must fail explicitly.");
			expect(get(directory / "settings-a.json") == record(5, 37), "Failed replacement preserves last valid generation.");
			expect(store.retry().status == SaveStatus::prepared && store.complete(true).status == SaveStatus::saved, "Replacement can retry after the lock is released.");
			expect(SettingsStore(directory).loaded().volume == 64, "Successful replacement retry reloads.");
		}
#endif
		{
			const std::filesystem::path directory = root / "ambiguous-generation";
			put(directory / "settings-a.json", record(9, 11));
			put(directory / "settings-b.json", record(9, 22));
			SettingsStore store(directory);
			expect(!store.loaded().can_save && !store.loaded().recovery_available, "Conflicting highest generations cannot choose an arbitrary recovery value.");
			throws([&store, &directory]() { store.recovery_copy(directory / "previous", RecoveryChoice::previous_valid); },
				"Ambiguous previous values cannot be copied as validated recovery.");
			expect(store.recovery_copy(directory / "defaults", RecoveryChoice::defaults).loaded().volume == 80,
				"Defaults remain an explicit fresh recovery option.");
		}
		{
			const std::filesystem::path directory = root / "exhausted-generation";
			put(directory / "settings-a.json", record(std::numeric_limits<int>::max(), 29));
			SettingsStore store(directory);
			expect(store.prepare(30).status == SaveStatus::refused && !store.loaded().can_save,
				"Generation exhaustion is a refusal, never signed overflow.");
			expect(get(directory / "settings-a.json") == record(std::numeric_limits<int>::max(), 29),
				"Generation exhaustion preserves the prior bytes.");
		}
		{
			const std::filesystem::path directory = root / "changed-externally";
			put(directory / "settings-a.json", record(1, 12));
			SettingsStore store(directory);
			const std::string changed = record(2, 83);
			put(directory / "settings-a.json", changed);
			expect(store.prepare(24).status == SaveStatus::refused, "Observed external changes require reload instead of overwrite.");
			expect(get(directory / "settings-a.json") == changed, "Refusal preserves external bytes.");
		}
		std::printf("PASS save recovery: %d assertions; owned temporary paths only\n", assertions);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "save recovery fixture failed: %s\n", error.what());
		return 1;
	}
}
