// hid_ble.h - BLE-HID keyboard wrapper for Cypherbox Mini
// Vendored from ESP32_BT_HID/hid.h

#pragma once

#include <Arduino.h>

namespace hidx {

void init(const char* deviceName, const char* manufacturer, uint8_t battery);
void tick();
bool isConnected();

void writeChar(char c);
void writeString(const char* s);
void tap(uint8_t keycode, uint8_t modifiers = 0);

// Consumer/media keys (MEDIA_* usage IDs from BLEHIDMediaKeys.h).
void tapMedia(uint16_t usageId);

// Mouse (composite Report ID 3, same connection as the keyboard).
// button is a HID_MOUSE_* bit (LEFT=1, RIGHT=2, MIDDLE=4).
void mouseMove(int dx, int dy, int wheel = 0);
void mouseClick(uint8_t button);
void mouseScroll(int amount);

// Key holds (DuckyScript HOLD/RELEASE). keycode may be KEY_NONE for a
// modifier-only hold; modifiers is an OR of KEY_MOD_* bits.
void pressKey(uint8_t keycode, uint8_t modifiers = 0);
void releaseKey(uint8_t keycode);
void releaseAll();

// Pairing / bond management.
void onPasskey(void (*cb)(uint32_t passkey));   // called when host requests a passkey
bool isBonded();                                // a host bond is stored
void clearBonds();                              // forget all bonded hosts

}  // namespace hidx
