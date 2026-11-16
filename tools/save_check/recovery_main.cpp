// Tool page around the copyable game-owned example. Its storage is mounted
// before main; the page runner always uses a disposable browser profile.
#include "tools/save_check/recovery_settings.h"

#include <emscripten.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

EM_JS(int, selected_save_profile, (), {
	return Module['saveProfile'] === 'recovered' ? 1 : 0;
});

EM_JS(void, report_settings_state, (const char* status, int volume, int can_save,
	int retry, int recovery, const char* message, const char* profile), {
	Module['settingsState'] = {
		status: UTF8ToString(status), volume, canSave: !!can_save,
		retryAvailable: !!retry, recoveryAvailable: !!recovery,
		message: UTF8ToString(message), profile: UTF8ToString(profile)
	};
	if (Module['renderSettings']) Module['renderSettings']();
});

EM_JS(void, report_save_operation, (const char* status, const char* message), {
	Module['saveOperation'] = { status: UTF8ToString(status), message: UTF8ToString(message) };
});

EM_JS(void, report_settings_ready, (), {
	Module['settingsReady'] = true;
	if (Module['renderSettings']) Module['renderSettings']();
});
EM_JS(void, report_settings_failure, (const char* message), {
	if (Module['onError']) Module['onError'](UTF8ToString(message));
	else abort(UTF8ToString(message));
});

namespace
{
	std::unique_ptr<save_example::SettingsStore> store;
	std::string profile = "main";

	const char* status_name(save_example::LoadStatus status)
	{
		switch (status)
		{
		case save_example::LoadStatus::missing: return "missing";
		case save_example::LoadStatus::current: return "current";
		case save_example::LoadStatus::migrated: return "migrated";
		case save_example::LoadStatus::recovery_required: return "recovery_required";
		}
		return "recovery_required";
	}

	void state()
	{
		const save_example::LoadReport& loaded = store->loaded();
		report_settings_state(status_name(loaded.status), loaded.volume,
			loaded.can_save, loaded.retry_available, loaded.recovery_available,
			loaded.message.c_str(), profile.c_str());
	}

	int operation(const save_example::SaveReport& result)
	{
		const char* name = "refused";
		switch (result.status)
		{
		case save_example::SaveStatus::prepared: name = "prepared"; break;
		case save_example::SaveStatus::saved: name = "saved"; break;
		case save_example::SaveStatus::failed: name = "failed"; break;
		case save_example::SaveStatus::refused: break;
		}
		report_save_operation(name, result.message.c_str());
		state();
		return result.status == save_example::SaveStatus::prepared ||
			result.status == save_example::SaveStatus::saved ? 0 : 1;
	}

	template <typename Action>
	int guarded(Action action)
	{
		if (!store)
		{
			report_save_operation("refused", "Settings have not finished loading.");
			return 1;
		}
		try { return operation(action()); }
		catch (const std::exception& error)
		{
			report_save_operation("refused", error.what());
			if (store) state();
			return 1;
		}
	}
}

extern "C" EMSCRIPTEN_KEEPALIVE int prepare_volume(int volume)
{
	return guarded([volume]() { return store->prepare(volume); });
}

extern "C" EMSCRIPTEN_KEEPALIVE int retry_save()
{
	return guarded([]() { return store->retry(); });
}

extern "C" EMSCRIPTEN_KEEPALIVE int complete_save(int success)
{
	return guarded([success]() { return store->complete(success != 0); });
}

extern "C" EMSCRIPTEN_KEEPALIVE int recover_settings(int previous)
{
	return guarded([previous]()
		{
			std::unique_ptr<save_example::SettingsStore> fresh = std::make_unique<save_example::SettingsStore>(
				store->recovery_copy("/save/recovered", previous ? save_example::RecoveryChoice::previous_valid :
					save_example::RecoveryChoice::defaults));
			store = std::move(fresh);
			profile = "recovered";
			return store->prepare(store->loaded().volume);
		});
}

int main()
{
	try
	{
		profile = selected_save_profile() ? "recovered" : "main";
		store = std::make_unique<save_example::SettingsStore>(
			profile == "recovered" ? "/save/recovered" : "/save");
		state();
		report_settings_ready();
	}
	catch (const std::exception& error)
	{
		report_settings_failure(error.what());
	}
}
