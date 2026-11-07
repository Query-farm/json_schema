cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME json_schema)

# Header-only; the vcpkg port in vcpkg_ports/jsoncons patches it to evaluate regular expressions with RE2
find_package(jsoncons CONFIG REQUIRED)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)

set(EXTENSION_SOURCES src/json_schema_extension.cpp
src/query_farm_telemetry.cpp)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

target_link_libraries(${EXTENSION_NAME} jsoncons)
target_link_libraries(${LOADABLE_EXTENSION_NAME} jsoncons)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
