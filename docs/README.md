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
