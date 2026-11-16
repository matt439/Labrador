#pragma once

#include "engine/core/diagnostic.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace labrador_tests
{
	// Single-threaded fixture. The fixed storage makes the noexcept callback
	// honest; a long message is flagged rather than allocating in the callback.
	class DiagnosticCapture
	{
	public:
		DiagnosticCapture()
		{
			labrador::set_diagnostic_callback(receive, this);
		}

		~DiagnosticCapture()
		{
			labrador::set_diagnostic_callback(nullptr);
		}

		DiagnosticCapture(const DiagnosticCapture&) = delete;
		DiagnosticCapture& operator=(const DiagnosticCapture&) = delete;

		std::string_view messages() const
		{
			return std::string_view(this->bytes_.data(), this->size_);
		}
		std::size_t calls() const { return this->calls_; }
		bool overflow() const { return this->overflow_; }

	private:
		static void receive(std::string_view message, void* context) noexcept
		{
			DiagnosticCapture& capture = *static_cast<DiagnosticCapture*>(context);
			++capture.calls_;
			const std::size_t count = std::min(message.size(),
				capture.bytes_.size() - capture.size_);
			std::copy_n(message.begin(), count,
				capture.bytes_.begin() + capture.size_);
			capture.size_ += count;
			capture.overflow_ |= count != message.size();
		}

		std::array<char, 16384> bytes_{};
		std::size_t size_ = 0;
		std::size_t calls_ = 0;
		bool overflow_ = false;
	};
}
