#include "engine/app/application.h"
#include "engine/collision/narrow_phase.h"
#include "engine/collision/tunnelling.h"
#include "engine/core/state_context.h"
#include "engine/math/rectanglef.h"

#include <cstdio>
#include <memory>
#include <stdexcept>

namespace
{
    struct PlainState : labrador::State
    {
        void init() override {}
        void update(float) override {}
        void draw(labrador::Renderer&) const override {}
    };

    struct NestedInitState : PlainState
    {
        bool* destroyed;
        explicit NestedInitState(bool* flag) : destroyed(flag) {}
        ~NestedInitState() override { *destroyed = true; }
        void init() override
        {
            // All borrowed state is copied before the potentially destructive call.
            bool* flag = destroyed;
            labrador::StateContext* owner = context();
            owner->transition_to(std::make_unique<PlainState>());
            owner->notify_activation(false);
            std::printf("nested init callback still running; destroyed=%d\n", *flag ? 1 : 0);
        }
    };
}

int main()
{
    // Valid-option acceptance and bounded execution of the actual StepTimer loop.
    labrador::ApplicationOptions options;
    options.target_fps = 10000001;
    options.validate();
    const double seconds = 1.0 / static_cast<double>(options.target_fps);
    std::printf("target_fps=%d accepted; converted timer ticks=%llu\n",
        options.target_fps, static_cast<unsigned long long>(labrador::StepTimer::SecondsToTicks(seconds)));
    labrador::StepTimer timer;
    timer.SetFixedTimeStep(true);
    timer.SetTargetElapsedSeconds(seconds);
    int callbacks = 0;
    try
    {
        timer.Tick([&timer, &callbacks]()
            {
                ++callbacks;
                std::printf("timer callback=%d elapsed_ticks=%llu total_ticks=%llu\n",
                    callbacks, static_cast<unsigned long long>(timer.GetElapsedTicks()),
                    static_cast<unsigned long long>(timer.GetTotalTicks()));
                if (callbacks == 3) { throw std::runtime_error("bounded probe stops zero-step loop"); }
            });
    }
    catch (const std::runtime_error& error)
    {
        std::printf("timer stopped by probe: %s\n", error.what());
    }

    // A diagonal corner crossing at ordinary finite world coordinates. Each
    // square's projection extent on the direction of travel is sqrt(2).
    const mattmath::RectangleF fixed(0.0f, 0.0f, 1.0f, 1.0f);
    const mattmath::RectangleF before(-1.1f, 0.8f, 1.0f, 1.0f);
    const mattmath::RectangleF middle(-0.95f, 0.95f, 1.0f, 1.0f);
    const mattmath::RectangleF after(-0.8f, 1.1f, 1.0f, 1.0f);
    const float displacement = (after.position() - before.position()).length();
    const float projected_extent = mattmath::Vector2F(1.0f, 1.0f).length();
    std::printf("tunnelling: displacement=%.9g budget=%.9g can_tunnel=%d\n",
        static_cast<double>(displacement),
        static_cast<double>(labrador::max_safe_displacement(projected_extent, projected_extent)),
        labrador::can_tunnel(displacement, projected_extent, projected_extent) ? 1 : 0);
    std::printf("actual overlap before=%d middle=%d after=%d\n",
        labrador::narrow_phase(fixed, before).has_value() ? 1 : 0,
        labrador::narrow_phase(fixed, middle).has_value() ? 1 : 0,
        labrador::narrow_phase(fixed, after).has_value() ? 1 : 0);

    bool destroyed = false;
    labrador::StateContext owner;
    owner.transition_to(std::make_unique<NestedInitState>(&destroyed));
    std::printf("nested init final context depth=%d\n", owner.depth());
}
