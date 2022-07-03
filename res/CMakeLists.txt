#cmake_minimum_required(VERSION 3.5)
#cmake_policy(SET CMP0091 NEW)
#
#set(GLSL_VALIDATOR glslc)
#
#file(GLOB_RECURSE GLSL_SOURCE_FILES
#        "vkshaders/*.frag"
#        "vkshaders/*.vert"
#)
#
#foreach(GLSL ${GLSL_SOURCE_FILES})
#    get_filename_component(FILE_NAME, ${GLSL} NAME)
#    set(shader_path GLSL)
#    set(output_path "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
#    add_custom_command(
#        OUTPUT ${output_path}
#        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/vkshaders/"
#        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
#        DEPENDS ${GLSL}
#    )
#    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
#endforeach(GLSL)
#add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})