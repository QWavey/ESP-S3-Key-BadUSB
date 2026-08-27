#ifndef USB_MANAGER_H
#define USB_MANAGER_H

#include "GlobalState.h"

KeyCode parseKeyCode(String keyCodeStr);
void fastPressKey(String key);
void fastPressKeyCombination(std::vector<String> keys);
void fastTypeString(String text);
void handleKeyInput(String line);
void pressKeyOnly(String key);
void releaseAllKeys();

// Stealth HID (silent startup): present/remove the USB keyboard on demand.
void hidDetach();            // pull the device off the bus (host sees an unplug)
bool hidAttach();            // re-present and wait until the host enumerates it
void ensureHidReady();       // attach + settle only if currently detached
void hidReleaseIfSilent();   // detach again after typing, when silentStartup is on

#endif // USB_MANAGER_H
