#include "engine/collision/contacts.h"
#include "engine/collision/broad_phase.h"
#include "engine/collision/collision_object.h"
#include "engine/collision/narrow_phase.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/scene/scene.h"
#include <cstdio>
#include <memory>
#include <vector>

namespace {
class Collider : public labrador::CollisionObject {
public:
    explicit Collider(std::unique_ptr<mattmath::Shape> geometry, bool* destroyed = nullptr)
        : geometry_(std::move(geometry)), destroyed_(destroyed) {}
    ~Collider() override { if (destroyed_) *destroyed_ = true; }
    void update(float) override {}
    void draw(labrador::DrawList&) const override {}
    mattmath::RectangleF bounds() const override { return geometry_->bounding_box(); }
    const mattmath::Shape* shape() const override { return geometry_.get(); }
    labrador::CollisionLayer layer() const override { return 1; }
    labrador::CollisionMask mask() const override { return 1; }
    labrador::CollisionTag tag() const override { return 0; }
    bool for_deletion() const override { return retire_; }
    void set_for_deletion(bool retire) override { retire_ = retire; }
    void on_contact(const labrador::CollisionObject&, const mattmath::Vector2F&, float) override {}
private:
    std::unique_ptr<mattmath::Shape> geometry_;
    bool* destroyed_;
    bool retire_ = false;
};
}
int main() {
    bool destroyed = false;
    labrador::Scene scene(nullptr, nullptr);
    Collider* doomed = scene.add(std::make_unique<Collider>(
        std::make_unique<mattmath::RectangleF>(0.0f, 0.0f, 10.0f, 10.0f), &destroyed));
    scene.add(std::make_unique<Collider>(
        std::make_unique<mattmath::RectangleF>(8.0f, 0.0f, 10.0f, 10.0f)));
    scene.end_tick();
    scene.resolve();
    std::printf("contacts before retirement=%zu\n", scene.contacts().size());
    doomed->set_for_deletion(true);
    scene.end_tick();
    std::printf("doomed destroyed=%d; contacts after retirement=%zu\n",
        destroyed ? 1 : 0, scene.contacts().size());
    const mattmath::Vector2F x_axis(0.70710678f, 0.70710678f);
    const mattmath::Vector2F y_axis(-0.70710678f, 0.70710678f);
    const mattmath::Vector2F half_extents(0.0001f, 1.0f);
    Collider first(std::make_unique<mattmath::RectangleRotated>(
        mattmath::Vector2F(0.0f, 0.0f), x_axis, y_axis, half_extents));
    Collider second(std::make_unique<mattmath::RectangleRotated>(
        y_axis * 2.0001f, x_axis, y_axis, half_extents));
    std::vector<labrador::CollisionObject*> objects{ &first, &second };
    labrador::BroadPhase broad;
    std::vector<labrador::Contact> contacts;
    labrador::find_contacts(objects, contacts, &broad);
    std::printf("diagonal thin OBBs: boxes overlap=%d; positive longitudinal gap=%g; full contacts=%zu\n",
        first.bounds().intersects(second.bounds()) ? 1 : 0,
        static_cast<double>(mattmath::Vector2F::dot(second.shape()->center() - first.shape()->center(), y_axis) - 2.0f),
        contacts.size());
}
