project(Fuk)

find_package(Vulkan REQUIRED)

add_executable(Fuk
    Main.cpp
)
target_include_directories(Fuk INTERFACE ${VULKAN_SDK}/include)
target_link_libraries(Fuk PRIVATE glfw vk-bootstrap vk-bootstrap-compiler-warnings vk-bootstrap-vulkan-headers stb_image)