cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME apex)

project(${BUILD_NAME} CXX C)

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib)

find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Bullet REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Og -fpermissive")

file(GLOB_RECURSE SOURCES "ApexEngineV2/*.cpp" "ApexEngineV2/*.c")
file(GLOB_RECURSE HEADERS "ApexEngineV2/*.h")

set(CMAKE_CXX_STANDARD 17)
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${OPENGL_INCLUDE_DIRS}
  ${OPENAL_INCLUDE_DIR}
  ${BULLET_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/ApexEngineV2
)
add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS})

# if(APPLE)
  # target_link_libraries(${BUILD_NAME} ${CMAKE_THREAD_LIBS_INIT} ${OPENAL_LIBRARY} ${OPENGL_LIBRARIES} glfw m)
# else()
  target_link_libraries(${BUILD_NAME} ${CMAKE_THREAD_LIBS_INIT} ${OPENAL_LIBRARY} ${OPENGL_LIBRARIES} GLEW glfw m)
# endif()
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
