#pragma once

// What startup_child.cpp's exit code says about its window, read back by
// startup_info_tests.cpp.
constexpr int startup_child_visible = 10;
constexpr int startup_child_hidden = 20;
constexpr int startup_child_failed = 30;
