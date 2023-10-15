#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
/*
#define VMA_DEBUG_LOG_FORMAT(format, ...)                                                                                                                      \
    do {                                                                                                                                                         \
        printf((format), __VA_ARGS__);                                                                                                                             \
        printf("\n");                                                                                                                                              \
    } while (false)
*/
#include <volk.h>
#include <vk_mem_alloc.h>