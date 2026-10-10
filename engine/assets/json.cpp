#include "engine/assets/json.h"
#include "engine/core/file_path.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace labrador
{
	struct JsonDocument::Impl
	{
		rapidjson::Document document;
		std::string source_path;
	};

	namespace
	{
		// Big enough that every asset in the project is read in one or two calls,
		// small enough to sit comfortably on the stack.
		constexpr size_t read_chunk_size = 64 * 1024;

		// The one place rapidjson is recovered from the void* that keeps it out
		// of json.h. The pointer always came from a rapidjson::Value in this
		// file, so the cast is a formality rather than a bet.
		const rapidjson::Value& value_of(const void* value)
		{
			return *static_cast<const rapidjson::Value*>(value);
		}
	}

	JsonDocument read_json_file(const char* path)
	{
		if (path == nullptr)
		{
			throw std::invalid_argument("read_json_file - null path");
		}

		// A path, not a narrow string, so the UTF-8 name reaches the platform
		// as UTF-8 wherever narrow is not already that - wide on Windows.
		std::ifstream file(path_from_utf8(path), std::ios::binary);
		if (!file)
		{
			throw std::runtime_error(
				std::string("read_json_file - cannot open '") + path + "'");
		}

		std::string text;
		char chunk[read_chunk_size];
		while (file.read(chunk, sizeof(chunk)) || file.gcount() > 0)
		{
			text.append(chunk, static_cast<size_t>(file.gcount()));
		}
		if (file.bad())
		{
			throw std::runtime_error(
				std::string("read_json_file - read error in '") + path + "'");
		}

		auto impl = std::make_unique<JsonDocument::Impl>();
		impl->source_path = path;
		impl->document.Parse(text.c_str(), text.size());
		if (impl->document.HasParseError())
		{
			throw std::runtime_error(
				std::string("read_json_file - '") + path +
				"' is not valid JSON: " +
				rapidjson::GetParseError_En(impl->document.GetParseError()) +
				" (offset " + std::to_string(impl->document.GetErrorOffset()) +
				")");
		}
		return JsonDocument(std::move(impl));
	}

	JsonValue::JsonValue(const void* value, const std::string* source_path,
		std::string context) :
		value_(value),
		source_path_(source_path),
		context_(std::move(context))
	{
	}

	std::string JsonValue::where() const
	{
		return "'" + *this->source_path_ + "': " +
			(this->context_.empty() ? std::string("the document") :
				this->context_);
	}

	void JsonValue::fail(const std::string& what) const
	{
		throw std::runtime_error(this->where() + " " + what);
	}

	JsonValue JsonValue::child(const char* key) const
	{
		const rapidjson::Value& value = value_of(this->value_);
		if (!value.IsObject())
		{
			this->fail("is not an object");
		}

		const auto member = value.FindMember(key);
		if (member == value.MemberEnd())
		{
			this->fail(std::string("has no '") + key + "'");
		}

		return JsonValue(&member->value, this->source_path_,
			this->context_.empty() ? std::string(key) :
				this->context_ + "." + key);
	}

	JsonValue JsonValue::element(size_t index) const
	{
		const rapidjson::Value& value = value_of(this->value_);
		return JsonValue(&value[static_cast<rapidjson::SizeType>(index)],
			this->source_path_,
			this->context_ + "[" + std::to_string(index) + "]");
	}

	JsonValue JsonValue::object(const char* key) const
	{
		const JsonValue value = this->child(key);
		if (!value_of(value.value_).IsObject())
		{
			value.fail("is not an object");
		}
		return value;
	}

	JsonValue JsonValue::array(const char* key) const
	{
		const JsonValue value = this->child(key);
		if (!value_of(value.value_).IsArray())
		{
			value.fail("is not an array");
		}
		return value;
	}

	std::string JsonValue::string(const char* key) const
	{
		const JsonValue value = this->child(key);
		const rapidjson::Value& raw = value_of(value.value_);
		if (!raw.IsString())
		{
			value.fail("is not a string");
		}
		return std::string(raw.GetString(), raw.GetStringLength());
	}

	std::string JsonValue::identifier(const char* key) const
	{
		return this->child(key).as_identifier();
	}

	std::string JsonValue::as_identifier() const
	{
		const std::string value = this->as_string();
		if (value.find('\0') != std::string::npos)
		{
			this->fail("contains an embedded NUL in an identifier or path");
		}
		return value;
	}

	int JsonValue::integer(const char* key) const
	{
		const JsonValue value = this->child(key);
		const rapidjson::Value& raw = value_of(value.value_);
		if (!raw.IsInt())
		{
			value.fail("is not a whole number");
		}
		return raw.GetInt();
	}

	// Any JSON number, not only one written with a decimal point: `0` and `0.0`
	// mean the same thing to a person editing a level file, and IsFloat() is
	// false for the first of them.
	float JsonValue::number(const char* key) const
	{
		const JsonValue value = this->child(key);
		const rapidjson::Value& raw = value_of(value.value_);
		if (!raw.IsNumber())
		{
			value.fail("is not a number");
		}
		return raw.GetFloat();
	}

	bool JsonValue::boolean(const char* key) const
	{
		const JsonValue value = this->child(key);
		const rapidjson::Value& raw = value_of(value.value_);
		if (!raw.IsBool())
		{
			value.fail("is not true or false");
		}
		return raw.GetBool();
	}

	bool JsonValue::has(const char* key) const
	{
		const rapidjson::Value& value = value_of(this->value_);
		if (!value.IsObject())
		{
			this->fail("is not an object");
		}
		return value.FindMember(key) != value.MemberEnd();
	}

	size_t JsonValue::size() const
	{
		const rapidjson::Value& value = value_of(this->value_);
		if (!value.IsArray())
		{
			this->fail("is not an array");
		}
		return value.Size();
	}

	JsonValue JsonValue::at(size_t index) const
	{
		const rapidjson::Value& value = value_of(this->value_);
		if (!value.IsArray())
		{
			this->fail("is not an array");
		}
		if (index >= value.Size())
		{
			this->fail("has no element " + std::to_string(index));
		}
		return this->element(index);
	}

	std::string JsonValue::as_string() const
	{
		const rapidjson::Value& value = value_of(this->value_);
		if (!value.IsString())
		{
			this->fail("is not a string");
		}
		return std::string(value.GetString(), value.GetStringLength());
	}

	JsonDocument::JsonDocument(std::unique_ptr<Impl> impl) :
		impl_(std::move(impl))
	{
	}

	JsonDocument::~JsonDocument() = default;
	JsonDocument::JsonDocument(JsonDocument&&) noexcept = default;
	JsonDocument& JsonDocument::operator=(JsonDocument&&) noexcept = default;

	JsonValue JsonDocument::root() const
	{
		// Upcast to the base before the pointer becomes a void*: Document is a
		// GenericValue, but only the compiler may say by how much.
		const rapidjson::Value* root = &this->impl_->document;
		return JsonValue(root, &this->impl_->source_path, std::string());
	}

	const std::string& JsonDocument::source_path() const
	{
		return this->impl_->source_path;
	}
}
