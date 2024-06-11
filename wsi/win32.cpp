#include <format>
#include <stdexcept>
#include <windows.h>


void EnableHIDPI() {
    // Prevent Windows from auto stretching content; just want a raw rectangle of pixels
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        auto error = GetLastError();
        if (error != ERROR_ACCESS_DENIED) { // ERROR_ACCESS_DENIED == already set; ignore error
            throw std::runtime_error(std::format("WIN32: Failed to set dpi awareness: {}\n", error));
        }
    }
}
