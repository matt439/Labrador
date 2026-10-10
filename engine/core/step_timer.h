//
// StepTimer.h - A simple timer that provides elapsed time information
//

#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>


namespace labrador
{
    // Helper class for animation and simulation timing.
    //
    // The source clock is std::chrono::steady_clock, on every platform. On
    // Windows that is QueryPerformanceCounter read through the standard
    // library, which agrees with a direct QPC call to within the time
    // between two reads of it, and in a
    // browser it is performance.now(), coarsened by the browser to somewhere
    // between 0.1 and 1 ms - which a 60 Hz step clamped at a tenth of a
    // second does not notice.
    class StepTimer
    {
    public:
        StepTimer() noexcept(false) :
            m_elapsedTicks(0),
            m_totalTicks(0),
            m_leftOverTicks(0),
            m_frameCount(0),
            m_framesPerSecond(0),
            m_framesThisSecond(0),
            m_secondCounter(0),
            m_isFixedTimeStep(false),
            m_targetElapsedTicks(TicksPerSecond / 60)
        {
            m_lastTime = Clock::now();

            // Initialize max delta to 1/10 of a second.
            m_maxDelta = ClockFrequency / 10;
        }

        // Get elapsed time since the previous Update call.
        uint64_t GetElapsedTicks() const noexcept { return m_elapsedTicks; }
        double GetElapsedSeconds() const noexcept { return TicksToSeconds(m_elapsedTicks); }

        // Get total time since the start of the program.
        uint64_t GetTotalTicks() const noexcept { return m_totalTicks; }
        double GetTotalSeconds() const noexcept { return TicksToSeconds(m_totalTicks); }

        // Get total number of updates since start of the program.
        uint32_t GetFrameCount() const noexcept { return m_frameCount; }

        // Get the current framerate.
        uint32_t GetFramesPerSecond() const noexcept { return m_framesPerSecond; }

        // Set whether to use fixed or variable timestep mode.
        void SetFixedTimeStep(bool isFixedTimestep) noexcept { m_isFixedTimeStep = isFixedTimestep; }

        // Set how often to call Update when in fixed timestep mode.
        // A fixed step must advance time. Invalid setters leave the old period intact.
        void SetTargetElapsedTicks(uint64_t targetElapsed)
        {
            if (targetElapsed == 0)
            {
                throw std::invalid_argument("StepTimer target period must be positive.");
            }
            m_targetElapsedTicks = targetElapsed;
        }
        void SetTargetElapsedSeconds(double targetElapsed)
        {
            SetTargetElapsedTicks(SecondsToTicks(targetElapsed));
        }

        // Integer format represents time using 10,000,000 ticks per second.
        static constexpr uint64_t TicksPerSecond = 10000000;

        static constexpr double TicksToSeconds(uint64_t ticks) noexcept { return static_cast<double>(ticks) / TicksPerSecond; }
        static uint64_t SecondsToTicks(double seconds)
        {
            const double ticks = seconds * TicksPerSecond;
            // The rounded double representation of UINT64_MAX is 2^64.
            if (!std::isfinite(ticks) || ticks < 0.0 ||
                ticks >= static_cast<double>((std::numeric_limits<uint64_t>::max)()))
            {
                throw std::invalid_argument("StepTimer seconds are outside the tick range.");
            }
            return static_cast<uint64_t>(ticks);
        }

        // After an intentional timing discontinuity (for instance a blocking IO operation)
        // call this to avoid having the fixed timestep logic attempt a set of catch-up
        // Update calls.

        void ResetElapsedTime()
        {
            m_lastTime = Clock::now();

            m_leftOverTicks = 0;
            m_framesPerSecond = 0;
            m_framesThisSecond = 0;
            m_secondCounter = 0;
        }

        // Update timer state, calling the specified Update function the appropriate number of times.
        template<typename TUpdate>
        void Tick(const TUpdate& update)
        {
            // Query the current time.
            const Clock::time_point currentTime = Clock::now();

            uint64_t timeDelta = static_cast<uint64_t>((currentTime - m_lastTime).count());

            m_lastTime = currentTime;
            m_secondCounter += timeDelta;

            // Clamp excessively large time deltas (e.g. after paused in the debugger).
            if (timeDelta > m_maxDelta)
            {
                timeDelta = m_maxDelta;
            }

            // Convert clock units into a canonical tick format. This cannot overflow due to the previous clamp.
            timeDelta *= TicksPerSecond;
            timeDelta /= ClockFrequency;

            const uint32_t lastFrameCount = m_frameCount;

            if (m_isFixedTimeStep)
            {
                // Fixed timestep update logic

                // If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
                // the clock to exactly match the target value. This prevents tiny and irrelevant errors
                // from accumulating over time. Without this clamping, a game that requested a 60 fps
                // fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
                // accumulate enough tiny errors that it would drop a frame. It is better to just round
                // small deviations down to zero to leave things running smoothly.

                if (static_cast<uint64_t>(std::abs(static_cast<int64_t>(timeDelta - m_targetElapsedTicks))) < TicksPerSecond / 4000)
                {
                    timeDelta = m_targetElapsedTicks;
                }

                m_leftOverTicks += timeDelta;

                while (m_leftOverTicks >= m_targetElapsedTicks)
                {
                    m_elapsedTicks = m_targetElapsedTicks;
                    m_totalTicks += m_targetElapsedTicks;
                    m_leftOverTicks -= m_targetElapsedTicks;
                    m_frameCount++;

                    update();
                }
            }
            else
            {
                // Variable timestep update logic.
                m_elapsedTicks = timeDelta;
                m_totalTicks += timeDelta;
                m_leftOverTicks = 0;
                m_frameCount++;

                update();
            }

            // Track the current framerate.
            if (m_frameCount != lastFrameCount)
            {
                m_framesThisSecond++;
            }

            if (m_secondCounter >= ClockFrequency)
            {
                m_framesPerSecond = m_framesThisSecond;
                m_framesThisSecond = 0;
                m_secondCounter %= ClockFrequency;
            }
        }

    private:
        // Source timing data uses the clock's own units.
        using Clock = std::chrono::steady_clock;
        static_assert(Clock::period::num == 1,
            "StepTimer assumes a clock whose tick is a whole fraction of a second.");
        static constexpr uint64_t ClockFrequency = Clock::period::den;

        Clock::time_point m_lastTime;
        uint64_t m_maxDelta;

        // Derived timing data uses a canonical tick format.
        uint64_t m_elapsedTicks;
        uint64_t m_totalTicks;
        uint64_t m_leftOverTicks;

        // Members for tracking the framerate.
        uint32_t m_frameCount;
        uint32_t m_framesPerSecond;
        uint32_t m_framesThisSecond;
        uint64_t m_secondCounter;

        // Members for configuring fixed timestep mode.
        bool m_isFixedTimeStep;
        uint64_t m_targetElapsedTicks;
    };
}
