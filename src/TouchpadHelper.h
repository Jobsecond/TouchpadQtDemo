#ifndef TOUCHPAD_HELPER_H
#define TOUCHPAD_HELPER_H

#include <Windows.h>
#include <vector>

namespace RawInput {
    struct TouchpadContact {
        ULONG contactId = 0;
        ULONG x = 0;
        ULONG y = 0;
    };

    class TouchpadHelper {
    public:
        // Returns true if a Windows Precision Touchpad exists on the system.
        static bool exists();

        // Registers the touchpad as a raw input device for the given window handle.
        // Returns true on success.
        static bool registerInput(HWND hwnd);

        // Parses the raw input from a WM_INPUT message and returns touchpad information.
        static std::vector<TouchpadContact> parseInput(HRAWINPUT hRawInput);
    };

} // namespace RawInput

#endif // TOUCHPAD_HELPER_H
