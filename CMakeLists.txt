cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME apex)

project(${BUILD_NAME} CXX)

set(OpenGL_GL_PREFERENCE GLVND)

find_package(Threads)
find_package(OpenAL)
find_package(OpenGL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fpermissive")

file(GLOB_RECURSE SOURCES "ApexEngineV2/*.cpp" "ApexEngineV2/*.cpp")
file(GLOB_RECURSE HEADERS "ApexEngineV2/*.h")

set(CMAKE_CXX_STANDARD 17)
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${OPENAL_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/ApexEngineV2
)

add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${BUILD_NAME} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_SOURCE_DIR}/libnoise.a GLEW ${OPENAL_LIBRARY} ${OPENGL_LIBRARIES} glfw m)
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
