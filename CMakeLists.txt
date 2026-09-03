# T001 — two targets with deliberately different dependency sets.
#
# esqlcpp (the preprocessor) must link NO MariaDB library: NFR-001.2 requires
# the Tier 1 suite to run with no MariaDB installed at all. T084 enforces that
# in CI; this file keeps the dependency out in the first place.
#
# libesqlc (the runtime) is C11 rather than the C99 the plan named, because
# Principle VI mandates _Static_assert on the ABI structure and C99 has no such
# facility. Recorded as a deviation in the implementation report.
cmake_minimum_required(VERSION 3.20)
project(esqlc C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

# ---- the preprocessor: no database, no MariaDB ------------------------------
add_executable(esqlcpp
    src/pp/main.cc
    src/pp/diag.cc
    src/pp/scan.cc
    src/pp/decl.cc
    src/pp/dispatch.cc
    src/pp/emit.cc
    src/pp/whenever.cc
    src/pp/sqlsa.cc
    src/pp/charset.cc
)
target_include_directories(esqlcpp PRIVATE src/pp)

# ---- the runtime: the only thing that knows MariaDB exists ------------------
# -DESQLC_NO_MARIADB=ON forces the no-MariaDB path on a machine that has it
# installed, so NFR-001.2 is verifiable locally and not only in CI.
option(ESQLC_NO_MARIADB "Pretend MariaDB is absent (for verifying NFR-001.2)" OFF)

if(ESQLC_NO_MARIADB)
    set(MARIADB_CONFIG "")
    message(STATUS "ESQLC_NO_MARIADB=ON — runtime target skipped deliberately")
else()
    find_program(MARIADB_CONFIG NAMES mariadb_config mysql_config)
endif()

if(MARIADB_CONFIG)
    execute_process(COMMAND ${MARIADB_CONFIG} --include
                    OUTPUT_VARIABLE MARIADB_INCLUDE OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${MARIADB_CONFIG} --libs
                    OUTPUT_VARIABLE MARIADB_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
    # mariadb_config emits several -I flags; pass them through verbatim rather
    # than trying to reduce them to a single directory.
    separate_arguments(MARIADB_INC_LIST  UNIX_COMMAND "${MARIADB_INCLUDE}")
    separate_arguments(MARIADB_LIBS_LIST UNIX_COMMAND "${MARIADB_LIBS}")

    add_library(esqlc STATIC
        src/rt/context.c
        src/rt/session.c
        src/rt/txn.c
        src/rt/exec.c
        src/rt/cursor.c
        src/rt/sqlca.c
        src/rt/sqlsa.c
        src/rt/diag.c
    )
    target_include_directories(esqlc PUBLIC include PRIVATE src/rt)
    target_compile_options(esqlc PRIVATE ${MARIADB_INC_LIST})
    target_link_libraries(esqlc PUBLIC ${MARIADB_LIBS_LIST})
else()
    message(STATUS "mariadb_config not found — runtime target skipped, "
                   "Tier 1 preprocessor tests still build and run")
endif()

# ---- stub runtime: satisfies the ABI with no database (T007) ----------------
add_library(esqlc_stub STATIC tests/stub/esqlc_stub.c)
target_include_directories(esqlc_stub PUBLIC include)

enable_testing()
add_test(NAME tier1_golden
         COMMAND ${CMAKE_SOURCE_DIR}/tests/harness/run_golden.sh $<TARGET_FILE:esqlcpp>)
add_test(NAME tier1_negative
         COMMAND ${CMAKE_SOURCE_DIR}/tests/harness/run_negative.sh $<TARGET_FILE:esqlcpp>)
add_test(NAME abi_isolation
         COMMAND ${CMAKE_SOURCE_DIR}/tests/harness/abi_isolation.sh $<TARGET_FILE:esqlcpp>)
add_test(NAME contract_sync
         COMMAND ${CMAKE_SOURCE_DIR}/tests/harness/contract_sync.sh)

# Spec-derived assertions. These are the specification tests; the golden files
# above are regression guards. A snapshot asserts what the code does, an
# assertion asserts what the requirement demands — only the latter can fail
# when the code is wrong but self-consistent.
add_test(NAME spec_assertions
         COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/harness/spec_assertions.py
                 $<TARGET_FILE:esqlcpp>)

# Every diagnostic the code can emit must be registered in a spec. Two codes
# slipped through Gate 1 unregistered; this makes the check structural.
add_test(NAME diag_registry
         COMMAND ${CMAKE_SOURCE_DIR}/tests/harness/diag_registry.sh)

# The streaming attribute has no reliable functional symptom at fixture scale,
# so it is guarded structurally (Gate 3, T347).
add_test(NAME streaming_guard
         COMMAND ${CMAKE_SOURCE_DIR}/tests/harness/streaming_guard.sh)

# The emitter and the runtime encode the SQLSA layout independently — one as a
# C struct, one as an offset table. Add a field to one side only and the runtime
# corrupts the program's own memory silently. This compares them (Gate 5, T523).
if(TARGET esqlc)
    add_test(NAME sqlsa_layout_sync
             COMMAND ${CMAKE_SOURCE_DIR}/tests/harness/sqlsa_layout_sync.sh
                     $<TARGET_FILE:esqlcpp> $<TARGET_FILE:esqlc>)
    set_tests_properties(sqlsa_layout_sync PROPERTIES SKIP_RETURN_CODE 77)
endif()

# Principle I makes the manual the contract, so a citation pointing at the
# wrong page is worse than none — it looks like evidence. Skips when manual/ is
# absent, which is the CI case: the PDF is third-party copyright and gitignored.
add_test(NAME citation_check
         COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/harness/citation_check.py)
set_tests_properties(citation_check PROPERTIES SKIP_RETURN_CODE 77)

# Tier 2 needs a live server. It skips rather than fails when none is
# reachable, so NFR-001.2 (Tier 1 runs with no MariaDB) still holds.
if(TARGET esqlc)
    add_test(NAME tier2_live
             COMMAND ${CMAKE_SOURCE_DIR}/tests/harness/run_tier2.sh
                     $<TARGET_FILE:esqlcpp> $<TARGET_FILE:esqlc>)
    set_tests_properties(tier2_live PROPERTIES SKIP_RETURN_CODE 77)
endif()
