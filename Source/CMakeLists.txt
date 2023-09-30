project(Fuk)

find_package(Vulkan REQUIRED)

add_custom_target(copy_data ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/data)   

add_executable(Fuk
    Main.cpp
)
add_dependencies(Fuk copy_data)
target_include_directories(Fuk INTERFACE ${VULKAN_SDK}/include)
target_link_libraries(Fuk PRIVATE glfw vk-bootstrap vk-bootstrap-compiler-warnings vk-bootstrap-vulkan-headers stb_image)