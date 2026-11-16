#include <doctest/doctest.h>

#include "samples/components/binding_row.h"
#include "samples/components/test_content.h"

#include "engine/input/keyboard.h"
#include "engine/math/vector2f.h"

using namespace labrador;
using components::BindingRow;
using components::CaptureKind;
using components::TestContent;

TEST_CASE("binding capture waits for its opener and every previously held key to release")
{
	TestContent content;
	BindingRow row("bind", L"Rebind", L"Restart: r", Vector2F::zero, 200.0f, "font", &content.resources);
	Keyboard keyboard;
	keyboard.set_focused(true);
	keyboard.poll();
	keyboard.on_key_down(Key::enter);
	keyboard.on_key_down(Key::r);
	keyboard.poll();
	row.begin_capture();
	CHECK(row.update_capture(keyboard.state(), keyboard.previous_state()).kind == CaptureKind::none);
	CHECK(row.waiting_for_release());
	keyboard.on_key_up(Key::enter);
	keyboard.on_key_down(Key::t);
	keyboard.poll();
	CHECK(row.update_capture(keyboard.state(), keyboard.previous_state()).kind == CaptureKind::none);
	keyboard.on_key_up(Key::r);
	keyboard.on_key_up(Key::t);
	keyboard.poll();
	CHECK(row.update_capture(keyboard.state(), keyboard.previous_state()).kind == CaptureKind::none);
	CHECK_FALSE(row.waiting_for_release());
	keyboard.on_key_down(Key::t);
	keyboard.poll();
	const components::CaptureStep selected = row.update_capture(keyboard.state(), keyboard.previous_state());
	CHECK(selected.kind == CaptureKind::key);
	CHECK(selected.key == Key::t);
	CHECK(row.capturing());
	CHECK(row.waiting_for_release());
	keyboard.poll();
	CHECK(row.update_capture(keyboard.state(), keyboard.previous_state()).kind == CaptureKind::none);
}

TEST_CASE("binding capture refuses simultaneous candidates and waits for a fresh neutral snapshot")
{
	TestContent content;
	BindingRow row("bind", L"Rebind", L"Restart: r", Vector2F::zero, 200.0f, "font", &content.resources);
	Keyboard keyboard;
	keyboard.set_focused(true);
	keyboard.poll();
	row.begin_capture();
	row.update_capture(keyboard.state(), keyboard.previous_state());
	keyboard.on_key_down(Key::a);
	keyboard.on_key_down(Key::b);
	keyboard.poll();
	CHECK(row.update_capture(keyboard.state(), keyboard.previous_state()).kind == CaptureKind::multiple);
	CHECK(row.capturing());
	keyboard.on_key_up(Key::a);
	keyboard.poll();
	CHECK(row.update_capture(keyboard.state(), keyboard.previous_state()).kind == CaptureKind::none);
	CHECK(row.waiting_for_release());
	keyboard.on_key_up(Key::b);
	keyboard.poll();
	row.update_capture(keyboard.state(), keyboard.previous_state());
	CHECK_FALSE(row.waiting_for_release());
}

TEST_CASE("binding capture cancels on focus loss and restores the page supplied value")
{
	TestContent content;
	BindingRow row("bind", L"Rebind", L"Restart: r", Vector2F::zero, 200.0f, "font", &content.resources);
	KeyboardState now;
	now.focused = true;
	row.begin_capture();
	row.update_capture(now, now);
	row.set_value(L"Restart: t");
	CHECK(row.value().text() == L"Press one key");
	now.focused = false;
	CHECK(row.update_capture(now, now).kind == CaptureKind::focus_lost);
	CHECK_FALSE(row.capturing());
	CHECK(row.value().text() == L"Restart: t");
	row.begin_capture();
	row.cancel_capture();
	CHECK(row.value().text() == L"Restart: t");
}
