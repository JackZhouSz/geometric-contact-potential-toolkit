set(SOURCES
  # Tests
  test_interval.cpp
  test_utils.cpp
  test_quadrature.cpp

  # Benchmarks

  # Utilities
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCES})
target_sources(ipc_toolkit_tests PRIVATE ${SOURCES})

################################################################################
# Subfolders
################################################################################