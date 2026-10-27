#include "tests/render/d3d12/wait_observer.h"

#include "engine/render/d3d12/backend.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/throw_if_failed.h"
#include "tests/render/test_window.h"

#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>

namespace
{
	using namespace labrador;
	using namespace labrador;
	using Microsoft::WRL::ComPtr;

	constexpr DWORD completion_timeout_ms = 5000;
	constexpr int initial_size = 64;
	constexpr int resized_size = 32;

	[[noreturn]] void fail_fast(const char* operation, unsigned long long result,
		DWORD win32_error = ERROR_SUCCESS) noexcept
	{
		std::fprintf(stderr, "D3D12 synchronization test fatal: %s "
			"(result 0x%llx, Win32 error 0x%lx)\n", operation, result, win32_error);
		std::fflush(stderr);
		// A failed bounded wait must not return into block_until: production
		// ignores the DWORD result and would continue reset or resource release.
		// This is only this isolated test process, never another process/device.
		std::_Exit(EXIT_FAILURE);
	}

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

	class QueueGate
	{
	public:
		explicit QueueGate(DeviceResources& resources) : queue_(resources.command_queue())
		{
			ID3D12Device* device = resources.device();
			ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(this->gate_.GetAddressOf())));
			ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(this->witness_.GetAddressOf())));
			ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(this->tail_.GetAddressOf())));
			// Arm before block_queue can enqueue a wait. If a boundary never
			// reaches our observer and blocks elsewhere, this still releases the
			// host-owned fence and terminates only the isolated executable.
			const std::chrono::steady_clock::time_point deadline =
				std::chrono::steady_clock::now() + std::chrono::seconds(10);
			this->watchdog_ = std::thread([this, deadline]()
				{
					std::unique_lock<std::mutex> lock(this->mutex_);
					if (this->condition_.wait_until(lock, deadline,
						[this]() { return this->cancelled_; }))
					{
						return;
					}
					lock.unlock();
					this->release();
					fail_fast("boundary watchdog expired", WAIT_TIMEOUT);
				});
		}

		~QueueGate() { this->finish(); }
		QueueGate(const QueueGate&) = delete;
		QueueGate& operator=(const QueueGate&) = delete;

		void block_queue()
		{
			ThrowIfFailed(this->queue_->Wait(this->gate_.Get(), 1));
			// BEFORE submit: its engine frame-fence signal follows this witness.
			// A witness queued after submit could lag a correct engine wait.
			ThrowIfFailed(this->queue_->Signal(this->witness_.Get(), 1));
		}

		UINT64 witness() const { return this->witness_->GetCompletedValue(); }

		HRESULT release() noexcept
		{
			// Signaling the same CPU-owned value is idempotent. The watchdog
			// independently attempts it even if another release is in progress.
			const HRESULT result = this->gate_->Signal(1);
			if (FAILED(result))
			{
				fail_fast("CPU gate Signal failed", static_cast<unsigned long>(result));
			}
			return result;
		}

		void finish() noexcept
		{
			if (this->finished_)
			{
				return;
			}
			// Always release before waiting for the watchdog or destroying any
			// renderer/resource owner, including assertion/exception unwinding.
			this->release();
			{
				std::lock_guard<std::mutex> lock(this->mutex_);
				this->cancelled_ = true;
			}
			this->condition_.notify_one();
			this->watchdog_.join();

			// A separate tail includes ALL submitted work, including new clear
			// commands recorded by a completed begin_frame or resize boundary.
			HRESULT result = this->queue_->Signal(this->tail_.Get(), 1);
			if (FAILED(result))
			{
				fail_fast("tail Signal failed", static_cast<unsigned long>(result));
			}
			result = this->tail_->SetEventOnCompletion(1, this->tail_done_.handle());
			if (FAILED(result))
			{
				fail_fast("tail event registration failed", static_cast<unsigned long>(result));
			}
			const DWORD waited = ::WaitForSingleObjectEx(this->tail_done_.handle(),
				completion_timeout_ms, FALSE);
			const DWORD wait_error = waited == WAIT_FAILED ? GetLastError() : ERROR_SUCCESS;
			if (waited != WAIT_OBJECT_0 || this->tail_->GetCompletedValue() != 1)
			{
				fail_fast("tail did not complete normally", waited, wait_error);
			}
			this->finished_ = true;
		}

	private:
		ComPtr<ID3D12CommandQueue> queue_;
		ComPtr<ID3D12Fence> gate_;
		ComPtr<ID3D12Fence> witness_;
		ComPtr<ID3D12Fence> tail_;
		Event tail_done_;
		std::mutex mutex_;
		std::condition_variable condition_;
		bool cancelled_ = false;
		bool finished_ = false;
		std::thread watchdog_;
	};

	struct WaitRecord
	{
		QueueGate* gate = nullptr;
		bool observed = false;
		UINT64 witness_before = 0;
		DWORD requested_timeout = 0;
		BOOL requested_alertable = TRUE;
		DWORD initial_event_state = WAIT_FAILED;
		HRESULT release_result = E_FAIL;
		DWORD completion = WAIT_FAILED;
	};

	thread_local WaitRecord* active_record = nullptr;

	class ObserveWait
	{
	public:
		explicit ObserveWait(WaitRecord& record) : previous_(active_record)
		{
			active_record = &record;
		}
		~ObserveWait() { active_record = this->previous_; }
		ObserveWait(const ObserveWait&) = delete;
		ObserveWait& operator=(const ObserveWait&) = delete;
	private:
		WaitRecord* previous_;
	};

	struct Fixture
	{
		render_tests::TestWindow window{ initial_size, initial_size };
		RenderResources resources;
		std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();

		Fixture()
		{
			this->renderer->create_device(this->window.handle(), initial_size, initial_size, 1);
			this->renderer->set_resources(&this->resources);
			TextureData white;
			white.width = 2;
			white.height = 2;
			white.levels.push_back(texture_level(white.format, 2, 2, 0));
			white.pixels.resize(white.levels[0].size, 255);
			add_texture_asset(*this->renderer, this->resources, "pending_submission", white);
			const TextureHandle texture = this->resources.resolve_texture("pending_submission");
			this->renderer->begin_frame();
			this->renderer->set_view_count(1);
			this->renderer->view(0).draw_sprite(texture, RectangleI(0, 0, 2, 2),
				RectangleF(4.0f, 4.0f, 16.0f, 16.0f), Colour::white, 0.0f,
				Vector2F::zero, SpriteFlip::none, 0.0f);
		}
	};

	void check_observation(const WaitRecord& record, const QueueGate& gate)
	{
		REQUIRE(record.observed);
		CHECK(record.requested_timeout == INFINITE);
		CHECK(record.requested_alertable == FALSE);
		CHECK(record.witness_before == 0);
		CHECK(record.initial_event_state == WAIT_TIMEOUT);
		CHECK(record.release_result == S_OK);
		CHECK(record.completion == WAIT_OBJECT_0);
		// UINT64_MAX on removal must not masquerade as completion.
		CHECK(gate.witness() == 1);
	}
}

namespace labrador::d3d12_wait_test
{
	DWORD WINAPI bounded_wait(HANDLE event, DWORD milliseconds, BOOL alertable) noexcept
	{
		WaitRecord* record = active_record;
		const bool first = record != nullptr && !record->observed;
		if (first)
		{
			record->requested_timeout = milliseconds;
			record->requested_alertable = alertable;
			record->witness_before = record->gate->witness();
			if (milliseconds != INFINITE || alertable != FALSE || record->witness_before != 0)
			{
				record->gate->release();
				fail_fast("pending-wait precondition failed", record->witness_before);
			}
			// Probe without consuming a signaled auto-reset event and then
			// continuing. An unexpected result terminates this process instead.
			record->initial_event_state = ::WaitForSingleObjectEx(event, 0, alertable);
			const DWORD probe_error = record->initial_event_state == WAIT_FAILED
				? GetLastError() : ERROR_SUCCESS;
			if (record->initial_event_state != WAIT_TIMEOUT)
			{
				record->gate->release();
				fail_fast("production event was not unsignaled", record->initial_event_state,
					probe_error);
			}
			record->observed = true;
			record->release_result = record->gate->release();
		}
		const DWORD result = ::WaitForSingleObjectEx(event, completion_timeout_ms, alertable);
		const DWORD wait_error = result == WAIT_FAILED ? GetLastError() : ERROR_SUCCESS;
		if (result != WAIT_OBJECT_0)
		{
			if (record != nullptr)
			{
				record->gate->release();
			}
			fail_fast("bounded production wait failed", result, wait_error);
		}
		if (first)
		{
			record->completion = result;
		}
		return result;
	}
}

// These are instrumented ordering checks. The queue holds real renderer work
// pending at each observed production wait; the CPU releases the gate there.
// They do not establish an unmodified infinite wait, physical GPU saturation,
// overlap of both swapchain slots, Vulkan behavior or final device destruction.
TEST_CASE("D3D12 same-index begin_frame observes a pending submission wait")
{
	Fixture fixture;
	DeviceResources& device = fixture.renderer->impl()->device_resources;
	const int slot = device.frame_index();
	QueueGate gate(device);
	gate.block_queue();
	fixture.renderer->submit();
	WaitRecord record{ &gate };
	{
		ObserveWait observation(record);
		REQUIRE(gate.witness() == 0);
		fixture.renderer->begin_frame();
	}
	gate.finish();
	check_observation(record, gate);
	CHECK(device.frame_index() == slot);
}

TEST_CASE("D3D12 real hidden resize observes a pending submission wait")
{
	Fixture fixture;
	REQUIRE(fixture.window.resize(resized_size, resized_size));
	QueueGate gate(fixture.renderer->impl()->device_resources);
	gate.block_queue();
	fixture.renderer->submit();
	WaitRecord record{ &gate };
	bool resized = false;
	{
		ObserveWait observation(record);
		REQUIRE(gate.witness() == 0);
		resized = fixture.renderer->window_size_changed(resized_size, resized_size);
	}
	gate.finish();
	check_observation(record, gate);
	CHECK(resized);
	CHECK(fixture.renderer->back_buffer_size() == Vector2F(
		static_cast<float>(resized_size), static_cast<float>(resized_size)));
}

TEST_CASE("D3D12 renderer teardown observes a pending submission wait")
{
	Fixture fixture;
	QueueGate gate(fixture.renderer->impl()->device_resources);
	gate.block_queue();
	fixture.renderer->submit();
	WaitRecord record{ &gate };
	{
		ObserveWait observation(record);
		REQUIRE(gate.witness() == 0);
		fixture.renderer.reset();
	}
	gate.finish();
	check_observation(record, gate);
}
