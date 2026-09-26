#include "engine/app/window.h"
#include "engine/ui/focus.h"
#include "engine/ui/widget.h"

#include <cstdio>
#include <memory>
#include <string>

class ProbeNotify final : public labrador::WindowNotify
{
public:
    int suspends = 0;
    int resumes = 0;
    void tick() override {}
    void on_activated() override {}
    void on_deactivated() override {}
    void on_suspending() override { ++suspends; }
    void on_resuming() override { ++resumes; }
    void on_window_moved() const override {}
    void on_window_size_changed(int, int) override {}
    void on_key_down(labrador::Key) const override {}
    void on_key_up(labrador::Key) const override {}
    void on_text(char32_t) const override {}
    void on_mouse_move(int, int) const override {}
    void on_mouse_button_down(labrador::MouseButton) const override {}
    void on_mouse_button_up(labrador::MouseButton) const override {}
    void on_mouse_wheel(float) const override {}
    void on_mouse_wheel_horizontal(float) const override {}
};

class ProbeWidget final : public labrador::UiWidget
{
public:
    ProbeWidget() : labrador::UiWidget("probe") {}
    void update(float) override {}
    void draw(labrador::DrawList&) const override {}
    void set_colour(const labrador::Colour&) override {}
    mattmath::RectangleF bounds() const override
    {
        return mattmath::RectangleF(0, 0, 10, 10);
    }
};

int main()
{
    ProbeNotify notify;
    labrador::WindowOptions options;
    options.window_class_name = L"LabradorReviewClientsPower";
    options.client_size = mattmath::Vector2I(64, 64);
    labrador::Window window(GetModuleHandleW(nullptr), SW_HIDE, options,
        &notify);
    SendMessageW(window.handle(), WM_POWERBROADCAST, PBT_APMSUSPEND, 0);
    SendMessageW(window.handle(), WM_POWERBROADCAST, PBT_APMRESUMESUSPEND, 0);
    std::printf("modern power suspend/resume: suspending=%d resuming=%d\n",
        notify.suspends, notify.resumes);
    SendMessageW(window.handle(), WM_POWERBROADCAST, PBT_APMQUERYSUSPEND, 0);
    SendMessageW(window.handle(), WM_POWERBROADCAST, PBT_APMRESUMESUSPEND, 0);
    std::printf("obsolete query-suspend/resume: suspending=%d resuming=%d\n",
        notify.suspends, notify.resumes);

    ProbeWidget widget;
    labrador::FocusGroup group;
    std::shared_ptr<int> captured = std::make_shared<int>(17);
    const std::weak_ptr<int> lifetime = captured;
    group.add(&widget, [&group, token = std::move(captured)]()
    {
        // Copy every observation before clear. Nothing reads the destroyed
        // callable or its captures after clear; the weak_ptr is a local.
        const std::weak_ptr<int> observed = token;
        labrador::FocusGroup* target = &group;
        std::printf("callback token alive before rebuilding=%d\n",
            !observed.expired());
        target->clear();
        std::printf("callback still executing: token already destroyed=%d\n",
            observed.expired());
    });
    const labrador::Activation activation = group.activate(0);
    std::printf("activation returned ran=%d group empty=%d captured expired=%d\n",
        activation == labrador::Activation::ran, group.size() == 0,
        lifetime.expired());
}
