# JSON Schema repository notes

For installation and the full user documentation, see the
[Query Farm JSON Schema documentation](https://query.farm/products/extensions/json_schema/).

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
