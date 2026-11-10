cmake_minimum_required(VERSION 3.16)
project(gradeant_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

add_library(gradeant_core SHARED src/core/compiler.cpp)
target_compile_definitions(gradeant_core PRIVATE GRADEANT_BUILDING_LIBRARY)
target_include_directories(gradeant_core PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(gradeant_core PRIVATE ${OpenCV_LIBS})
set_target_properties(gradeant_core PROPERTIES OUTPUT_NAME gradeant_core)

if(MSVC)
  target_compile_options(gradeant_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(gradeant_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
