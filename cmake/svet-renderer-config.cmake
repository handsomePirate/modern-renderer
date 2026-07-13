include(CMakeFindDependencyMacro)

find_dependency(SDL3 REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/svet-renderer-targets.cmake")