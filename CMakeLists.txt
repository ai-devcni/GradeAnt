cmake_minimum_required(VERSION 3.16)
project(gradeant_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
option(GRADEANT_BUILD_NATIVE "Build the OpenCV-backed native library" ON)

if(GRADEANT_BUILD_NATIVE)
  find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

  add_library(gradeant_core SHARED
    src/api/gradeant_api.cpp
    src/core/compiler.cpp
    src/core/detection.cpp
    src/core/generation.cpp
    src/core/ir.cpp
  )
  target_compile_definitions(gradeant_core PRIVATE GRADEANT_BUILDING_LIBRARY)
  target_include_directories(gradeant_core PRIVATE ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(gradeant_core PRIVATE ${OpenCV_LIBS})
  set_target_properties(gradeant_core PROPERTIES OUTPUT_NAME gradeant_core)

  if(MSVC)
    target_compile_options(gradeant_core PRIVATE /W4 /permissive-)
  else()
    target_compile_options(gradeant_core PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  if(BUILD_TESTING)
    add_executable(gradeant_api_tests tests/api_tests.cpp)
    target_link_libraries(gradeant_api_tests PRIVATE gradeant_core ${OpenCV_LIBS})
    target_include_directories(gradeant_api_tests PRIVATE ${OpenCV_INCLUDE_DIRS})
    add_test(NAME gradeant_api_tests COMMAND gradeant_api_tests)
  endif()
endif()

if(BUILD_TESTING)
  add_executable(gradeant_ir_tests
    tests/ir_tests.cpp
    src/core/generation.cpp
    src/core/ir.cpp
  )
  target_include_directories(gradeant_ir_tests PRIVATE src/core)
  if(MSVC)
    target_compile_options(gradeant_ir_tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(gradeant_ir_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME gradeant_ir_tests COMMAND gradeant_ir_tests)
endif()
