PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=json_schema
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# vcpkg deps must use the same exception ABI as the wasm variant (JS-EH for
# wasm_mvp, native wasm EH for wasm_eh/threads); see vcpkg-triplets/.
wasm_mvp: export DUCKDB_WASM_VCPKG_CXX_FLAGS=-fexceptions
wasm_eh: export DUCKDB_WASM_VCPKG_CXX_FLAGS=-fwasm-exceptions
wasm_threads: export DUCKDB_WASM_VCPKG_CXX_FLAGS=-fwasm-exceptions -pthread

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile