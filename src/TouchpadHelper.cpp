#include "TouchpadHelper.h"

#include <windows.h>
#include <hidsdi.h>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <memory>


namespace RawInput {

// Internal helper: TouchpadContactCreator to accumulate contact fields.
class TouchpadContactCreator {
public:
    ULONG contactId = 0;
    ULONG x = 0;
    ULONG y = 0;
    bool hasContactId = false;
    bool hasX = false;
    bool hasY = false;

    bool tryCreate(TouchpadContact &contact) const {
        if (hasContactId && hasX && hasY) {
            contact.contactId = contactId;
            contact.x = x;
            contact.y = y;
            return true;
        }
        return false;
    }

    void clear() {
        contactId = 0;
        x = 0;
        y = 0;
        hasContactId = false;
        hasX = false;
        hasY = false;
    }
};


bool TouchpadHelper::exists() {
    UINT deviceCount = 0;
    constexpr UINT size = sizeof(RAWINPUTDEVICELIST);

    // First call with nullptr to get the number of devices.
    if (GetRawInputDeviceList(nullptr, &deviceCount, size) != 0) {
        return false;
    }

    std::vector<RAWINPUTDEVICELIST> devices(deviceCount);
    if (GetRawInputDeviceList(devices.data(), &deviceCount, size) != deviceCount) {
        return false;
    }

    for (const auto &device : std::as_const(devices)) {
        if (device.dwType == RIM_TYPEHID) {
            UINT infoSize = 0;
            // Get the size required for the device info.
            if (GetRawInputDeviceInfoW(device.hDevice, RIDI_DEVICEINFO, nullptr, &infoSize) != 0) {
                continue;
            }
            RID_DEVICE_INFO deviceInfo{};
            deviceInfo.cbSize = infoSize;
            if (GetRawInputDeviceInfoW(device.hDevice, RIDI_DEVICEINFO, &deviceInfo, &infoSize) == static_cast<UINT>(-1)) {
                continue;
            }
            // Check for Windows Precision Touchpad
            if ((deviceInfo.hid.usUsagePage == HID_USAGE_PAGE_DIGITIZER)
                    && (deviceInfo.hid.usUsage == HID_USAGE_DIGITIZER_TOUCH_PAD)) {
                return true;
            }
        }
    }
    return false;
}

bool TouchpadHelper::registerInput(HWND hwnd) {
    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x0D; // Digitizer Page
    device.usUsage = 0x05; // Touch Pad
    device.dwFlags = 0;  // WM_INPUT only when in foreground.
    device.hwndTarget = hwnd;

    return RegisterRawInputDevices(&device, 1, sizeof(device));
}

std::vector<TouchpadContact> TouchpadHelper::parseInput(HRAWINPUT hRawInput) {

    std::vector<TouchpadContact> contacts;

    UINT rawInputSize = 0;
    constexpr UINT headerSize = sizeof(RAWINPUTHEADER);

    // Get the size of the raw input data.
    if (GetRawInputData(hRawInput, RID_INPUT, nullptr, &rawInputSize, headerSize) != 0) {
        return {};
    }

    if (rawInputSize == 0) {
        return {};
    }

    // Use a unique pointer to manage raw input data memory.
    auto rawDataBuffer = std::make_unique<BYTE[]>(rawInputSize);
    if (GetRawInputData(hRawInput, RID_INPUT, rawDataBuffer.get(), &rawInputSize, headerSize) != rawInputSize) {
        return {};
    }

    // Interpret the data as a RAWINPUT structure.
    const auto pRawInput = reinterpret_cast<RAWINPUT *>(rawDataBuffer.get());

    // Compute the length of the HID raw data.
    const UINT hidDataLength = pRawInput->data.hid.dwSizeHid * pRawInput->data.hid.dwCount;
    if (rawInputSize < sizeof(RAWINPUT) || hidDataLength > rawInputSize) {
        return {};
    }

    // Get pointer to HID raw data.
    BYTE *hidRawData = rawDataBuffer.get() + (rawInputSize - hidDataLength);

    // Retrieve preparsed data size.
    UINT preparsedDataSize = 0;
    if (GetRawInputDeviceInfoW(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, nullptr, &preparsedDataSize) != 0) {
        return {};
    }

    if (preparsedDataSize == 0) {
        return {};
    }

    // Use a unique pointer to manage preparsed data memory.
    auto preparsedDataBuffer = std::make_unique<BYTE[]>(preparsedDataSize);
    if (GetRawInputDeviceInfoW(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, preparsedDataBuffer.get(), &preparsedDataSize) != preparsedDataSize) {
        return {};
    }

    const auto pData = reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedDataBuffer.get());

    // Get the HID capabilities.
    HIDP_CAPS caps = {};
    if (HidP_GetCaps(pData, &caps) != HIDP_STATUS_SUCCESS) {
        return {};
    }

    USHORT valueCapsLength = caps.NumberInputValueCaps;
    std::vector<HIDP_VALUE_CAPS> valueCaps(valueCapsLength);
    if (HidP_GetValueCaps(HidP_Input, valueCaps.data(), &valueCapsLength, pData) != HIDP_STATUS_SUCCESS) {
        return {};
    }

    UINT contactCount = 0;
    TouchpadContactCreator creator;

    // Sort valueCaps by LinkCollection
    std::sort(valueCaps.begin(), valueCaps.end(), [](const HIDP_VALUE_CAPS &a, const HIDP_VALUE_CAPS &b) {
        return a.LinkCollection < b.LinkCollection;
    });

    // Process each value cap.
    for (const auto &cap : valueCaps) {
        ULONG usageValue = 0;
        const USAGE usage = cap.IsRange ? cap.Range.UsageMin : cap.NotRange.Usage;
        if (HidP_GetUsageValue(HidP_Input,
                               cap.UsagePage,
                               cap.LinkCollection,
                               usage,
                               &usageValue,
                               pData,
                               reinterpret_cast<PCHAR>(hidRawData),
                               hidDataLength) != HIDP_STATUS_SUCCESS)
        {
            continue;
        }

        /* Reference for HID Usage Tables: https://www.usb.org/hid
           # Generic Desktop Page (0x01)
             - X (0x30)
             - Y (0x31)
           # Digitizers Page (0x0D)
             - Contact Identifier (0x51)
             - Contact Count (0x54)
         */

        // For LinkCollection 0 (Physical) , look for contact count.
        if (cap.LinkCollection == 0) {
            if (cap.UsagePage == 0x0D && usage == 0x54) { // Contact Count
                contactCount = usageValue;
            }
        }
        else {
            // For other LinkCollections, extract contact details.
            if (cap.UsagePage == 0x0D && usage == 0x51) { // Contact Identifier
                creator.contactId = usageValue;
                creator.hasContactId = true;
            }
            else if (cap.UsagePage == 0x01 && usage == 0x30) { // X
                creator.x = usageValue;
                creator.hasX = true;
            }
            else if (cap.UsagePage == 0x01 && usage == 0x31) { // Y
                creator.y = usageValue;
                creator.hasY = true;
            }
        }

        // If all required fields are available, create a contact.
        TouchpadContact contact{};
        if (creator.tryCreate(contact)) {
            contacts.push_back(contact);
            if (contacts.size() >= contactCount) {
                break;
            }
            creator.clear();
        }
    }

    return contacts;
}

} // namespace RawInput
