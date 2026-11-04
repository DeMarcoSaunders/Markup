# Driver for the installed-package tests. Run with `cmake -P`, never included.
#
# Installs the project to a throwaway prefix, then configures, builds and runs a
# standalone consumer project against it. That round trip is the only way to test what
# `find_package(Markup)` consumers actually get: the ordinary tests link the in-build
# targets, so a wrong EXPORT_NAME, a library left out of install(), or a backend define
# that never reached the export are all invisible to them.
#
# Expected -D variables:
#   MARKUP_BUILD_DIR          build tree to install from
#   MARKUP_CONFIG             config to install and build ("Release", ...). Legitimately
#                             empty on a single-config generator with no CMAKE_BUILD_TYPE,
#                             where $<CONFIG> expands to nothing -- so it is optional, and
#                             --config is omitted rather than passed blank.
#   MARKUP_PREFIX             throwaway install prefix (wiped each run)
#   MARKUP_CONSUMER_SRC       consumer project source dir
#   MARKUP_CONSUMER_BIN       consumer build dir (wiped each run)
#   MARKUP_EXE_NAME           consumer executable, without suffix
#   MARKUP_GENERATOR          generator to reuse for the nested build
#   MARKUP_GENERATOR_PLATFORM optional, -A
#   MARKUP_GENERATOR_TOOLSET  optional, -T
#   MARKUP_C_COMPILER         optional, pinned so the nested build cannot pick another
#   MARKUP_EXE_SUFFIX         optional, ".exe" on Windows

foreach(_required
    MARKUP_BUILD_DIR MARKUP_PREFIX
    MARKUP_CONSUMER_SRC MARKUP_CONSUMER_BIN MARKUP_EXE_NAME MARKUP_GENERATOR)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "run.cmake: ${_required} not set")
    endif()
endforeach()

# Reports a failed step with its output. execute_process swallows it otherwise, and a bare
# "exit code 1" from a nested configure is not something anyone can act on.
function(markup_run_step label)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "${label} failed (exit ${_rc})\n"
            "command: ${ARGN}\n"
            "--- stdout ---\n${_out}\n"
            "--- stderr ---\n${_err}")
    endif()
    set(MARKUP_STEP_OUTPUT "${_out}" PARENT_SCOPE)
endfunction()

# Both are inside the build tree. Wiped so a stale prefix cannot make a broken install
# look fine -- the failure this whole test exists to catch.
#
# MARKUP_CONSUMER_BIN is kept deliberately short by the caller: the nested configure adds
# CMakeFiles/CMakeScratch/TryCompile-xxxxxx/cmTC_xxxxx.dir/<cfg>/cmTC_xxxxx.tlog/... under
# it, roughly 110 characters, and on Windows the whole thing must stay under MAX_PATH
# (260). A build directory nested deeply enough will still trip it, with an MSB4018
# "exceeds the OS max path limit" from the try-compile rather than anything about Markup.
file(REMOVE_RECURSE "${MARKUP_PREFIX}" "${MARKUP_CONSUMER_BIN}")

# --config only where there is a config to name. Passing it blank is an error, and a
# single-config tree built with no CMAKE_BUILD_TYPE has exactly one thing to install.
set(_config_args "")
if(MARKUP_CONFIG)
    set(_config_args --config "${MARKUP_CONFIG}")
endif()

markup_run_step("install"
    "${CMAKE_COMMAND}" --install "${MARKUP_BUILD_DIR}"
    ${_config_args} --prefix "${MARKUP_PREFIX}")

set(_cfg_args
    -S "${MARKUP_CONSUMER_SRC}"
    -B "${MARKUP_CONSUMER_BIN}"
    -G "${MARKUP_GENERATOR}"
    "-DCMAKE_PREFIX_PATH=${MARKUP_PREFIX}")
if(MARKUP_GENERATOR_PLATFORM)
    list(APPEND _cfg_args -A "${MARKUP_GENERATOR_PLATFORM}")
endif()
if(MARKUP_GENERATOR_TOOLSET)
    list(APPEND _cfg_args -T "${MARKUP_GENERATOR_TOOLSET}")
endif()
if(MARKUP_C_COMPILER)
    list(APPEND _cfg_args "-DCMAKE_C_COMPILER=${MARKUP_C_COMPILER}")
endif()
# Multi-config generators take the config at build time; the rest need it now -- but only
# if the parent build named one, so the consumer matches however this tree was configured.
if(MARKUP_CONFIG AND NOT MARKUP_GENERATOR MATCHES "Visual Studio|Xcode|Ninja Multi-Config")
    list(APPEND _cfg_args "-DCMAKE_BUILD_TYPE=${MARKUP_CONFIG}")
endif()

markup_run_step("consumer configure" "${CMAKE_COMMAND}" ${_cfg_args})
markup_run_step("consumer build"
    "${CMAKE_COMMAND}" --build "${MARKUP_CONSUMER_BIN}" ${_config_args})

# The consumer pins RUNTIME_OUTPUT_DIRECTORY to bin/ for every config, so there is one
# path to look in rather than a per-generator guess.
set(_exe "${MARKUP_CONSUMER_BIN}/bin/${MARKUP_EXE_NAME}${MARKUP_EXE_SUFFIX}")
if(NOT EXISTS "${_exe}")
    message(FATAL_ERROR "consumer built but ${_exe} is missing")
endif()

markup_run_step("consumer run" "${_exe}")
message(STATUS "${MARKUP_STEP_OUTPUT}")
