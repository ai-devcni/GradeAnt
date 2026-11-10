#pragma once

#if defined(_WIN32)
#  if defined(GRADEANT_BUILDING_LIBRARY)
#    define GRADEANT_API __declspec(dllexport)
#  else
#    define GRADEANT_API __declspec(dllimport)
#  endif
#else
#  define GRADEANT_API __attribute__((visibility("default")))
#endif

// Compile a layout image into a self-contained HTML document. The returned
// pointer remains valid until the next call on the same thread. Copy it before
// making another call from that thread.
extern "C" GRADEANT_API const char* compile_image_layout(const char* image_path);

// Read the most recent error on the calling thread; an empty string means
// the preceding compile call succeeded. The pointer has the same lifetime rule.
extern "C" GRADEANT_API const char* get_last_error();
