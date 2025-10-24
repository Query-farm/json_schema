# DuckDB JSON Schema Extension by [Query.Farm](https://query.farm)

The **JSON Schema** extension, developed by **[Query.Farm](https://query.farm)**, brings powerful JSON schema validation and data enrichment capabilities directly to your SQL queries in DuckDB. Validate JSON data against schemas, ensure data integrity, and automatically apply default values using the industry-standard JSON Schema specification without leaving your database environment.

## Use Cases

The JSON Schema extension is perfect for:
- **Data validation**: Ensure JSON data conforms to predefined schemas and business rules
- **API data validation**: Validate incoming API payloads and responses against contracts
- **Data enrichment**: Automatically apply default values to incomplete JSON documents
- **Schema compliance**: Verify that JSON schemas themselves are valid and well-formed
- **Data quality assurance**: Implement data quality checks in ETL pipelines
- **Configuration validation**: Ensure configuration files meet required specifications
- **Document standardization**: Apply consistent structure and defaults to JSON documents
- **Data governance**: Enforce data contracts and schemas across your organization

## Installation

**`json_schema` is a [DuckDB Community Extension](https://github.com/duckdb/community-extensions).**

You can now use this by using this SQL:

```sql
INSTALL json_schema FROM community;
LOAD json_schema;
```

## Functions

### `json_schema_validate_schema(schema)`
Validates that a JSON schema is well-formed and follows the JSON Schema specification.

**Usage:**
```sql
-- Validate a well-formed schema
SELECT json_schema_validate_schema('{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "User Profile",
  "type": "object",
  "properties": {
    "id": {
      "type": "integer",
      "description": "A unique identifier for the user"
    }
  }
}') as valid;
┌─────────┐
│  valid  │
│ boolean │
├─────────┤
│ true    │
└─────────┘
```

### `json_schema_validate(schema, json_data)`
Validates a JSON value against a JSON schema, returning true if the data conforms to the schema.

**Usage:**
```sql
-- Validate JSON data against a schema
SELECT json_schema_validate('{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "User Profile",
  "type": "object",
  "properties": {
    "id": {
      "type": "integer",
      "description": "A unique identifier for the user"
    }
  }
}', {'id': 5}) as valid;
┌─────────┐
│  valid  │
│ boolean │
├─────────┤
│ true    │
└─────────┘
```

### `json_schema_patch(schema, json_data)`
Calculates the JSON patch operations needed to make the JSON data conform to the schema by adding missing default values.

**Usage:**
```sql
-- Generate patch to add missing default values
SELECT json_schema_patch('{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "User Profile",
  "type": "object",
  "properties": {
    "id": {
      "type": "integer",
      "description": "A unique identifier for the user",
      "default": 5
    }
  }
}', {'name': 'George'}) as patch;
┌───────────────────────────────────────┐
│                 patch                 │
│                 json                  │
├───────────────────────────────────────┤
│ [{"op":"add","path":"/id","value":5}] │
└───────────────────────────────────────┘
```

### `json_schema_update(schema, json_data)`
Updates the JSON data to conform to the schema by applying default values for missing properties.

**Usage:**
```sql
-- Update JSON data with schema defaults
SELECT json_schema_update('{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "User Profile",
  "type": "object",
  "properties": {
    "id": {
      "type": "integer",
      "description": "A unique identifier for the user",
      "default": 5
    }
  }
}', {'name': 'George'}) as updated;
┌──────────────────────────┐
│         updated          │
│           json           │
├──────────────────────────┤
│ {"id":5,"name":"George"} │
└──────────────────────────┘
```

**Parameters:**
- `schema`: A JSON Schema string following the JSON Schema specification
- `json_data`: JSON data to validate or update (for validation and update functions)

**JSON Schema Support:**
The JSON Schema extension supports the JSON Schema specification, including:

- **Data types**: `string`, `number`, `integer`, `boolean`, `array`, `object`, `null`
- **Validation keywords**: `required`, `properties`, `type`, `format`, `minimum`, `maximum`
- **Default values**: `default` keyword for automatic value assignment
- **Schema metadata**: `title`, `description`, `$schema`
- **Draft versions**: Supports modern JSON Schema drafts

## JSON Schema Features

The JSON Schema extension supports a comprehensive set of JSON Schema features:

### Data Types
- **Primitive types**: `string`, `number`, `integer`, `boolean`, `null`
- **Complex types**: `object`, `array`
- **Type validation**: Ensures data matches expected types

### Validation Keywords
- **Required fields**: `required` array to specify mandatory properties
- **Property definitions**: `properties` to define object structure
- **Numeric constraints**: `minimum`, `maximum`, `multipleOf`
- **String constraints**: `minLength`, `maxLength`, `pattern`, `format`
- **Array constraints**: `minItems`, `maxItems`, `uniqueItems`

### Default Values
- **Automatic enrichment**: `default` keyword automatically populates missing values
- **Nested defaults**: Support for default values in nested objects and arrays
- **Type-aware defaults**: Ensures default values match property types

## Tips and Best Practices

1. **Design schemas first**: Define your JSON schemas before implementing data validation
2. **Use meaningful descriptions**: Include `description` fields to document your schema intent
3. **Leverage default values**: Use `default` properties to ensure data completeness
4. **Validate schemas**: Always use `json_schema_validate_schema()` to verify schema correctness
5. **Handle validation failures**: Plan for cases where data doesn't conform to schemas
6. **Version your schemas**: Use `$schema` to specify which JSON Schema draft you're using
7. **Test with real data**: Validate schemas against actual data samples during development
8. **Performance considerations**: Complex schemas with deeply nested structures may impact performance

## Error Handling

The JSON Schema extension provides clear error messages for common issues:

- **Invalid schemas**: Detailed messages about schema syntax or structure problems
- **Validation failures**: Specific information about which data doesn't conform to the schema
- **Type mismatches**: Clear indication when data types don't match schema expectations
- **Missing required fields**: Identification of which required properties are absent

## Contributing

The JSON Schema extension is open source and developed by [Query.Farm](https://query.farm). Contributions are welcome!

## License

[MIT License](LICENSE)