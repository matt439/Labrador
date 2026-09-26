#include "engine/core/state_context.h"
#include "engine/core/name_table.h"
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace {
struct PlainState : labrador::State {
    void init() override {}
    void update(float) override {}
    void draw(labrador::Renderer&) const override {}
};
struct ReentrantState : PlainState {
    bool* destroyed;
    explicit ReentrantState(bool* flag) : destroyed(flag) {}
    ~ReentrantState() override { *destroyed = true; std::puts("old state destroyed"); }
    void on_deactivated() override { context()->transition_to(std::make_unique<PlainState>()); }
    void update(float) override {
        // Copy everything before reentry: observe early destruction without reading freed memory.
        bool* flag = destroyed;
        labrador::StateContext* owner = context();
        std::puts("update enters");
        owner->notify_activation(false);
        std::printf("update still running; state already destroyed=%d\n", *flag ? 1 : 0);
    }
};
struct ThrowsOnMove {
    int value;
    explicit ThrowsOnMove(int number) : value(number) {}
    ThrowsOnMove(const ThrowsOnMove&) = default;
    ThrowsOnMove(ThrowsOnMove&& other) : value(other.value) {
        if (value == 1) throw std::runtime_error("move failure");
    }
    ThrowsOnMove& operator=(ThrowsOnMove&&) = default;
};
}
int main() {
    bool destroyed = false;
    labrador::StateContext owner;
    owner.transition_to(std::make_unique<ReentrantState>(&destroyed));
    owner.update(0.0f);
    labrador::NameTable<ThrowsOnMove> table("test");
    try { table.add("failed", ThrowsOnMove(1)); }
    catch (const std::exception& failure) { std::printf("insert threw: %s\n", failure.what()); }
    table.add("other", ThrowsOnMove(2));
    std::printf("failed name present=%d; failed resolves to other's value=%d\n",
        table.contains("failed") ? 1 : 0, table.get(table.resolve("failed")).value);
}
