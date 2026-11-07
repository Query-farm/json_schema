#define DUCKDB_EXTENSION_MAIN

#include "json_schema_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/execution/expression_executor_state.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/string_vector.hpp"
#include "query_farm_telemetry.hpp"

#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonpatch/jsonpatch.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

#include <cstdint>
#include <list>
#include <unordered_map>

#if defined(_WIN32)
#include "duckdb/common/windows.hpp"
#elif defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#endif

namespace duckdb {

namespace jsonschema = jsoncons::jsonschema;
using json = jsoncons::json;
using compiled_schema_t = jsonschema::json_schema<json>;

//! Schemas and documents nested deeper than this are rejected. Parsing is iterative, but compiling and validating
//! recurse once per level (bounded by the native stack guard below, which may stop earlier on small stacks).
static constexpr int MAX_NESTING_DEPTH = 256;
//! Compiled schemas cached per thread (an LRU of the most recently used schema texts)
static constexpr idx_t SCHEMA_CACHE_CAPACITY = 16;
//! json_schema_validate() lists at most this many validation errors in its error message
static constexpr idx_t MAX_ERRORS_IN_MESSAGE = 5;
//! Longest validation message (in bytes) returned; messages can quote instance strings
static constexpr idx_t MAX_MESSAGE_LENGTH = 512;

//! A positional-only parameter with a real name (bare types would be reported as col0, col1, ...)
static FunctionParameter Positional(const char *name, LogicalType type) {
	return FunctionParameter(name, std::move(type), {}, FunctionParameterKind::POSITIONAL_ONLY);
}

//! Truncate to at most max_bytes without splitting a UTF-8 sequence
static string Truncate(const string &text, idx_t max_bytes = MAX_MESSAGE_LENGTH) {
	if (text.size() <= max_bytes) {
		return text;
	}
	idx_t end = max_bytes;
	while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
		end--;
	}
	return text.substr(0, end) + "...";
}

//===--------------------------------------------------------------------===//
// Native stack guard
//===--------------------------------------------------------------------===//
// Compiling and evaluating a schema recurse once per level of schema/document nesting and per $ref hop, and the
// thread stacks DuckDB runs on differ widely (macOS worker threads: 512KB, Linux: 8MB, musl: 128KB). The patched
// jsoncons (vcpkg_ports/jsoncons) checks jsonschema::stack_guard::limit() on every recursion step and throws
// instead of overflowing the stack.

//! Lowest usable address of the current thread's stack, or 0 if unknown
static uintptr_t CurrentThreadStackLow() {
#if defined(_WIN32)
	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery(&mbi, &mbi, sizeof(mbi)) == 0) {
		return 0;
	}
	// The allocation base is the bottom of the reserved stack region; leave room for the guard pages
	return reinterpret_cast<uintptr_t>(mbi.AllocationBase) + 64 * 1024;
#elif defined(__APPLE__)
	pthread_t self = pthread_self();
	auto top = reinterpret_cast<uintptr_t>(pthread_get_stackaddr_np(self));
	auto size = static_cast<uintptr_t>(pthread_get_stacksize_np(self));
	return top > size ? top - size : 0;
#elif defined(__linux__)
	pthread_attr_t attr;
	if (pthread_getattr_np(pthread_self(), &attr) != 0) {
		return 0;
	}
	void *addr = nullptr;
	size_t size = 0;
	auto rc = pthread_attr_getstack(&attr, &addr, &size);
	pthread_attr_destroy(&attr);
	if (rc != 0) {
		return 0;
	}
	return reinterpret_cast<uintptr_t>(addr);
#else
	// Unknown platform (including wasm): fall back to a fixed budget below the current stack pointer
	return 0;
#endif
}

static uintptr_t CurrentStackPointer() {
	volatile char marker = 0;
	return reinterpret_cast<uintptr_t>(&marker);
}

//! The lowest stack address a compilation/validation started here may reach before it is aborted
static uintptr_t ComputeStackLimit() {
	static constexpr uintptr_t FALLBACK_BUDGET = 256 * 1024;
	static constexpr uintptr_t MIN_MARGIN = 64 * 1024;
	static constexpr uintptr_t MIN_BUDGET = 16 * 1024;

	// Querying the stack bounds can be expensive (glibc reads /proc/self/maps for the main thread), so cache it
	static thread_local uintptr_t stack_low = 0;
	static thread_local bool stack_low_known = false;
	if (!stack_low_known) {
		stack_low = CurrentThreadStackLow();
		stack_low_known = true;
	}
	auto sp = CurrentStackPointer();
	if (stack_low == 0 || sp <= stack_low) {
		return sp > FALLBACK_BUDGET ? sp - FALLBACK_BUDGET : 1;
	}
	uintptr_t available = sp - stack_low;
	// Keep a margin for the native frames pushed between two checks and for unwinding/error reporting
	uintptr_t margin = MaxValue<uintptr_t>(MIN_MARGIN, available / 4);
	if (available <= margin + MIN_BUDGET) {
		return sp - MinValue<uintptr_t>(available, MIN_BUDGET);
	}
	return stack_low + margin;
}

//! Arms the jsoncons stack guard for the duration of a compilation or validation
class StackScope {
public:
	StackScope() : previous(jsonschema::stack_guard::limit()) {
		if (previous == 0) {
			jsonschema::stack_guard::limit() = ComputeStackLimit();
		}
	}
	~StackScope() {
		jsonschema::stack_guard::limit() = previous;
	}

private:
	uintptr_t previous;
};

static json ParseJson(string_t text) {
	jsoncons::json_options options;
	options.max_nesting_depth(MAX_NESTING_DEPTH);
	return json::parse(jsoncons::string_view(text.GetData(), text.GetSize()), options);
}

static unique_ptr<compiled_schema_t> CompileSchema(string_t schema_text) {
	json schema_json;
	try {
		schema_json = ParseJson(schema_text);
	} catch (const std::exception &e) {
		throw InvalidInputException("Invalid JSON schema: the schema is not valid JSON: %s", Truncate(e.what()));
	}
	try {
		jsonschema::evaluation_options options;
		// Validate "format" (date-time, email, ipv4, regex, ...) rather than treating it as an annotation
		options.require_format_validation(true);
		// Schemas without "$schema" are interpreted as the current specification
		options.default_version(jsonschema::schema_version::draft202012());
		// No resolver is installed: only the built-in meta-schemas (draft 4, 6, 7, 2019-09 and 2020-12) can be
		// referenced by URI. Remote and file $refs are refused, so a schema never makes I/O.
		StackScope stack_scope;
		return make_uniq<compiled_schema_t>(jsonschema::make_json_schema(std::move(schema_json), options));
	} catch (const std::exception &e) {
		throw InvalidInputException("Invalid JSON schema: %s", Truncate(e.what()));
	}
}

struct ValidationError {
	string path;
	string keyword;
	string message;
};

//! Collects (up to a limit of) validation errors
struct ErrorCollector {
	explicit ErrorCollector(idx_t limit_p = NumericLimits<idx_t>::Maximum()) : limit(limit_p) {
	}
	jsonschema::walk_state operator()(const jsonschema::validation_message &msg) {
		errors.push_back(
		    ValidationError {msg.instance_location().to_string(), msg.keyword(), Truncate(msg.message())});
		return errors.size() >= limit ? jsonschema::walk_state::abort : jsonschema::walk_state::advance;
	}
	idx_t limit;
	vector<ValidationError> errors;
};

[[noreturn]] static void ThrowValidationErrors(const vector<ValidationError> &errors) {
	string message = "JSON document does not match the schema: ";
	for (idx_t i = 0; i < errors.size(); i++) {
		if (i > 0) {
			message += "; ";
		}
		auto &err = errors[i];
		message += (err.path.empty() ? string("(root)") : err.path) + ": " + err.message;
	}
	if (errors.size() >= MAX_ERRORS_IN_MESSAGE) {
		message += "; ... (use json_schema_errors() for the full list)";
	}
	throw InvalidInputException(message);
}

static json ParseDocument(string_t document) {
	try {
		return ParseJson(document);
	} catch (const std::exception &e) {
		throw InvalidInputException("Invalid JSON document: %s", Truncate(e.what()));
	}
}

//! Runs a validation, turning errors raised by the validator itself (such as the stack guard) into DuckDB errors
template <class FUNC>
static auto RunValidator(FUNC &&func) -> decltype(func()) {
	try {
		StackScope stack_scope;
		return func();
	} catch (const duckdb::Exception &) {
		throw;
	} catch (const std::exception &e) {
		throw InvalidInputException("JSON schema validation failed: %s", Truncate(e.what()));
	}
}

//===--------------------------------------------------------------------===//
// Per-thread compiled schema cache
//===--------------------------------------------------------------------===//
struct JsonSchemaLocalState : public FunctionLocalState {
	compiled_schema_t &GetSchema(string_t schema_text) {
		// Fast path: the most recently used schema (always the case for a constant schema argument)
		if (!entries.empty() && string_t(entries.front().first) == schema_text) {
			return *entries.front().second;
		}
		string key = schema_text.GetString();
		auto it = index.find(key);
		if (it != index.end()) {
			entries.splice(entries.begin(), entries, it->second);
			return *entries.front().second;
		}
		auto compiled = CompileSchema(schema_text);
		if (entries.size() >= SCHEMA_CACHE_CAPACITY) {
			index.erase(entries.back().first);
			entries.pop_back();
		}
		entries.emplace_front(std::move(key), std::move(compiled));
		index[entries.front().first] = entries.begin();
		return *entries.front().second;
	}

private:
	using Entry = std::pair<string, unique_ptr<compiled_schema_t>>;
	std::list<Entry> entries;
	std::unordered_map<string, std::list<Entry>::iterator> index;
};

static unique_ptr<FunctionLocalState> JsonSchemaInitLocalState(ExpressionState &, const BoundFunctionExpression &,
                                                               FunctionData *) {
	return make_uniq<JsonSchemaLocalState>();
}

static JsonSchemaLocalState &GetLocalState(ExpressionState &state) {
	return ExecuteFunctionState::GetFunctionState(state)->Cast<JsonSchemaLocalState>();
}

//===--------------------------------------------------------------------===//
// Functions
//===--------------------------------------------------------------------===//
static void JsonSchemaValidateSchemaFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &local = GetLocalState(state);
	UnaryExecutor::Execute<string_t, bool>(args.data[0], result, args.size(), [&](string_t schema) {
		local.GetSchema(schema);
		return true;
	});
}

static void JsonSchemaValidateFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &local = GetLocalState(state);
	BinaryExecutor::Execute<string_t, string_t, bool>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t schema, string_t value) {
		    auto &compiled = local.GetSchema(schema);
		    auto document = ParseDocument(value);
		    ErrorCollector collector(MAX_ERRORS_IN_MESSAGE);
		    RunValidator([&] { compiled.validate(document, collector); });
		    if (!collector.errors.empty()) {
			    ThrowValidationErrors(collector.errors);
		    }
		    return true;
	    });
}

static void JsonSchemaIsValidFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &local = GetLocalState(state);
	BinaryExecutor::Execute<string_t, string_t, bool>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t schema, string_t value) {
		    auto &compiled = local.GetSchema(schema);
		    json document;
		    try {
			    document = ParseJson(value);
		    } catch (const std::exception &) {
			    // A document that is not JSON does not conform to any schema
			    return false;
		    }
		    return RunValidator([&] { return compiled.is_valid(document); });
	    });
}

static void JsonSchemaErrorsFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &local = GetLocalState(state);
	BinaryExecutor::Execute<string_t, string_t, list_entry_t>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t schema, string_t value) {
		    auto &compiled = local.GetSchema(schema);
		    ErrorCollector collector;
		    json document;
		    bool parsed = true;
		    try {
			    document = ParseJson(value);
		    } catch (const std::exception &e) {
			    parsed = false;
			    collector.errors.push_back(ValidationError {"", "", "Invalid JSON document: " + Truncate(e.what())});
		    }
		    if (parsed) {
			    RunValidator([&] { compiled.validate(document, collector); });
		    }
		    list_entry_t entry(ListVector::GetListSize(result), collector.errors.size());
		    for (auto &err : collector.errors) {
			    ListVector::PushBack(result, Value::STRUCT({{"path", Value(err.path)},
			                                                {"keyword", Value(err.keyword)},
			                                                {"message", Value(err.message)}}));
		    }
		    return entry;
	    });
}

//! Validates the document and returns the JSON Patch that inserts the schema's default values
static json ComputeDefaultsPatch(compiled_schema_t &compiled, const json &document) {
	ErrorCollector collector(MAX_ERRORS_IN_MESSAGE);
	json patch(jsoncons::json_array_arg);
	RunValidator([&] { compiled.validate(document, collector, patch); });
	if (!collector.errors.empty()) {
		ThrowValidationErrors(collector.errors);
	}
	return patch;
}

static void JsonSchemaPatchFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &local = GetLocalState(state);
	BinaryExecutor::Execute<string_t, string_t, string_t>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t schema, string_t value) {
		    auto &compiled = local.GetSchema(schema);
		    auto document = ParseDocument(value);
		    auto patch = ComputeDefaultsPatch(compiled, document);
		    return StringVector::AddString(result, patch.to_string());
	    });
}

static void JsonSchemaUpdateFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &local = GetLocalState(state);
	BinaryExecutor::Execute<string_t, string_t, string_t>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t schema, string_t value) {
		    auto &compiled = local.GetSchema(schema);
		    auto document = ParseDocument(value);
		    auto patch = ComputeDefaultsPatch(compiled, document);
		    std::error_code ec;
		    jsoncons::jsonpatch::apply_patch(document, patch, ec);
		    if (ec) {
			    throw InvalidInputException("Could not apply the schema's default values: %s", ec.message());
		    }
		    return StringVector::AddString(result, document.to_string());
	    });
}

//===--------------------------------------------------------------------===//
// Registration
//===--------------------------------------------------------------------===//
static const char *const DRAFT_NOTE =
    " Supports JSON Schema draft 4, 6, 7, 2019-09 and 2020-12 (chosen by \"$schema\"; 2020-12 when absent), "
    "validates \"format\", and refuses remote $refs.";

static void Register(ExtensionLoader &loader, ScalarFunction function, string description, vector<string> examples) {
	// Every function raises an error for an invalid schema, so they must be marked as fallible.
	function.SetFallible();
	function.SetInitStateCallback(JsonSchemaInitLocalState);
	CreateScalarFunctionInfo info(std::move(function));
	FunctionDescription desc;
	desc.description = std::move(description) + DRAFT_NOTE;
	desc.examples = std::move(examples);
	desc.categories = {"json", "validation"};
	info.descriptions.push_back(std::move(desc));
	loader.RegisterFunction(std::move(info));
}

static void LoadInternal(ExtensionLoader &loader) {
	auto schema = Positional("schema", LogicalType::JSON());
	auto json_data = Positional("json_data", LogicalType::JSON());

	Register(loader,
	         ScalarFunction("json_schema_validate_schema", {schema}, LogicalType::BOOLEAN, JsonSchemaValidateSchemaFun),
	         "Checks that a JSON schema is well formed and returns true; raises an error describing the problem "
	         "otherwise.",
	         {"json_schema_validate_schema('{\"type\": \"object\", \"required\": [\"id\"]}')"});

	Register(loader,
	         ScalarFunction("json_schema_validate", {schema, json_data}, LogicalType::BOOLEAN, JsonSchemaValidateFun),
	         "Validates a JSON document against a JSON schema and returns true; raises an error listing the first "
	         "validation errors otherwise. Use json_schema_is_valid() for a true/false result.",
	         {"json_schema_validate('{\"type\": \"object\", \"properties\": {\"id\": {\"type\": \"integer\"}}}', "
	          "'{\"id\": 5}')"});

	Register(loader,
	         ScalarFunction("json_schema_is_valid", {schema, json_data}, LogicalType::BOOLEAN, JsonSchemaIsValidFun),
	         "Returns whether a JSON document conforms to a JSON schema (false for a document that is not valid "
	         "JSON). Raises an error only when the schema itself is invalid.",
	         {"json_schema_is_valid('{\"type\": \"integer\", \"minimum\": 0}', '-1')",
	          "json_schema_is_valid('{\"prefixItems\": [{\"type\": \"string\"}]}', '[\"a\", 1]')"});

	child_list_t<LogicalType> error_fields {{"path", LogicalType::VARCHAR},
	                                        {"keyword", LogicalType::VARCHAR},
	                                        {"message", LogicalType::VARCHAR}};
	Register(loader,
	         ScalarFunction("json_schema_errors", {schema, json_data},
	                        LogicalType::LIST(LogicalType::STRUCT(std::move(error_fields))), JsonSchemaErrorsFun),
	         "Returns every validation error of a JSON document against a JSON schema as a list of "
	         "{path, keyword, message} structs, where path is a JSON Pointer into the document (empty for the "
	         "root). Returns an empty list for a conforming document.",
	         {"json_schema_errors('{\"type\": \"object\", \"required\": [\"id\", \"name\"]}', '{\"id\": 1}')",
	          "json_schema_errors('{\"type\": \"array\", \"items\": {\"type\": \"integer\"}}', '[1, \"two\", 3]')"});

	Register(loader, ScalarFunction("json_schema_patch", {schema, json_data}, LogicalType::JSON(), JsonSchemaPatchFun),
	         "Validates a JSON document against a JSON schema and returns the JSON Patch (RFC 6902) that adds the "
	         "schema's default values for missing properties. Raises an error if the document does not validate.",
	         {"json_schema_patch('{\"properties\": {\"id\": {\"type\": \"integer\", \"default\": 5}}}', "
	          "'{\"name\": \"George\"}')"});

	Register(loader,
	         ScalarFunction("json_schema_update", {schema, json_data}, LogicalType::JSON(), JsonSchemaUpdateFun),
	         "Validates a JSON document against a JSON schema and returns the document with the schema's default "
	         "values filled in for missing properties. Raises an error if the document does not validate.",
	         {"json_schema_update('{\"properties\": {\"id\": {\"type\": \"integer\", \"default\": 5}}}', "
	          "'{\"name\": \"George\"}')"});

	QueryFarmSendTelemetry(loader, "json_schema", JSON_SCHEMA_EXTENSION_VERSION);
}

void JsonSchemaExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string JsonSchemaExtension::Name() {
	return "json_schema";
}

std::string JsonSchemaExtension::Version() const {
	return JSON_SCHEMA_EXTENSION_VERSION;
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(json_schema, loader) {
	duckdb::LoadInternal(loader);
}
}
