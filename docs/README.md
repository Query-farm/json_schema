<p align="center">
  <a href="https://query.farm">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="https://query.farm/media-kit/logo/wordmark-dark.svg">
      <img alt="Query.Farm" src="https://query.farm/media-kit/logo/wordmark-light.svg" height="64">
    </picture>
  </a>
</p>

# DuckDB JSON Schema Extension by [Query.Farm](https://query.farm)

[![DuckDB](https://img.shields.io/badge/DuckDB-community_extension-fdf1e0?logo=duckdb&logoColor=fff000)](https://duckdb.org/community_extensions/extensions/json_schema.html)
[![v1.5 build](https://github.com/Query-farm/json_schema/actions/workflows/MainDistributionPipeline.yml/badge.svg?branch=v1.5)](https://github.com/Query-farm/json_schema/actions/workflows/MainDistributionPipeline.yml?query=branch%3Av1.5)

The **JSON Schema** extension, developed by **[Query.Farm](https://query.farm)**, brings powerful JSON schema validation and data enrichment capabilities directly to your SQL queries in DuckDB. Validate JSON data against schemas, ensure data integrity, and automatically apply default values using the industry-standard JSON Schema specification without leaving your database environment.

## Documentation

Full documentation, including installation, usage, the function reference, and cookbook examples, is available at:

**[https://query.farm/products/extensions/json_schema](https://query.farm/products/extensions/json_schema)**

## Installation

```sql
INSTALL json_schema FROM community;
LOAD json_schema;
```

## Validation errors and boolean filters

Both validation functions return `TRUE` on success and raise an error on failure.
They do not return `FALSE` for a nonconforming document or invalid schema. Validate
the schema separately before using `TRY` to classify rows, because `TRY` also catches
schema errors. This example treats errors and SQL `NULL` as invalid:

```sql
SELECT json_schema_validate_schema('{"type":"integer"}');
SELECT payload,
       COALESCE(TRY(json_schema_validate('{"type":"integer"}', payload)), false) AS valid
FROM (VALUES ('3'::JSON), ('"five"'::JSON), (NULL::JSON)) AS input(payload);
```
