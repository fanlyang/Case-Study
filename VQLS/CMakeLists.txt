cmake_minimum_required(VERSION 3.10)
project(MyXACCProject VERSION 1.0 LANGUAGES CXX)

# -------------------------------------------------------------------------
# Basic language / standard
# -------------------------------------------------------------------------
# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------------------------------------------------------
# XACC installation root (set early)
# -------------------------------------------------------------------------
# Default to $HOME/.xacc (this will be /root/.xacc when running as root in the container).
# Users may override by passing -DXACC_ROOT=/path/to/xacc on the cmake command line.
set(XACC_ROOT "$ENV{HOME}/.xacc" CACHE PATH "XACC install root")

# Append likely package-config locations so find_package(CONFIG) can find CppMicroServices.
# The config file you found lives under: <XACC_ROOT>/share/cppmicroservices4/cmake/CppMicroServicesConfig.cmake
list(APPEND CMAKE_PREFIX_PATH
  "${XACC_ROOT}/share/cppmicroservices4"
  "${XACC_ROOT}"
)

# If the user or environment explicitly sets CppMicroServices_DIR, keep it; otherwise
# set a reasonable default pointing to the cmake dir inside the cppmicroservices share dir.
if(NOT DEFINED CppMicroServices_DIR OR CppMicroServices_DIR STREQUAL "")
  set(_default_cppms_dir "${XACC_ROOT}/share/cppmicroservices4/cmake")
  if(EXISTS "${_default_cppms_dir}/CppMicroServicesConfig.cmake")
    set(CppMicroServices_DIR "${_default_cppms_dir}" CACHE PATH "CppMicroServices config directory" FORCE)
  endif()
endif()

# Print helpful info for debugging configure-time search paths.
message(STATUS "XACC_ROOT = ${XACC_ROOT}")
message(STATUS "CMAKE_PREFIX_PATH = ${CMAKE_PREFIX_PATH}")
message(STATUS "CppMicroServices_DIR = ${CppMicroServices_DIR}")

# -------------------------------------------------------------------------
# Find CppMicroServices (CONFIG mode preferred)
# -------------------------------------------------------------------------
# Try to find the installed CppMicroServices that ships with XACC.
# We try quietly first and then fall back to manual locate if necessary.
find_package(CppMicroServices QUIET COMPONENTS Framework)

if(NOT CppMicroServices_FOUND)
  message(STATUS "find_package(CppMicroServices) did not find a config file; attempting fallback search under ${XACC_ROOT} ...")

  # Try again after explicitly setting CppMicroServices_DIR (if not already set)
  if(DEFINED CppMicroServices_DIR AND EXISTS "${CppMicroServices_DIR}/CppMicroServicesConfig.cmake")
    message(STATUS "Attempting find_package with CppMicroServices_DIR = ${CppMicroServices_DIR}")
    find_package(CppMicroServices REQUIRED COMPONENTS Framework)
  else()
    # Manual locate: find headers and library, then create an IMPORTED target so the rest of the CMakeLists
    # can link to CppMicroServices::Framework even if no config file is present.
    find_path(CPPMS_INCLUDE_DIR
      NAMES cppmicroservices/Bundle.h
      PATHS "${XACC_ROOT}/include" "${XACC_ROOT}/include/cppmicroservices" ${CMAKE_SYSTEM_INCLUDE_PATH}
    )

    find_library(CPPMS_LIBRARY
      NAMES CppMicroServices libCppMicroServices cppmicroservices
      PATHS "${XACC_ROOT}/lib" ${CMAKE_LIBRARY_PATH}
    )

    if(CPPMS_INCLUDE_DIR AND CPPMS_LIBRARY)
      message(STATUS "Found CppMicroServices include: ${CPPMS_INCLUDE_DIR}")
      message(STATUS "Found CppMicroServices library: ${CPPMS_LIBRARY}")

      add_library(CppMicroServices::Framework SHARED IMPORTED GLOBAL)
      set_target_properties(CppMicroServices::Framework PROPERTIES
        IMPORTED_LOCATION "${CPPMS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CPPMS_INCLUDE_DIR}"
      )
    else()
      message(FATAL_ERROR "Could not find CppMicroServices config file or headers/libs under ${XACC_ROOT}. \
Either install the development files, or run cmake with -DCppMicroServices_DIR=/root/.xacc/share/cppmicroservices4/cmake (adjust path as needed).")
    endif()
  endif()
else()
  message(STATUS "Found CppMicroServices via find_package (CONFIG).")
endif()

# -------------------------------------------------------------------------
# Try to find XACC CMake package (optional). If installed with cmake config,
# find_package(XACC CONFIG) will provide targets; otherwise we will link to
# the 'xacc' library name and add manual include dirs below.
# -------------------------------------------------------------------------
find_package(XACC QUIET CONFIG)
if(XACC_FOUND)
  message(STATUS "Found XACC CMake package via find_package(XACC CONFIG).")
else()
  message(STATUS "XACC CMake package not found via find_package; will use manual include/lib names from ${XACC_ROOT}.")
endif()

# ----------------------------------------
# Ensure XACC library is discovered and expose an imported target
# (This ensures the linker uses the correct full path to libxacc)
# ----------------------------------------
# find the xacc library in the XACC install lib directory
find_library(XACC_LIBRARY NAMES xacc libxacc PATHS "${XACC_ROOT}/lib" NO_DEFAULT_PATH)
find_library(XACC_QUANTUM_GATE_LIBRARY NAMES xacc-quantum-gate PATHS "${XACC_ROOT}/lib" NO_DEFAULT_PATH)

if(NOT XACC_LIBRARY)
  message(FATAL_ERROR "Could not find libxacc in ${XACC_ROOT}/lib. Check XACC_ROOT and that libxacc exists.")
endif()

if(NOT XACC_QUANTUM_GATE_LIBRARY)
  message(FATAL_ERROR "Could not find libxacc-quantum-gate in ${XACC_ROOT}/lib. Check XACC_ROOT and that libxacc-quantum-gate exists.")
endif()
# create an imported target so linking is robust
add_library(XACC::xacc SHARED IMPORTED GLOBAL)
set_target_properties(XACC::xacc PROPERTIES
  IMPORTED_LOCATION "${XACC_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${XACC_ROOT}/include"
)

add_library(XACC::quantum_gate SHARED IMPORTED GLOBAL)
set_target_properties(XACC::quantum_gate PROPERTIES
  IMPORTED_LOCATION "${XACC_QUANTUM_GATE_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${XACC_ROOT}/include"
)

# -------------------------------------------------------------------------
# Sources and executable target
# -------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.cc" "src/*.cxx")
if(NOT SOURCES)
  message(WARNING "No source files found in src/. Ensure your sources are in src/ or adjust the GLOB pattern.")
endif()

add_executable(demo ${SOURCES})

# Add XACC include directories (safe even if the XACC CMake package provides them).
target_include_directories(demo PRIVATE
  "${XACC_ROOT}/include"
  "${XACC_ROOT}/include/xacc"
  "${XACC_ROOT}/include/quantum/gate"
)

# If we created or found an XACC exported target (XACC::xacc), prefer it. Else we'll link by library name.
set(EXTRA_LIBS pthread dl)

if(TARGET CppMicroServices::Framework)
  list(APPEND EXTRA_LIBS CppMicroServices::Framework)
else()
  # If find_package provided variables instead of targets (older installs)
  list(APPEND EXTRA_LIBS ${CppMicroServices_LIBRARIES})
endif()

if(TARGET XACC::xacc)
  list(APPEND EXTRA_LIBS XACC::xacc)
else()
  list(APPEND EXTRA_LIBS xacc)
endif()
# Also link the quantum gate plugin library
list(APPEND EXTRA_LIBS  XACC::quantum_gate)

target_link_libraries(demo PRIVATE ${EXTRA_LIBS})

# -------------------------------------------------------------------------
# Helpful configure/run messages
# -------------------------------------------------------------------------
message(STATUS "Using XACC library: ${XACC_LIBRARY}")
message(STATUS "If runtime loading fails, ensure the following environment variables are set before running:")
message(STATUS "  export XACC_ROOT=\"${XACC_ROOT}\"")
message(STATUS "  export LD_LIBRARY_PATH=\"$XACC_ROOT/lib:$XACC_ROOT/plugins:\${LD_LIBRARY_PATH}\"")
message(STATUS "Before running the produced binary, ensure runtime library search path includes XACC libs/plugins, e.g.:")
message(STATUS "  export XACC_ROOT=\"${XACC_ROOT}\"")
message(STATUS "  export LD_LIBRARY_PATH=\"$XACC_ROOT/lib:$XACC_ROOT/plugins:$LD_LIBRARY_PATH\"")