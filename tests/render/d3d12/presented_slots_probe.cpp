#include "tests/render/d3d12/wait_observer.h"
#include "tests/render/test_window.h"

#include "engine/render/d3d12/backend.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/throw_if_failed.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace
{
	using namespace labrador;
	using namespace labrador;
	using Microsoft::WRL::ComPtr;

	constexpr DWORD completion_timeout_ms = 5000;
	constexpr int window_size = 64;
	constexpr int unverified_exit = 2;
	static_assert(DeviceResources::frame_count == 2);

	class Unverified : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	[[noreturn]] void fail_fast(const char* operation, unsigned long long result) noexcept
	{
		std::fprintf(stderr, "D3D12 presented-slots probe fatal: %s (0x%llx)\n", operation, result);
		std::fflush(stderr);
		// Never return an unsuccessful bounded wait into production's void
		// block_until path and allow unsafe allocator/resource reuse.
		std::_Exit(EXIT_FAILURE);
	}

	class Watchdog
	{
	public:
		Watchdog() : deadline_(std::chrono::steady_clock::now() + std::chrono::seconds(10)),
			worker_([this]()
			{
				std::unique_lock<std::mutex> lock(this->mutex_);
				if (!this->condition_.wait_until(lock, this->deadline_,
					[this]() { return this->finished_; }))
				{
					fail_fast("ten-second process deadline expired", WAIT_TIMEOUT);
				}
			}) {}
		~Watchdog()
		{
			{
				std::lock_guard<std::mutex> lock(this->mutex_);
				this->check_deadline();
				this->finished_ = true;
			}
			this->condition_.notify_one();
			this->worker_.join();
			this->check_deadline();
		}
		Watchdog(const Watchdog&) = delete;
		Watchdog& operator=(const Watchdog&) = delete;
	private:
		void check_deadline() const noexcept
		{
			if (std::chrono::steady_clock::now() >= this->deadline_)
			{
				fail_fast("ten-second completion deadline expired", WAIT_TIMEOUT);
			}
		}
		std::mutex mutex_;
		std::condition_variable condition_;
		bool finished_ = false;
		const std::chrono::steady_clock::time_point deadline_;
		std::thread worker_;
	};

	class Event
	{
	public:
		Event()
		{
			this->handle_ = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
			if (this->handle_ == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}
		}
		~Event() { CloseHandle(this->handle_); }
		Event(const Event&) = delete;
		Event& operator=(const Event&) = delete;
		HANDLE handle() const { return this->handle_; }
	private:
		HANDLE handle_ = nullptr;
	};

	enum class Release
	{
		held,
		observer,
		deadline,
		early_wait,
		cleanup,
	};

	const char* release_name(Release release)
	{
		switch (release)
		{
		case Release::held: return "held";
		case Release::observer: return "reuse-observer";
		case Release::deadline: return "two-second-rescue";
		case Release::early_wait: return "early-backend-wait";
		case Release::cleanup: return "cleanup";
		}
		return "unknown";
	}

	struct WaitRecord;

	class Gate
	{
	public:
		explicit Gate(DeviceResources& resources) : queue_(resources.command_queue())
		{
			ID3D12Device* const device = resources.device();
			ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(this->gate_.GetAddressOf())));
			for (ComPtr<ID3D12Fence>& witness : this->witnesses_)
			{
				ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
					IID_PPV_ARGS(witness.GetAddressOf())));
			}
			ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(this->tail_.GetAddressOf())));
		}
		~Gate() { this->finish(); }
		Gate(const Gate&) = delete;
		Gate& operator=(const Gate&) = delete;

		void block_queue()
		{
			this->deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
			// The worker exists BEFORE Queue::Wait can block submitted work.
			// It alone CPU-signals value 1, for an observer, rescue or cleanup.
			this->worker_ = std::thread([this]()
				{
					std::unique_lock<std::mutex> lock(this->mutex_);
					if (!this->condition_.wait_until(lock, this->deadline_,
						[this]() { return this->release_ != Release::held; }))
					{
						this->release_ = Release::deadline;
					}
					lock.unlock();
					++this->signal_calls_;
					const HRESULT result = this->gate_->Signal(1);
					if (FAILED(result))
					{
						fail_fast("CPU gate signal failed", static_cast<unsigned long>(result));
					}
					lock.lock();
					this->released_ = true;
					this->condition_.notify_all();
				});
			this->armed_ = true;
			ThrowIfFailed(this->queue_->Wait(this->gate_.Get(), 1));
		}

		void checkpoint()
		{
			std::lock_guard<std::mutex> lock(this->mutex_);
			if (this->release_ == Release::held &&
				std::chrono::steady_clock::now() >= this->deadline_)
			{
				this->release_ = Release::deadline;
				this->condition_.notify_all();
			}
			if (this->release_ != Release::held)
			{
				throw Unverified(release_name(this->release_));
			}
		}

		void mark(std::size_t index)
		{
			// BEFORE the frame submission: the engine's subsequent frame-fence
			// completion then covers this slot's witness. A post-Present marker
			// could still lag the earlier engine fence at a correct wait return.
			ThrowIfFailed(this->queue_->Signal(this->witnesses_[index].Get(), 1));
		}

		UINT64 witness(std::size_t index) const
		{
			return this->witnesses_[index]->GetCompletedValue();
		}
		bool observe(HANDLE event, DWORD milliseconds, BOOL alertable, WaitRecord& record) noexcept;

		void release_and_wait(Release reason) noexcept
		{
			std::unique_lock<std::mutex> lock(this->mutex_);
			if (this->release_ == Release::held)
			{
				this->release_ = reason;
			}
			this->condition_.notify_all();
			if (!this->condition_.wait_for(lock, std::chrono::seconds(5),
				[this]() { return this->released_; }))
			{
				fail_fast("CPU gate release acknowledgement expired", WAIT_TIMEOUT);
			}
		}

		void finish() noexcept
		{
			if (this->finished_ || !this->armed_)
			{
				return;
			}
			this->release_and_wait(Release::cleanup);
			this->worker_.join();
			HRESULT result = this->queue_->Signal(this->tail_.Get(), 1);
			if (FAILED(result))
			{
				fail_fast("queue-tail signal failed", static_cast<unsigned long>(result));
			}
			result = this->tail_->SetEventOnCompletion(1, this->tail_done_.handle());
			if (FAILED(result))
			{
				fail_fast("queue-tail event failed", static_cast<unsigned long>(result));
			}
			const DWORD waited = ::WaitForSingleObjectEx(this->tail_done_.handle(),
				completion_timeout_ms, FALSE);
			if (waited != WAIT_OBJECT_0 || this->tail_->GetCompletedValue() != 1)
			{
				fail_fast("queue-tail completion failed", waited);
			}
			std::printf("gate_release=%s CPU_signals=%u tail=1 witnesses=%llu,%llu\n",
				release_name(this->release_), this->signal_calls_, this->witness(0),
				this->witness(1));
			this->finished_ = true;
		}

	private:
		ComPtr<ID3D12CommandQueue> queue_;
		ComPtr<ID3D12Fence> gate_;
		std::array<ComPtr<ID3D12Fence>, 2> witnesses_;
		ComPtr<ID3D12Fence> tail_;
		Event tail_done_;
		std::mutex mutex_;
		std::condition_variable condition_;
		std::chrono::steady_clock::time_point deadline_;
		Release release_ = Release::held;
		bool released_ = false;
		bool armed_ = false;
		bool finished_ = false;
		unsigned signal_calls_ = 0;
		std::thread worker_;
	};

	struct WaitRecord
	{
		Gate* gate = nullptr;
		bool observed = false;
		DWORD requested_timeout = 0;
		BOOL alertable = TRUE;
		DWORD event_before = WAIT_FAILED;
		DWORD completion = WAIT_FAILED;
	};

	thread_local Gate* active_gate = nullptr;
	thread_local WaitRecord* active_wait = nullptr;

	class GateScope
	{
	public:
		explicit GateScope(Gate& gate) { active_gate = &gate; }
		~GateScope() { active_gate = nullptr; active_wait = nullptr; }
		GateScope(const GateScope&) = delete;
		GateScope& operator=(const GateScope&) = delete;
	};

	bool Gate::observe(HANDLE event, DWORD milliseconds, BOOL alertable,
		WaitRecord& record) noexcept
	{
		std::unique_lock<std::mutex> lock(this->mutex_);
		// Claim the release under the SAME mutex as the rescue. Checking a
		// loose atomic and then probing would allow a rescue between them.
		if (this->release_ != Release::held || std::chrono::steady_clock::now() >= this->deadline_)
		{
			lock.unlock();
			this->release_and_wait(Release::deadline);
			return false;
		}
		record.requested_timeout = milliseconds;
		record.alertable = alertable;
		if (milliseconds != INFINITE || alertable != FALSE ||
			this->gate_->GetCompletedValue() != 0 ||
			this->witness(0) != 0 || this->witness(1) != 0)
		{
			lock.unlock();
			this->release_and_wait(Release::cleanup);
			fail_fast("final reuse wait was not pending behind the closed gate", milliseconds);
		}
		record.event_before = ::WaitForSingleObjectEx(event, 0, alertable);
		if (record.event_before != WAIT_TIMEOUT)
		{
			lock.unlock();
			this->release_and_wait(Release::cleanup);
			// Never consume an unexpectedly signaled auto-reset event and
			// then continue into a second wait or allocator reset.
			fail_fast("production event was not unsignaled", record.event_before);
		}
		if (std::chrono::steady_clock::now() >= this->deadline_)
		{
			lock.unlock();
			this->release_and_wait(Release::deadline);
			return false;
		}
		record.observed = true;
		this->release_ = Release::observer;
		lock.unlock();
		this->release_and_wait(Release::observer);
		return true;
	}

	void draw(Renderer& renderer, TextureHandle texture)
	{
		renderer.set_view_count(1);
		renderer.view(0).draw_sprite(texture, RectangleI(0, 0, 2, 2),
			RectangleF(4.0f, 4.0f, 16.0f, 16.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		renderer.submit();
	}

	int run_probe()
	{
		render_tests::TestWindow window(window_size, window_size);
		RenderResources resources;
		std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();
		renderer->create_device(window.handle(), window_size, window_size, 1);
		renderer->set_resources(&resources);
		{
			const RenderDeviceInfo& info = renderer->device_info();
			std::printf("backend=%s api=%s device=%s kind=%s build=%s\n", info.backend.c_str(),
				info.api.c_str(), info.device_name.c_str(),
				info.kind == RenderDeviceKind::hardware ? "hardware" : "software",
#ifdef NDEBUG
				"Release"
#else
				"Debug"
#endif
			);
			std::fflush(stdout);
		}
		TextureHandle texture;
		{
			TextureData white;
			white.width = 2;
			white.height = 2;
			white.levels.push_back(texture_level(white.format, 2, 2, 0));
			white.pixels.resize(white.levels[0].size, 255);
			add_texture_asset(*renderer, resources, "presented_slots_probe", white);
			texture = resources.resolve_texture("presented_slots_probe");
		}
		DeviceResources& device = renderer->impl()->device_resources;
		unsigned warm_mask = 0;
		for (int frame = 0; frame < 6; ++frame)
		{
			renderer->begin_frame();
			const int slot = device.frame_index();
			if (slot < 0 || slot >= 2)
			{
				throw std::runtime_error("invalid warm frame index");
			}
			warm_mask |= 1u << static_cast<unsigned>(slot);
			draw(*renderer, texture);
			renderer->end_frame();
		}
		device.wait_for_gpu();
		std::printf("ordinary warm presents=6 slot_mask=0x%x, drained before queue gate\n",
			warm_mask);
		std::fflush(stdout);
		if (warm_mask != 3)
		{
			throw Unverified("warm hidden presents did not visit both slots");
		}

		// Declared AFTER engine owners: every exit opens the gate and retires
		// its queue tail before renderer/resources can be destroyed.
		Gate gate(device);
		gate.block_queue();
		std::array<int, 2> slots{};
		WaitRecord record{ &gate };
		int reused_slot = -1;
		{
			GateScope scope(gate);
			for (std::size_t frame = 0; frame < slots.size(); ++frame)
			{
				gate.checkpoint();
				renderer->begin_frame();
				gate.checkpoint();
				slots[frame] = device.frame_index();
				if (slots[frame] < 0 || slots[frame] >= 2)
				{
					throw std::runtime_error("invalid gated frame index");
				}
				gate.mark(frame);
				draw(*renderer, texture);
				gate.checkpoint();
				std::printf("held-gate frame=%zu submitted_slot=%d entering Present\n",
					frame, slots[frame]);
				std::fflush(stdout);
				renderer->end_frame();
				gate.checkpoint();
				std::printf("held-gate frame=%zu submitted_slot=%d next_slot=%d witness=%llu\n",
					frame, slots[frame], device.frame_index(), gate.witness(frame));
				std::fflush(stdout);
				if (frame == 0 && device.frame_index() == slots[0])
				{
					throw Unverified("first hidden Present retained its slot; second frame not "
						"attempted");
				}
			}
			if (slots[0] == slots[1])
			{
				throw Unverified("gated frames did not use distinct slots");
			}
			reused_slot = device.frame_index();
			if (reused_slot != slots[0] && reused_slot != slots[1])
			{
				throw std::runtime_error("next frame is not one of the two submitted slots");
			}
			gate.checkpoint();
			// Only this direct begin_frame may establish the verified wait.
			// Earlier production waits request release and mark unverified.
			active_wait = &record;
			renderer->begin_frame();
			active_wait = nullptr;
		}
		gate.finish();
		if (!record.observed)
		{
			throw Unverified("two-slot reuse wait was not observed before rescue");
		}
		if (record.completion != WAIT_OBJECT_0 || gate.witness(0) != 1 || gate.witness(1) != 1)
		{
			throw std::runtime_error("two-slot completion or queue retirement differs");
		}
		std::printf("observed submitted_slots=%d,%d reused_slot=%d requested_timeout=%lu "
			"alertable=%d initial_event=%lu completion=%lu\n", slots[0], slots[1], reused_slot,
			record.requested_timeout, record.alertable, record.event_before, record.completion);
		std::fputs("Instrumented wait with two queued, unretired presented slots; no physical "
			"display overlap, simultaneous GPU execution, saturation or final-device claim.\n",
			stdout);
		return EXIT_SUCCESS;
	}
}

namespace labrador::d3d12_wait_test
{
	DWORD WINAPI bounded_wait(HANDLE event, DWORD milliseconds, BOOL alertable) noexcept
	{
		WaitRecord* const record = active_wait;
		if (record != nullptr)
		{
			if (record->observed)
			{
				record->gate->release_and_wait(Release::cleanup);
				fail_fast("more than one wait in final begin_frame", milliseconds);
			}
			static_cast<void>(record->gate->observe(event, milliseconds, alertable, *record));
		}
		else if (active_gate != nullptr)
		{
			active_gate->release_and_wait(Release::early_wait);
		}
		const DWORD result = ::WaitForSingleObjectEx(event,
			std::min(milliseconds, completion_timeout_ms), alertable);
		if (result != WAIT_OBJECT_0)
		{
			if (active_gate != nullptr)
			{
				active_gate->release_and_wait(Release::cleanup);
			}
			fail_fast("bounded production wait failed", result);
		}
		if (record != nullptr)
		{
			record->completion = result;
		}
		return result;
	}
}

int main()
{
	// This outlives run_probe's engine owners and its gate/tail cleanup.
	Watchdog watchdog;
	try
	{
		const int result = run_probe();
		std::fputs("VERIFIED: two actual presented slots had queued, unretired submissions "
			"at the instrumented reuse wait; cleanup completed.\n", stdout);
		return result;
	}
	catch (const Unverified& reason)
	{
		std::fprintf(stderr, "UNVERIFIED capability: %s\n", reason.what());
		return unverified_exit;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "FAILED: %s\n", error.what());
		return EXIT_FAILURE;
	}
	catch (...)
	{
		std::fputs("FAILED: unknown exception\n", stderr);
		return EXIT_FAILURE;
	}
}
