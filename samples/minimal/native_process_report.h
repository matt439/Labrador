#pragma once

#include "engine/app/application.h"
#include "tools/native_process_observation.h"

#include <cstdio>
#include <exception>
#include <string>

namespace minimal_observation
{
	// Optional maintainer instrumentation. Declared after the Application so
	// its endpoint precedes destruction of states, resources and the device.
	class ProcessReport
	{
	public:
		ProcessReport(labrador::Application* app, bool enabled,
			const char* source_revision) :
			app_(app), enabled_(enabled), source_revision_(source_revision)
		{
		}

		~ProcessReport()
		{
			if (!this->enabled_)
			{
				return;
			}
			try
			{
				const bool failed = std::uncaught_exceptions() != 0;
				const native_process_observation::Observation observation =
					native_process_observation::capture(
						this->app_->frame_timings().first_completed_frame_utc,
						failed ? "exception_unwind_before_application_destruction_and_report_assembly" :
						"after_application_run_before_application_destruction_and_report_assembly");
				std::string backend = "null";
				std::string device_name = "null";
				std::string device_api = "null";
				if (this->app_->frame_timings().frame != 0)
				{
					const labrador::RenderDeviceInfo& device = this->app_->renderer()->device_info();
					backend = native_process_observation::json_string(device.backend);
					device_name = native_process_observation::json_string(device.device_name);
					device_api = native_process_observation::json_string(device.api);
				}
				std::printf("{\"scope\":\"minimal_smoke_process_observation\","
					"\"status\":%s,\"source_revision\":%s,\"configuration\":%s,"
					"\"completed_frames\":%llu,"
					"\"render_backend\":%s,\"device_name\":%s,\"device_api\":%s,"
					"\"process_observations\":%s}\n",
					failed ? "\"startup_or_run_failed\"" : "\"run_returned\"",
					native_process_observation::json_string(this->source_revision_).c_str(),
#ifdef NDEBUG
					"\"release\"",
#else
					"\"debug\"",
#endif
					static_cast<unsigned long long>(this->app_->frame_timings().frame),
					backend.c_str(), device_name.c_str(), device_api.c_str(),
					native_process_observation::json(observation).c_str());
			}
			catch (...)
			{
				std::fprintf(stderr, "Native process observation unavailable: report assembly failed.\n");
			}
		}

		ProcessReport(const ProcessReport&) = delete;
		ProcessReport& operator=(const ProcessReport&) = delete;

	private:
		labrador::Application* app_;
		bool enabled_;
		const char* source_revision_;
	};
}
