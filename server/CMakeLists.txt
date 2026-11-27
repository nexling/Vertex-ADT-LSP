cmake_minimum_required(VERSION 3.20)

project(vertex-adt-ls LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "vertex-adt-ls requires an x64 toolchain")
endif()

# Release /MT must be set before FetchContent so lsp-framework is MultiThreaded too.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

set(LSP_INSTALL OFF CACHE BOOL "" FORCE)
set(LSP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LSP_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# 2.0.0 still registers an SSH-only test submodule. Tests stay off, so skip it.
FetchContent_Declare(
  lsp-framework
  GIT_REPOSITORY https://github.com/leon-bckl/lsp-framework.git
  GIT_TAG 2.0.0
  GIT_SUBMODULES ""
  GIT_SUBMODULES_RECURSE FALSE
)
FetchContent_MakeAvailable(lsp-framework)

# Generated catalog TUs: npm run generate-catalogs, from server/catalog/*.json (CMake does not run it).
# Custom validators (hand-maintained): abs, dbcreate, get_area_param, new_iterator, pnt_to_plane.
# Everything but main.cpp, so tests can link the real validators and code actions.
set(VERTEX_ADT_LS_CORE_SOURCES
  src/document_store.cpp
  src/work_scheduler.cpp
  src/text_util.cpp
  src/line_continuation.cpp
  src/library_paths.cpp
  src/dir_paths.cpp
  src/extracts.cpp
  src/parent_context.cpp
  src/validate_included_from.cpp
  src/included_from_ui.cpp
  src/workspace_index.cpp
  src/linked_files.cpp
  src/prep_cache.cpp
  src/workspace_state.cpp
  src/debug_dump.cpp
  src/debug_trace.cpp
  src/cheap_hover.cpp
  src/control_block_pairing.cpp
  src/indent_formatter.cpp
  src/cheap_completion.cpp
  src/completion_ranking.cpp
  src/slot_completion.cpp
  src/definition.cpp
  src/references.cpp
  src/document_links.cpp
  src/include_call_completion.cpp
  src/call_parameters.cpp
  src/cross_call_types.cpp
  src/remaining_param_completion.cpp
  src/call_return_value_completion.cpp
  src/signature_help.cpp
  src/dbcreate_field_completion.cpp
  src/debug_filename_completion.cpp
  src/form_name_completion.cpp
  src/macro_library_xml.cpp
  src/macro_library_completion.cpp
  src/vxsettings_xml.cpp
  src/piece_setup_sets.cpp
  src/object_parameter_completion.cpp
  src/newline_cleanup.cpp
  src/code_actions.cpp
  src/workspace_commands.cpp
  src/program_move.cpp
  src/program_move_commands.cpp
  src/workspace_exports.cpp
  src/impact_codelens.cpp
  src/namespace_graph.cpp
  src/local_slot_rename.cpp
  src/utf16_text.cpp
  src/command_parse.cpp
  src/semantic_tokens.cpp
  src/validate.cpp
  src/validate_type_timeline.cpp
  src/validate_if_loop_indent.cpp
  src/validate_command_syntax.cpp
  src/validate_used_before.cpp
  src/validate_unused_variables.cpp
  src/validate_call_param.cpp
  src/validate_cross_call_types.cpp
  src/validate_unknown_section.cpp
  src/validate_unclosed_resources.cpp
  src/validate_infinite_loops.cpp
  src/infinite_loops.cpp
  src/validate_in_file_decl.cpp
  src/validate_local_slots.cpp
  src/validate_callee_common.cpp
  src/validate_cross_file.cpp
  src/validate_joint_section.cpp
  src/bracket_balance.cpp
  src/concat_whitespace.cpp
  src/substring_index.cpp
  src/vertex_dbase_converter.cpp
  src/catalog/catalog.cpp
  src/catalog/table_validator.cpp
  src/catalog/custom_validators.cpp
  src/generated/command_catalog_data.cpp
  src/generated/keyword_catalog_data.cpp
  src/generated/command_type_writes_data.cpp
  src/generated/object_parameters_data.cpp
)
add_executable(vertex-adt-ls src/main.cpp ${VERTEX_ADT_LS_CORE_SOURCES})
target_include_directories(vertex-adt-ls PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(vertex-adt-ls PRIVATE lsp)
target_compile_features(vertex-adt-ls PRIVATE cxx_std_20)

if(MSVC)
  target_compile_options(vertex-adt-ls PRIVATE /utf-8 /bigobj)
endif()

set(VERTEX_ADT_LS_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/vertex-adt-ls.exe" CACHE FILEPATH
  "Language-server executable copy destination")

add_custom_command(
  TARGET vertex-adt-ls POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:vertex-adt-ls>
    ${VERTEX_ADT_LS_OUTPUT}
  COMMENT "Placing language-server executable at ${VERTEX_ADT_LS_OUTPUT}"
)

# Vertex Dbase Converter helper spawn tests (not shipped, not in ALL):
#   cmake --build build --config Release --target vertex-dbase-converter-test
#   build\Release\vertex-dbase-converter-test.exe
add_executable(fake-dbase-converter EXCLUDE_FROM_ALL tests/fake_dbase_converter.cpp)
target_compile_features(fake-dbase-converter PRIVATE cxx_std_20)

add_executable(vertex-dbase-converter-test EXCLUDE_FROM_ALL
  tests/vertex_dbase_converter_test.cpp
  src/vertex_dbase_converter.cpp
)
add_dependencies(vertex-dbase-converter-test fake-dbase-converter)
target_include_directories(vertex-dbase-converter-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(vertex-dbase-converter-test PRIVATE lsp)
target_compile_features(vertex-dbase-converter-test PRIVATE cxx_std_20)
get_filename_component(VERTEX_ADT_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
target_compile_definitions(vertex-dbase-converter-test PRIVATE
  "FAKE_CONVERTER_PATH=L\"$<TARGET_FILE:fake-dbase-converter>\""
  "VERTEX_ADT_REPO_ROOT=L\"${VERTEX_ADT_REPO_ROOT}\""
)
if(MSVC)
  target_compile_options(fake-dbase-converter PRIVATE /utf-8)
  target_compile_options(vertex-dbase-converter-test PRIVATE /utf-8)
endif()

# $DIR_* table, path display, value tracing and hover (not shipped, not in ALL):
#   cmake --build build --config Release --target dir-paths-test
#   build\Release\dir-paths-test.exe
add_executable(dir-paths-test EXCLUDE_FROM_ALL
  tests/dir_paths_test.cpp
  src/dir_paths.cpp
  src/library_paths.cpp
  src/text_util.cpp
  src/utf16_text.cpp
)
target_include_directories(dir-paths-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dir-paths-test PRIVATE lsp)
target_compile_features(dir-paths-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(dir-paths-test PRIVATE /utf-8)
endif()

# piece.setup set completion: vxsettings reader, conditions, overlay, site
# (not shipped, not in ALL):
#   cmake --build build --config Release --target piece-setup-sets-test
#   build\Release\piece-setup-sets-test.exe
add_executable(piece-setup-sets-test EXCLUDE_FROM_ALL
  tests/piece_setup_sets_test.cpp
  src/piece_setup_sets.cpp
  src/vxsettings_xml.cpp
  src/form_name_completion.cpp
  src/library_paths.cpp
  src/dir_paths.cpp
  src/text_util.cpp
  src/utf16_text.cpp
)
target_include_directories(piece-setup-sets-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(piece-setup-sets-test PRIVATE lsp)
target_compile_features(piece-setup-sets-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(piece-setup-sets-test PRIVATE /utf-8)
endif()

# Unbalanced parentheses / square brackets: document walk, full validate
# (not shipped, not in ALL):
#   cmake --build build --config Release --target bracket-balance-test
#   build\Release\bracket-balance-test.exe
add_executable(bracket-balance-test EXCLUDE_FROM_ALL
  tests/bracket_balance_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(bracket-balance-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(bracket-balance-test PRIVATE lsp)
target_compile_features(bracket-balance-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(bracket-balance-test PRIVATE /utf-8 /bigobj)
endif()

# Whitespace around a concatenation `&`: spans, document walk, code actions
# (not shipped, not in ALL):
#   cmake --build build --config Release --target concat-whitespace-test
#   build\Release\concat-whitespace-test.exe
add_executable(concat-whitespace-test EXCLUDE_FROM_ALL
  tests/concat_whitespace_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(concat-whitespace-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(concat-whitespace-test PRIVATE lsp)
target_compile_features(concat-whitespace-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(concat-whitespace-test PRIVATE /utf-8 /bigobj)
endif()

# Document links: named-section suffixes on call / # include paths
# (not shipped, not in ALL):
#   cmake --build build --config Release --target document-links-test
#   build\Release\document-links-test.exe
add_executable(document-links-test EXCLUDE_FROM_ALL
  tests/document_links_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(document-links-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(document-links-test PRIVATE lsp)
target_compile_features(document-links-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(document-links-test PRIVATE /utf-8 /bigobj)
endif()

# Literal substring index checks and substring assignment: line scan, document
# walk, code action (not shipped, not in ALL):
#   cmake --build build --config Release --target substring-index-test
#   build\Release\substring-index-test.exe
add_executable(substring-index-test EXCLUDE_FROM_ALL
  tests/substring_index_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(substring-index-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(substring-index-test PRIVATE lsp)
target_compile_features(substring-index-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(substring-index-test PRIVATE /utf-8 /bigobj)
endif()

# findstr* value expressions: arithmetic after the call, findstrr entry, value
# return types (not shipped, not in ALL):
#   cmake --build build --config Release --target findstr-value-expression-test
#   build\Release\findstr-value-expression-test.exe
add_executable(findstr-value-expression-test EXCLUDE_FROM_ALL
  tests/findstr_value_expression_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(findstr-value-expression-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(findstr-value-expression-test PRIVATE lsp)
target_compile_features(findstr-value-expression-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(findstr-value-expression-test PRIVATE /utf-8 /bigobj)
endif()

# Native snippet and catalog corrections (docs/adr/0016): snippet insertText,
# dbdelete without and dbgetactrow with parentheses (not shipped, not in ALL):
#   cmake --build build --config Release --target snippet-catalog-corrections-test
#   build\Release\snippet-catalog-corrections-test.exe
add_executable(snippet-catalog-corrections-test EXCLUDE_FROM_ALL
  tests/snippet_catalog_corrections_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(snippet-catalog-corrections-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(snippet-catalog-corrections-test PRIVATE lsp)
target_compile_features(snippet-catalog-corrections-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(snippet-catalog-corrections-test PRIVATE /utf-8 /bigobj)
endif()

# Drawing, model and file command entries (docs/adr/0020): parenthesesOptional
# statement form and `;`-separated statements (not shipped, not in ALL):
#   cmake --build build --config Release --target drawing-file-commands-test
#   build\Release\drawing-file-commands-test.exe
add_executable(drawing-file-commands-test EXCLUDE_FROM_ALL
  tests/drawing_file_commands_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(drawing-file-commands-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(drawing-file-commands-test PRIVATE lsp)
target_compile_features(drawing-file-commands-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(drawing-file-commands-test PRIVATE /utf-8 /bigobj)
endif()

# Point, edge, note, map and system command entries (docs/adr/0023; not shipped, not in ALL):
#   cmake --build build --config Release --target note-map-system-commands-test
add_executable(note-map-system-commands-test EXCLUDE_FROM_ALL
  tests/note_map_system_commands_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(note-map-system-commands-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(note-map-system-commands-test PRIVATE lsp)
target_compile_features(note-map-system-commands-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(note-map-system-commands-test PRIVATE /utf-8 /bigobj)
endif()

# File, map, error and form command entries (docs/adr/0028; not shipped, not in ALL):
#   cmake --build build --config Release --target file-map-error-form-commands-test
add_executable(file-map-error-form-commands-test EXCLUDE_FROM_ALL
  tests/file_map_error_form_commands_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(file-map-error-form-commands-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(file-map-error-form-commands-test PRIVATE lsp)
target_compile_features(file-map-error-form-commands-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(file-map-error-form-commands-test PRIVATE /utf-8 /bigobj)
endif()

# Control-block pairing: if/else/endif and loop/endloop hover and document
# highlight (not shipped, not in ALL):
#   cmake --build build --config Release --target control-block-pairing-test
#   build\Release\control-block-pairing-test.exe
add_executable(control-block-pairing-test EXCLUDE_FROM_ALL
  tests/control_block_pairing_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(control-block-pairing-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(control-block-pairing-test PRIVATE lsp)
target_compile_features(control-block-pairing-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(control-block-pairing-test PRIVATE /utf-8 /bigobj)
endif()

# Indentation formatter: depth rules, if headers, backslash groups, refusals
# (not shipped, not in ALL):
#   cmake --build build --config Release --target indent-formatter-test
#   build\Release\indent-formatter-test.exe
add_executable(indent-formatter-test EXCLUDE_FROM_ALL
  tests/indent_formatter_test.cpp
  src/indent_formatter.cpp
  src/line_continuation.cpp
  src/text_util.cpp
  src/utf16_text.cpp
)
target_include_directories(indent-formatter-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(indent-formatter-test PRIVATE lsp)
target_compile_features(indent-formatter-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(indent-formatter-test PRIVATE /utf-8)
endif()

# Indentation formatter corpus gate: idempotent, leading whitespace only
# (not shipped, not in ALL):
#   cmake --build build --config Release --target indent-formatter-corpus
#   build\Release\indent-formatter-corpus.exe C:\vxbd320 [tabSize] [--list-refusals]
add_executable(indent-formatter-corpus EXCLUDE_FROM_ALL
  tests/indent_formatter_corpus.cpp
  src/indent_formatter.cpp
  src/line_continuation.cpp
  src/text_util.cpp
  src/utf16_text.cpp
)
target_include_directories(indent-formatter-corpus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(indent-formatter-corpus PRIVATE lsp)
target_compile_features(indent-formatter-corpus PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(indent-formatter-corpus PRIVATE /utf-8)
endif()

# Trailing `\` line continuation: code-line rule, joining, position map
# (not shipped, not in ALL):
#   cmake --build build --config Release --target line-continuation-test
#   build\Release\line-continuation-test.exe
add_executable(line-continuation-test EXCLUDE_FROM_ALL
  tests/line_continuation_test.cpp
  src/line_continuation.cpp
  src/text_util.cpp
  src/utf16_text.cpp
)
target_include_directories(line-continuation-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(line-continuation-test PRIVATE lsp)
target_compile_features(line-continuation-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(line-continuation-test PRIVATE /utf-8)
endif()

# Program move: reference scan, resolution, path spelling, plan
# (docs/adr/0017-native-program-move.md; not shipped, not in ALL):
#   cmake --build build --config Release --target program-move-test
#   build\Release\program-move-test.exe
add_executable(program-move-test EXCLUDE_FROM_ALL
  tests/program_move_test.cpp
  src/program_move.cpp
  src/dir_paths.cpp
  src/library_paths.cpp
  src/text_util.cpp
  src/utf16_text.cpp
)
target_include_directories(program-move-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(program-move-test PRIVATE lsp)
target_compile_features(program-move-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(program-move-test PRIVATE /utf-8)
endif()

# Debug traces: parser/hover parity with the regex scan, line split, timings
# (docs/adr/0025-debug-trace-fast-path.md; not shipped, not in ALL):
#   cmake --build build --config Release --target debug-trace-test
#   build\Release\debug-trace-test.exe [--parity] [file.dbg ...]
add_executable(debug-trace-test EXCLUDE_FROM_ALL
  tests/debug_trace_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(debug-trace-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(debug-trace-test PRIVATE lsp)
target_compile_features(debug-trace-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(debug-trace-test PRIVATE /utf-8 /bigobj)
endif()

# Infinite-loop checks: bare loop with no exit, exitif that never changes
# (docs/adr/0027-native-infinite-loop-warnings.md; not shipped, not in ALL):
#   cmake --build build --config Release --target infinite-loops-test
#   build\Release\infinite-loops-test.exe
add_executable(infinite-loops-test EXCLUDE_FROM_ALL
  tests/infinite_loops_test.cpp
  ${VERTEX_ADT_LS_CORE_SOURCES}
)
target_include_directories(infinite-loops-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(infinite-loops-test PRIVATE lsp)
target_compile_features(infinite-loops-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(infinite-loops-test PRIVATE /utf-8 /bigobj)
endif()
