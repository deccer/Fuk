#pragma once

#if defined(__clang__)
#pragma clang diagnostic push
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#if defined(_MSC_VER)
#pragma warning(disable: 4100) // unreferenced formal parameter
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/stb/stb_image.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

