cmake_minimum_required(VERSION 3.27)
project(Solution)

find_package(Vulkan REQUIRED)

# Setup: volk
add_subdirectory(ThirdParty/volk)
target_include_directories(volk PUBLIC ${Vulkan_INCLUDE_DIRS})

# Setup: vk-bootstrap
add_subdirectory(ThirdParty/vk-bootstrap)

# Setup: GLFW
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/glfw)

# Setup: GLM
add_subdirectory(ThirdParty/glm)
target_compile_definitions(glm INTERFACE
    GLM_FORCE_AVX2
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# Setup: VMA
set(VMA_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/VulkanMemoryAllocator)
add_library(vma STATIC Source/BuildVma.cpp)
target_include_directories(vma PUBLIC ThirdParty/VulkanMemoryAllocator/include)
target_link_libraries(vma PUBLIC volk)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(vma PRIVATE -Wno-everything)
endif()

# Setup: spdlog
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/spdlog)
target_compile_options(spdlog PRIVATE $<$<BOOL:${MSVC}>:/w>)
target_compile_definitions(spdlog PRIVATE FMT_EXCEPTIONS=0)

# Setup: tracy
set(TRACY_CALLSTACK ON CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/tracy)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(TracyClient PRIVATE -Wno-everything)
endif()

add_subdirectory(Source)

# Setup: fastgltf
add_subdirectory(ThirdParty/fastgltf)

# Setup: stb
add_library(stb
    Source/Stbi.cpp
)
target_include_directories(stb SYSTEM PUBLIC ThirdParty/stb)

# Compile Shaders

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/data/shaders/*vs.glsl"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "Building Shaders")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/data/shaders/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} -V -S vert ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/data/shaders/*fs.glsl"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "Building Shaders")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/data/shaders/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} -V -S frag ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )