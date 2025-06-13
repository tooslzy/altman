#pragma once
#ifdef _WIN32
#include <windows.h>
inline bool ConfirmAction(const char* msg) {
    return MessageBoxA(NULL, msg, "Confirm", MB_ICONWARNING | MB_YESNO) == IDYES;
}
#else
inline bool ConfirmAction(const char* msg) { return true; }
#endif
