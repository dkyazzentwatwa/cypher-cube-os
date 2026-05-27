// hid_ble.cpp - BLE-HID keyboard wrapper for Cypherbox Mini
// Vendored from ESP32_BT_HID/hid_ble.cpp

#include "hid_ble.h"
#include "bt_hid_config.h"
#include "hijel/HijelHID_BLEKeyboard.h"

namespace {
HijelHID_BLEKeyboard kb(BT_HID_DEVICE_NAME, BT_HID_DEVICE_MANUF, BT_HID_DEVICE_BATTERY);
}

namespace hidx {

void init(const char* deviceName, const char* manufacturer, uint8_t battery) {
    (void)deviceName; (void)manufacturer; (void)battery;
    kb.begin();
}

void tick() {}

bool isConnected() { return kb.isConnected(); }

void writeChar(char c) {
    if (!kb.isConnected()) return;
    kb.write((uint8_t)c);
}

void writeString(const char* s) {
    if (!kb.isConnected() || !s) return;
    while (*s) {
        kb.write((uint8_t)*s);
        delay(BT_HID_KEY_INTERCHAR_MS);
        ++s;
    }
}

void tap(uint8_t keycode, uint8_t modifiers) {
    if (!kb.isConnected()) return;
    kb.tap(keycode, modifiers);
}

void tapMedia(uint16_t usageId) {
    if (!kb.isConnected()) return;
    kb.tap(usageId);
}

static int8_t clamp8(int v) { return (int8_t)(v < -127 ? -127 : (v > 127 ? 127 : v)); }

void mouseMove(int dx, int dy, int wheel) {
    if (!kb.isConnected()) return;
    kb.mouseMove(clamp8(dx), clamp8(dy), clamp8(wheel));
}

void mouseClick(uint8_t button) {
    if (!kb.isConnected()) return;
    kb.mouseClick(button);
}

void mouseScroll(int amount) {
    if (!kb.isConnected()) return;
    kb.mouseMove(0, 0, clamp8(amount));
}

void pressKey(uint8_t keycode, uint8_t modifiers) {
    if (!kb.isConnected()) return;
    kb.press(keycode, modifiers);
}

void releaseKey(uint8_t keycode) {
    if (!kb.isConnected()) return;
    kb.release(keycode);
}

void releaseAll() {
    if (!kb.isConnected()) return;
    kb.releaseAll();
}

void onPasskey(void (*cb)(uint32_t passkey)) {
    kb.onPassKey(cb);
}

bool isBonded() {
    return kb.isBonded();
}

void clearBonds() {
    kb.clearBonds();
}

}  // namespace hidx
