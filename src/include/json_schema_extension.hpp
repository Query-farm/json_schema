#pragma once

#include "duckdb.hpp"

//! Extension version (also reported to telemetry), format YYYYMMDDNN
#define JSON_SCHEMA_EXTENSION_VERSION "2026110701"

namespace duckdb {

class JsonSchemaExtension : public Extension {
public:
	void Load(ExtensionLoader &db) override;
	std::string Name() override;
	std::string Version() const override;
};

} // namespace duckdb
