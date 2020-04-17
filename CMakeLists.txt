cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME apex)

project(${BUILD_NAME} CXX)

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_SOURCE_DIR}/lib)

find_package(Threads)
find_package(OpenGL)
find_package(OpenAL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -fpermissive")

file(GLOB_RECURSE SOURCES "ApexEngineV2/*.cpp" "ApexEngineV2/*.cpp" "ApexEngineV2/*.c")
file(GLOB_RECURSE HEADERS "ApexEngineV2/*.h")

set(CMAKE_CXX_STANDARD 17)
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${OPENGL_INCLUDE_DIRS}
  ${OPENAL_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/ApexEngineV2
)

add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${BUILD_NAME} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_SOURCE_DIR}/libnoise.a GLEW ${OPENAL_LIBRARY} ${OPENGL_LIBRARIES} glfw m)
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
