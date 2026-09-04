# This file is included by DuckDB's build system. It specifies which extension to load

# Extension from this repo
duckdb_extension_load(json_schema
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LOAD_TESTS
    # Wasm SIDE_MODULE link only includes LINKED_LIBS; name the validator lib
    # or its symbols are undefined (loads, throws "n is not a function").
    LINKED_LIBS "../../vcpkg_installed/wasm32-emscripten/lib/libnlohmann_json_schema_validator.a"
)

duckdb_extension_load(json)
