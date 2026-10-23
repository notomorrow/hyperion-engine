# Windows half of HyperionSdk.cmake, copied beside it as HyperionSdkPlatform.cmake

set(HYPERION_SDK_RUNTIME_LIBRARY_PATTERN "*.dll")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

function(hyperion_sdk_packaged_libraries sdk_root runtime_dir out_var)
  set(${out_var} "${sdk_root}/Lib/hyperion.lib" PARENT_SCOPE)
endfunction()

# the NativeAOT runtime is built against the static VC runtime with the dynamic UCRT, so the executable linking it is as well.
function(hyperion_platform_link_scripts target link_inputs)
  file(TO_CMAKE_PATH "${link_inputs}" link_inputs)

  set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
  target_link_libraries(${target} PRIVATE ${link_inputs})
  target_link_options(${target} PRIVATE /NODEFAULTLIB:libucrt.lib /DEFAULTLIB:ucrt.lib)
endfunction()
