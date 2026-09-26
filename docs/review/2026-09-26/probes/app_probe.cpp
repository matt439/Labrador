#include "engine/app/window.h"
#include "engine/assets/json.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

class ProbeNotify : public labrador::WindowNotify
{
public:
    mutable labrador::Keyboard keyboard;
    mutable labrador::Mouse mouse;
    void tick() override {}
    void on_activated() override { keyboard.set_focused(true); mouse.set_focused(true); }
    void on_deactivated() override { keyboard.set_focused(false); mouse.set_focused(false); }
    void on_suspending() override { on_deactivated(); }
    void on_resuming() override { on_activated(); }
    void on_window_moved() const override {}
    void on_window_size_changed(int, int) override {}
    void on_key_down(labrador::Key key) const override { keyboard.on_key_down(key); }
    void on_key_up(labrador::Key key) const override { keyboard.on_key_up(key); }
    void on_text(char32_t character) const override { keyboard.on_text(character); }
    void on_mouse_move(int x, int y) const override { mouse.on_move(mattmath::Vector2I(x, y)); }
    void on_mouse_button_down(labrador::MouseButton button) const override { mouse.on_button_down(button); }
    void on_mouse_button_up(labrador::MouseButton button) const override { mouse.on_button_up(button); }
    void on_mouse_wheel(float delta) const override { mouse.on_wheel(delta); }
    void on_mouse_wheel_horizontal(float delta) const override { mouse.on_wheel_horizontal(delta); }
};

int main()
{
    ProbeNotify notify;
    labrador::WindowOptions options;
    options.window_class_name = L"LabradorReviewProbe";
    options.client_size = mattmath::Vector2I(64, 64);
    labrador::Window window(GetModuleHandleW(nullptr), SW_HIDE, options, &notify);
    notify.on_activated();
    notify.keyboard.poll();
    notify.mouse.poll();
    SendMessageW(window.handle(), WM_LBUTTONDOWN, MK_LBUTTON, 0);
    notify.mouse.poll();
    std::printf("mouse held before capture loss=%d captured=%d\n", notify.mouse.held(labrador::MouseButton::left), GetCapture() == window.handle());
    ReleaseCapture();
    notify.mouse.poll();
    std::printf("mouse held after capture loss=%d captured=%d focused=%d\n", notify.mouse.held(labrador::MouseButton::left), GetCapture() == window.handle(), notify.mouse.focused());
    // French physical US-W position: scan code 0x11, virtual-key Z.
    SendMessageW(window.handle(), WM_KEYDOWN, 'Z', static_cast<LPARAM>(0x00110001));
    notify.keyboard.poll();
    std::printf("French US-W position: w=%d z=%d\n", notify.keyboard.held(labrador::Key::w), notify.keyboard.held(labrador::Key::z));
    // Both physical shift keys map to one logical bit.
    SendMessageW(window.handle(), WM_KEYDOWN, VK_SHIFT, static_cast<LPARAM>(0x002A0001));
    SendMessageW(window.handle(), WM_KEYDOWN, VK_SHIFT, static_cast<LPARAM>(0x00360001));
    notify.keyboard.poll();
    SendMessageW(window.handle(), WM_KEYUP, VK_SHIFT, static_cast<LPARAM>(0xC02A0001));
    notify.keyboard.poll();
    std::printf("right shift still physically down: aggregate shift held=%d released=%d\n", notify.keyboard.held(labrador::Key::shift), notify.keyboard.released(labrador::Key::shift));
    const char* path = "out/review-2026-09-26/json-probe.json";
    std::ofstream(path) << R"({"name":"asset\u0000other","number":1e100})";
    const labrador::JsonDocument json = labrador::read_json_file(path);
    std::printf("JSON embedded NUL: expected length 11 actual=%zu; number finite=%d\n", json.root().string("name").size(), std::isfinite(json.root().number("number")));
}
