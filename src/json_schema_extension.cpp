#define DUCKDB_EXTENSION_MAIN

#include "json_schema_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include "nlohmann/json-schema.hpp"

namespace duckdb {

inline void JsonSchemaValidateSchemaFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &schema_vector = args.data[0];

	nlohmann::json_schema::json_validator validator;
	UnaryExecutor::Execute<string_t, bool>(schema_vector, result, args.size(), [&](string_t schema) {
		try {
			nlohmann::json parsed = nlohmann::json::parse(schema.GetString());
			validator.set_root_schema(parsed);
		} catch (const std::exception &e) {
			throw InvalidInputException("Validation of JSON schema failed: " + string(e.what()) +
			                            "\nSchema: " + schema.GetString());
		}
		return true;
	});
}

inline void JsonSchemaValidateFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &schema_vector = args.data[0];
	auto &value_vector = args.data[1];

	BinaryExecutor::Execute<string_t, string_t, bool>(
	    schema_vector, value_vector, result, args.size(), [&](string_t schema, string_t value) {
		    try {
			    nlohmann::json_schema::json_validator validator; // create validator
			    nlohmann::json parsed_schema = nlohmann::json::parse(schema.GetString());

			    validator.set_root_schema(parsed_schema);
			    nlohmann::json parsed_value = nlohmann::json::parse(value.GetString());
			    validator.validate(parsed_value);
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Validation of JSON schema failed, here is why: " + string(e.what()) +
			                                "\nValue: " + value.GetString());
		    }
		    return true;
	    });
}

inline void JsonSchemaPatchFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &schema_vector = args.data[0];
	auto &value_vector = args.data[1];

	BinaryExecutor::Execute<string_t, string_t, string_t>(
	    schema_vector, value_vector, result, args.size(), [&](string_t schema, string_t value) {
		    try {
			    nlohmann::json_schema::json_validator validator; // create validator
			    nlohmann::json parsed_schema = nlohmann::json::parse(schema.GetString());
			    validator.set_root_schema(parsed_schema);

			    nlohmann::json parsed_value = nlohmann::json::parse(value.GetString());
			    auto patch = validator.validate(parsed_value);
			    return StringVector::AddString(result, patch.dump());
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Validation of JSON schema failed, here is why: " + string(e.what()) +
			                                "\nValue: " + value.GetString());
		    }
	    });
}

inline void JsonSchemaUpdateFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &schema_vector = args.data[0];
	auto &value_vector = args.data[1];

	BinaryExecutor::Execute<string_t, string_t, string_t>(
	    schema_vector, value_vector, result, args.size(), [&](string_t schema, string_t value) {
		    try {
			    nlohmann::json_schema::json_validator validator; // create validator
			    nlohmann::json parsed_schema = nlohmann::json::parse(schema.GetString());
			    validator.set_root_schema(parsed_schema);

			    nlohmann::json parsed_value = nlohmann::json::parse(value.GetString());
			    auto patch = validator.validate(parsed_value);
			    auto updated = parsed_value.patch(patch);
			    return StringVector::AddString(result, updated.dump());
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Validation of JSON schema failed, here is why: " + string(e.what()) +
			                                "\nValue: " + value.GetString());
		    }
	    });
}

static void LoadInternal(ExtensionLoader &loader) {
	loader.RegisterFunction(ScalarFunction("json_schema_validate_schema", {LogicalType::JSON()}, LogicalType::BOOLEAN,
	                                       JsonSchemaValidateSchemaFun));

	loader.RegisterFunction(ScalarFunction("json_schema_validate", {LogicalType::JSON(), LogicalType::JSON()},
	                                       LogicalType::BOOLEAN, JsonSchemaValidateFun));

	loader.RegisterFunction(ScalarFunction("json_schema_patch", {LogicalType::JSON(), LogicalType::JSON()},
	                                       LogicalType::JSON(), JsonSchemaPatchFun));

	loader.RegisterFunction(ScalarFunction("json_schema_update", {LogicalType::JSON(), LogicalType::JSON()},
	                                       LogicalType::JSON(), JsonSchemaUpdateFun));
}

void JsonSchemaExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string JsonSchemaExtension::Name() {
	return "json_schema";
}

std::string JsonSchemaExtension::Version() const {
	return "2025102401";
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(json_schema, loader) {
	duckdb::LoadInternal(loader);
}
}
