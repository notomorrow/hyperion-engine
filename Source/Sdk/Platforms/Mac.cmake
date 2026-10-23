# macOS half of HyperionSdk.cmake, copied beside it as HyperionSdkPlatform.cmake

set(HYPERION_SDK_RUNTIME_LIBRARY_PATTERN "*.dylib")

# the engine libraries are copied beside the game, so it finds them there wherever it's moved to
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "@executable_path")

function(hyperion_sdk_packaged_libraries sdk_root runtime_dir out_var)
  file(GLOB libraries "${runtime_dir}/libhyperion.dylib" "${runtime_dir}/libhyperion-core.dylib")
  set(${out_var} ${libraries} PARENT_SCOPE)
endfunction()

function(hyperion_platform_link_scripts target link_inputs)
  target_link_libraries(${target} PRIVATE ${link_inputs})
endfunction()
