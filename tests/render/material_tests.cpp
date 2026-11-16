#include <doctest/doctest.h>

#include "engine/render/material.h"
#include "engine/render/material_table.h"
#include "engine/render/renderer.h"

#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace labrador;

namespace
{
	MaterialSource null_material(std::string_view name = "test_material")
	{
		return { MaterialKind::null, {}, name };
	}
}

TEST_CASE("MATERIAL: registration needs a device on every backend")
{
	Renderer renderer;
	CHECK_THROWS_AS(std::ignore = renderer.add_material(null_material()),
		std::logic_error);
}

TEST_CASE("MATERIAL: a table owns its source and its handles")
{
	MaterialTable table;
	MaterialTable other;
	std::string name = "copied_material";
	std::vector<unsigned char> text = { 'v', 'o', 'i', 'd', ' ', 'm', 'a',
		'i', 'n', '(', ')', '{', '}', '\0' };
	const MaterialSource source{ MaterialKind::glsl, text, name };
	const MaterialHandle handle = table.add(source, MaterialKind::glsl,
		true, false);
	REQUIRE(handle.valid());
	CHECK(table.size() == 1);
	CHECK(handle.index() == 0);
	name.assign("changed_name");
	text[0] = 'x';
	CHECK(table.source(0).name == "copied_material");
	CHECK(table.source(0).bytes[0] == 'v');

	const MaterialHandle second = other.add(null_material(), MaterialKind::null,
		true, false);
	CHECK(handle != second);
	CHECK_THROWS_AS(table.select(second, false), std::invalid_argument);
	CHECK_THROWS_AS(table.select(MaterialHandle{}, false),
		std::invalid_argument);
	CHECK_FALSE(table.has_selection());
	table.select(handle, false);
	CHECK(table.selected() == handle);
}

TEST_CASE("MATERIAL: expired handles stay foreign when a table reuses storage")
{
	alignas(MaterialTable) unsigned char storage[sizeof(MaterialTable)];
	using TableOwner = std::unique_ptr<MaterialTable, void(*)(MaterialTable*)>;
	MaterialHandle expired;
	{
		const TableOwner original(
			std::construct_at(reinterpret_cast<MaterialTable*>(storage)),
			std::destroy_at<MaterialTable>);
		expired = original->add(null_material("old"), MaterialKind::null,
			true, false);
	}
	const TableOwner replacement(
		std::construct_at(reinterpret_cast<MaterialTable*>(storage)),
		std::destroy_at<MaterialTable>);
	const MaterialHandle current = replacement->add(null_material("new"),
		MaterialKind::null, true, false);
	CHECK(expired.index() == current.index());
	CHECK(expired != current);
	CHECK_THROWS_AS(replacement->select(expired, false), std::invalid_argument);
	CHECK_THROWS_AS(replacement->set_values(expired, PostValues{}),
		std::invalid_argument);
	CHECK_FALSE(replacement->has_selection());
	replacement->select(current, false);
	CHECK(replacement->selected() == current);
}

TEST_CASE("MATERIAL: refused registration leaves an existing selection intact")
{
	MaterialTable table;
	const MaterialHandle handle = table.add(null_material(), MaterialKind::null,
		true, false);
	table.select(handle, false);
	CHECK_THROWS_AS(std::ignore = table.add(null_material(), MaterialKind::null,
		false, false), std::logic_error);
	CHECK_THROWS_AS(std::ignore = table.add(null_material(), MaterialKind::null,
		true, true), std::logic_error);
	CHECK_THROWS_AS(std::ignore = table.add(null_material(""),
		MaterialKind::null, true, false), std::invalid_argument);
	CHECK_THROWS_AS(std::ignore = table.add(null_material(), MaterialKind::glsl,
		true, false), std::invalid_argument);
	CHECK(table.size() == 1);
	CHECK(table.selected() == handle);
	CHECK_THROWS_AS(table.select(handle, true), std::logic_error);
	CHECK(table.selected() == handle);
}

TEST_CASE("MATERIAL: source kind checks happen before backend shader creation")
{
	MaterialTable table;
	const unsigned char bad_bytes[] = { 'b', 'a', 'd', '\0' };
	for (const MaterialKind kind : { MaterialKind::d3d11, MaterialKind::d3d12,
		MaterialKind::spirv, MaterialKind::glsl })
	{
		CAPTURE(static_cast<int>(kind));
		CHECK_THROWS_AS(std::ignore = table.add({ kind, {}, "empty" }, kind,
			true, false), std::invalid_argument);
		if (kind != MaterialKind::glsl)
		{
			CHECK_THROWS_AS(std::ignore = table.add(
				{ kind, bad_bytes, "bad_magic" }, kind, true, false),
				std::invalid_argument);
		}
	}
	const unsigned char no_terminator[] = { 'm', 'a', 'i', 'n' };
	const unsigned char embedded_terminator[] = { 'a', '\0', 'b', '\0' };
	CHECK_THROWS_AS(std::ignore = table.add(
		{ MaterialKind::glsl, no_terminator, "unterminated" },
		MaterialKind::glsl, true, false), std::invalid_argument);
	CHECK_THROWS_AS(std::ignore = table.add(
		{ MaterialKind::glsl, embedded_terminator, "embedded_null" },
		MaterialKind::glsl, true, false), std::invalid_argument);
	CHECK(table.size() == 0);
}

TEST_CASE("MATERIAL: values belong to a material and survive deselection")
{
	MaterialTable table;
	const MaterialHandle first = table.add(null_material("first"),
		MaterialKind::null, true, false);
	const MaterialHandle second = table.add(null_material("second"),
		MaterialKind::null, true, false);
	PostValues values;
	values.values[0] = 0.25f;
	CHECK_THROWS_AS(table.set_values(first, values), std::invalid_argument);
	table.select(first, false);
	table.set_values(first, values);
	table.select(second, false);
	CHECK(table.values().values[0] == 0.0f);
	CHECK_THROWS_AS(table.set_values(first, values), std::invalid_argument);
	table.clear_selection();
	CHECK_FALSE(table.has_selection());
	table.select(first, false);
	CHECK(table.values().values[0] == 0.25f);

	for (const float invalid : { std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity() })
	{
		PostValues bad = values;
		bad.values[31] = invalid;
		CHECK_THROWS_AS(table.set_values(first, bad), std::invalid_argument);
		CHECK(table.values().values[0] == 0.25f);
		CHECK(table.values().values[31] == 0.0f);
	}
}

TEST_CASE("MATERIAL: failed backend creation can roll back the last registration")
{
	MaterialTable table;
	const MaterialHandle kept = table.add(null_material("kept"),
		MaterialKind::null, true, false);
	table.select(kept, false);
	std::ignore = table.add(null_material("failed"), MaterialKind::null,
		true, false);
	table.remove_last();
	CHECK(table.size() == 1);
	CHECK(table.source(0).name == "kept");
	CHECK(table.selected() == kept);
}
