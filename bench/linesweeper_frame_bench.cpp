#include "bench/linesweeper_frame_result.h"

#include "engine/app/content_root.h"
#include "engine/app/window.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/label.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/board_view.h"
#include "samples/linesweeper/presentation/particles.h"
#include "samples/linesweeper/presentation/top_out_banner.h"
#include "samples/linesweeper/rules/world.h"

#include <DirectXMath.h>
#include <Windows.h>
#include <objbase.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

using namespace labrador;
using namespace linesweeper;
using namespace mattmath;

namespace
{
	using Clock = std::chrono::steady_clock;

	constexpr int frame_width = linesweeper_frame_bench::width;
	constexpr int frame_height = linesweeper_frame_bench::height;
	constexpr int particle_count =
		linesweeper_frame_bench::expected_particles;
	using linesweeper_frame_bench::FrameSample;
	using linesweeper_frame_bench::Options;
	using linesweeper_frame_bench::Result;

	const char* const font_name = "courier_new_bold_16";
	const char* const block_texture_name = "white";

	std::int64_t nanoseconds(Clock::duration duration)
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
	}

	std::string utc_now()
	{
		const std::time_t now = std::chrono::system_clock::to_time_t(
			std::chrono::system_clock::now());
		std::tm utc = {};
		if (gmtime_s(&utc, &now) != 0)
		{
			throw std::runtime_error("gmtime_s failed.");
		}

		char text[32] = {};
		if (std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0)
		{
			throw std::runtime_error("Could not format the UTC timestamp.");
		}
		return text;
	}

	bool release_build()
	{
#ifdef NDEBUG
		return true;
#else
		return false;
#endif
	}

	class FrameBenchmark final : public WindowNotify, public DeviceNotify
	{
	public:
		explicit FrameBenchmark(const Options& options) :
			options_(options),
			scene_(nullptr, nullptr)
		{
			this->samples_.reserve(static_cast<std::size_t>(options.sample));

			WindowOptions window_options;
			window_options.window_class_name = L"LineSweeperFrameBenchWindowClass";
			window_options.window_title = L"LineSweeper frame benchmark";
			window_options.client_size = Vector2I(frame_width, frame_height);

			this->window_ = std::make_unique<Window>(GetModuleHandleW(nullptr),
				SW_SHOWNORMAL, window_options, this);
			if (this->width_ != frame_width || this->height_ != frame_height)
			{
				throw std::runtime_error(
					"The benchmark window did not create at exactly 1280x720 client pixels.");
			}

			this->renderer_.set_device_notify(this);
			this->renderer_.create_device(this->window_->handle(), frame_width,
				frame_height, 1);
			this->device_created_ = true;
			this->renderer_.set_resources(&this->resources_);
			this->validate_device();
			this->load_content();
			this->build_workload();
		}

		void pump()
		{
			this->started_utc_ = utc_now();
			this->running_ = true;
			this->next_frame_ = Clock::now();
			this->window_->pump_until_quit();
			this->running_ = false;

			if (this->failure_)
			{
				std::rethrow_exception(this->failure_);
			}
			if (static_cast<int>(this->samples_.size()) != this->options_.sample)
			{
				throw std::runtime_error(
					"The window closed before the declared sample completed.");
			}
			this->finished_utc_ = utc_now();
		}

		Result take_result()
		{
			Result result;
			result.samples = std::move(this->samples_);
			result.device = this->renderer_.device_info();
			result.measurement_class = std::move(this->measurement_class_);
			result.started_utc = std::move(this->started_utc_);
			result.finished_utc = std::move(this->finished_utc_);
			return result;
		}

	private:
		void validate_device()
		{
			const RenderDeviceInfo& info = this->renderer_.device_info();
			if (info.backend == "null")
			{
				if (info.kind != RenderDeviceKind::null_device)
				{
					throw std::runtime_error(
						"The null backend reported a non-null render device.");
				}
				this->measurement_class_ = "null_backend";
				return;
			}

			if (info.kind != RenderDeviceKind::hardware)
			{
				throw std::runtime_error(
					"A raster benchmark requires a hardware render device; software fallback was refused.");
			}
			this->measurement_class_ = "hardware_raster";
		}

		void load_content()
		{
			const std::string root = executable_directory();
			load_font_asset(this->renderer_, this->resources_,
				resolved_under(root, "./fonts/"), font_name);
			load_texture_asset(this->renderer_, this->resources_,
				resolved_under(root, "./textures/"), block_texture_name);
		}

		void build_workload()
		{
			this->world_.tick = 1000;
			this->world_.score = 999999;
			this->world_.lines = 199;
			this->world_.level = 14;
			this->world_.hold_kind = static_cast<std::uint8_t>(Kind::o);
			this->world_.hold_available = 1;

			for (int index = 0; index < 5; ++index)
			{
				this->world_.queue[static_cast<std::size_t>(index)] =
					static_cast<std::uint8_t>(index % kind_count + 1);
			}
			this->world_.queue_count = 5;

			for (int y = well_buffer_rows; y < well_rows; ++y)
			{
				for (int x = 0; x < well_columns; ++x)
				{
					this->world_.cells[static_cast<std::size_t>(cell_index(x, y))] =
						static_cast<std::uint8_t>((x + y) % kind_count + 1);
				}
			}

			std::ignore = this->scene_.add(
				std::make_unique<BoardView>(&this->world_, &this->resources_));
			this->particles_ = this->scene_.add(std::make_unique<ParticleField>(
				&this->world_, &this->last_tick_,
				this->resources_.resolve_texture(block_texture_name)));
			std::ignore = this->scene_.add(
				std::make_unique<TopOutBanner>(&this->world_, &this->resources_));

			std::ignore = this->scene_.add(std::make_unique<Label>(
				L"keyboard   arrows move   Z X rotate   space drops   C holds",
				font_name, Vector2F(344.0f,
					static_cast<float>(frame_height) - 60.0f),
				&this->resources_, Colour::dark_gray));
			std::ignore = this->scene_.add(std::make_unique<Label>(
				L"pad        d-pad moves   A B rotate   Y drops       X holds",
				font_name, Vector2F(344.0f,
					static_cast<float>(frame_height) - 36.0f),
				&this->resources_, Colour::dark_gray));

			this->scene_.end_tick();
			this->scene_.add_view(Viewport(RectangleF(Vector2F::ZERO,
				Vector2F(static_cast<float>(frame_width),
					static_cast<float>(frame_height)))));

			// Freeze the exact expensive frame: all 200 visible cells top out at
			// once, producing 48 particles each. One zero-time update materialises
			// the effect and every measured frame is then a pure read of it.
			this->world_.topped_out = 1;
			++this->world_.tick;
			this->scene_.update(0.0f);
			this->scene_.end_tick();

			if (this->particles_->live() != particle_count ||
				this->particles_->dropped() != 0)
			{
				throw std::runtime_error(
					"The frozen LineSweeper workload is not exactly 9,600 live particles.");
			}
		}

		void set_failure(const char* message)
		{
			if (!this->failure_)
			{
				this->failure_ = std::make_exception_ptr(std::runtime_error(message));
			}
		}

		void tick() override
		{
			try
			{
				if (this->failure_)
				{
					this->window_->close();
					return;
				}

				// A SOFTWARE DEADLINE, NOT A CLAIM ABOUT SCAN-OUT. The four APIs do
				// not expose one common present mode here: both Direct3D backends ask
				// for sync interval one, Vulkan uses FIFO, and GL inherits its driver
				// default. Pacing the workload at the declared rate makes a missed
				// budget visible on every backend. The result calls this the scheduled
				// interval and reports present() separately; the cloud host record is
				// what attests that a 60 Hz display mode existed.
				std::this_thread::sleep_until(this->next_frame_);
				const Clock::time_point frame_start = Clock::now();
				const Clock::duration period = std::chrono::duration_cast<Clock::duration>(
					std::chrono::duration<double>(1.0 /
						static_cast<double>(this->options_.refresh)));
				this->next_frame_ += period;

				std::int64_t scheduled_interval = 0;
				if (this->has_previous_frame_)
				{
					scheduled_interval = nanoseconds(frame_start - this->previous_frame_);
				}
				this->previous_frame_ = frame_start;
				this->has_previous_frame_ = true;

				this->scene_.update(0.0f);
				this->scene_.end_tick();
				const Clock::time_point update_end = Clock::now();

				const Clock::time_point begin_start = update_end;
				this->renderer_.begin_frame();
				const Clock::time_point begin_end = Clock::now();

				this->renderer_.begin_marker(L"Render");
				this->scene_.draw(this->renderer_);
				this->renderer_.submit();
				this->renderer_.end_marker();
				const Clock::time_point submit_end = Clock::now();

				this->renderer_.end_frame();
				const Clock::time_point present_end = Clock::now();

				if (this->failure_)
				{
					this->window_->close();
					return;
				}

				if (this->completed_frames_ >= this->options_.warmup)
				{
					this->samples_.push_back(FrameSample{
						nanoseconds(update_end - frame_start),
						nanoseconds(begin_end - begin_start),
						nanoseconds(submit_end - begin_end),
						nanoseconds(present_end - submit_end),
						nanoseconds(present_end - frame_start),
						scheduled_interval,
					});
				}

				++this->completed_frames_;
				if (this->completed_frames_ >=
					this->options_.warmup + this->options_.sample)
				{
					this->window_->close();
				}
			}
			catch (...)
			{
				if (!this->failure_)
				{
					this->failure_ = std::current_exception();
				}
				this->window_->close();
			}
		}

		void on_activated() override {}

		// Focus is not part of rendering, and an SSM-launched worker need not own
		// the interactive desktop. Losing it must not invalidate a GPU sample.
		void on_deactivated() override {}

		void on_suspending() override
		{
			if (this->running_)
			{
				this->set_failure("The benchmark was suspended during measurement.");
			}
		}

		void on_resuming() override {}
		void on_window_moved() const override {}

		void on_window_size_changed(int width, int height) override
		{
			this->width_ = width;
			this->height_ = height;
			if (this->running_ &&
				(width != frame_width || height != frame_height))
			{
				this->set_failure("The benchmark window changed from 1280x720.");
				return;
			}
			if (this->device_created_)
			{
				std::ignore = this->renderer_.window_size_changed(width, height);
			}
		}

		void on_key_down(Key /*key*/) const override {}
		void on_key_up(Key /*key*/) const override {}
		void on_text(char32_t /*codepoint*/) const override {}
		void on_mouse_move(int /*x*/, int /*y*/) const override {}
		void on_mouse_button_down(MouseButton /*button*/) const override {}
		void on_mouse_button_up(MouseButton /*button*/) const override {}
		void on_mouse_wheel(float /*notches*/) const override {}
		void on_mouse_wheel_horizontal(float /*notches*/) const override {}

		void on_device_lost() override
		{
			this->resources_.release_device_resources();
			if (this->running_)
			{
				this->set_failure("The render device was lost during measurement.");
			}
		}

		void on_device_restored() override
		{
			this->load_content();
		}

		Options options_;

		// The window outlives the resources and renderer, and the resource table
		// outlives the renderer. Destruction is the reverse of this declaration
		// order; render_resources.h makes the latter a contract.
		std::unique_ptr<Window> window_;
		RenderResources resources_;
		Renderer renderer_;

		Scene scene_;
		World world_;
		TickResult last_tick_;
		ParticleField* particles_ = nullptr;

		std::vector<FrameSample> samples_;
		Clock::time_point next_frame_;
		Clock::time_point previous_frame_;
		int completed_frames_ = 0;
		int width_ = 0;
		int height_ = 0;
		bool has_previous_frame_ = false;
		bool device_created_ = false;
		bool running_ = false;
		std::exception_ptr failure_;
		std::string measurement_class_;
		std::string started_utc_;
		std::string finished_utc_;
	};

	class ComApartment final
	{
	public:
		ComApartment()
		{
			const HRESULT result = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
			if (FAILED(result))
			{
				throw std::runtime_error("CoInitializeEx failed.");
			}
		}

		~ComApartment()
		{
			CoUninitialize();
		}

		ComApartment(const ComApartment&) = delete;
		ComApartment& operator=(const ComApartment&) = delete;
	};
}

int wmain(int argc, wchar_t* argv[])
{
	try
	{
		const Options options =
			linesweeper_frame_bench::parse_options(argc, argv);
		if (!release_build())
		{
			throw std::runtime_error(
				"LineSweeperFrameBench requires a Release build.");
		}
		if (!DirectX::XMVerifyCPUSupport())
		{
			throw std::runtime_error(
				"This CPU does not support the instruction set the renderer needs.");
		}

		ComApartment com;
		FrameBenchmark benchmark(options);
		benchmark.pump();
		const Result result = benchmark.take_result();
		linesweeper_frame_bench::atomic_write(options.output,
			linesweeper_frame_bench::result_json(options, result));

		std::printf("Wrote %d retained frames to %ls (%s, %s).\n",
			options.sample, options.output.c_str(),
			result.device.backend.c_str(), result.measurement_class.c_str());
		return 0;
	}
	catch (const std::exception& error)
	{
		linesweeper_frame_bench::print_usage();
		std::fprintf(stderr, "LineSweeperFrameBench refused: %s\n", error.what());
		return 1;
	}
}
