project(Fuk)

set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan REQUIRED)

if (MSVC)
    add_custom_target(copy_data ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE_INIT}/data)
else ()
    add_custom_target(copy_data ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/data)
endif ()

add_executable(Fuk
    Main.cpp
)

if (MSVC)
    set_property(TARGET Fuk PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/")
    target_compile_options(stb_image INTERFACE /w)
    target_compile_options(glfw INTERFACE /w)
endif ()

add_dependencies(Fuk copy_data)
target_include_directories(Fuk INTERFACE ${VULKAN_SDK}/include)
target_link_libraries(Fuk PRIVATE glfw vk-bootstrap vk-bootstrap-compiler-warnings vk-bootstrap-vulkan-headers stb_image)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message("${_variableName}=${${_variableName}}")
#endforeach()