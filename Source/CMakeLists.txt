project(Fuk)

set(CMAKE_CXX_STANDARD 23)

if (MSVC)
    add_custom_target(copy_data ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE_INIT}/data)
else ()
    add_custom_target(copy_data ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/data)
endif ()

add_executable(Fuk
    Engine.cpp
	PipelineBuilder.cpp
	Mesh.cpp
    Main.cpp
)

if (MSVC)
    set_property(TARGET Fuk PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/")
endif ()

add_dependencies(Fuk Shaders copy_data)

target_compile_options(Fuk
	PRIVATE
	$<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
	-Wall
	-Wextra
	-Wno-missing-field-initializers
	-Wno-unused-result
	>
	$<$<CXX_COMPILER_ID:MSVC>:
	/W4
	/WX
	/permissive-
	/wd4324 # structure was padded
	>
)

target_compile_definitions(Fuk PRIVATE
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<BOOL:${WIN32}>:
  	_CRT_SECURE_NO_WARNINGS
  	WIN32_LEAN_AND_MEAN
  	NOMINMAX
  	TRACY_ENABLE
  	STBI_MSC_SECURE_CRT
	>
	VK_NO_PROTOTYPES  
)

target_include_directories(Fuk INTERFACE ${VULKAN_SDK}/include)
target_include_directories(Fuk PUBLIC ${CMAKE_BINARY_DIR}/VulkanMemoryAllocator-src/include)

target_include_directories(Fuk SYSTEM PRIVATE glm)

target_link_libraries(Fuk PRIVATE
	glm
	vma
	volk
	glfw
	spdlog
	TracyClient
	vk-bootstrap
	vk-bootstrap-compiler-warnings
	fastgltf
	stb
)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message("${_variableName}=${${_variableName}}")
#endforeach()