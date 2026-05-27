// payload.cpp - DuckyScript-subset payload runner.
// Vendored from ESP32_BT_HID/payload.cpp (HAL tick adapted out for cypherbox).

#include "payload.h"
#include "hid_ble.h"
#include "bt_hid_config.h"
#include "hijel/HijelHID_BLEKeyboard.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>

namespace {

struct KeyName { const char* name; uint8_t code; };

const KeyName SPECIAL_KEYS[] = {
    {"ENTER",       KEY_RETURN},
    {"RETURN",      KEY_RETURN},
    {"TAB",         KEY_TAB},
    {"SPACE",       KEY_SPACE},
    {"ESC",         KEY_ESCAPE},
    {"ESCAPE",      KEY_ESCAPE},
    {"BACKSPACE",   KEY_BACKSPACE},
    {"DELETE",      KEY_DELETE},
    {"INSERT",      KEY_INSERT},
    {"HOME",        KEY_HOME},
    {"END",         KEY_END},
    {"PAGEUP",      KEY_PAGE_UP},
    {"PAGEDOWN",    KEY_PAGE_DOWN},
    {"UP",          KEY_UP},
    {"DOWN",        KEY_DOWN},
    {"LEFT",        KEY_LEFT},
    {"RIGHT",       KEY_RIGHT},
    {"CAPSLOCK",    KEY_CAPS_LOCK},
    {"PRINTSCREEN", KEY_PRINT_SCREEN},
    {"PAUSE",       KEY_PAUSE},
    {"APP",         KEY_APPLICATION},
    {"F1",  KEY_F1},  {"F2",  KEY_F2},  {"F3",  KEY_F3},  {"F4",  KEY_F4},
    {"F5",  KEY_F5},  {"F6",  KEY_F6},  {"F7",  KEY_F7},  {"F8",  KEY_F8},
    {"F9",  KEY_F9},  {"F10", KEY_F10}, {"F11", KEY_F11}, {"F12", KEY_F12},
};

struct MediaName { const char* name; uint16_t code; };

// Consumer/media keys. Sent via the HID consumer report (Report ID 2).
const MediaName MEDIA_KEYS[] = {
    {"VOLUP",       MEDIA_VOLUME_UP},
    {"VOLUMEUP",    MEDIA_VOLUME_UP},
    {"VOLDOWN",     MEDIA_VOLUME_DOWN},
    {"VOLUMEDOWN",  MEDIA_VOLUME_DOWN},
    {"MUTE",        MEDIA_MUTE},
    {"PLAY",        MEDIA_PLAY},
    {"PAUSE",       MEDIA_PAUSE},
    {"PLAYPAUSE",   MEDIA_PLAY_PAUSE},
    {"NEXTTRACK",   MEDIA_NEXT_TRACK},
    {"NEXT",        MEDIA_NEXT_TRACK},
    {"PREVTRACK",   MEDIA_PREV_TRACK},
    {"PREV",        MEDIA_PREV_TRACK},
    {"STOP",        MEDIA_STOP},
    {"EJECT",       MEDIA_EJECT},
    {"BRIGHTUP",    MEDIA_BRIGHTNESS_UP},
    {"BRIGHTDOWN",  MEDIA_BRIGHTNESS_DOWN},
};

// Delay (ms) inserted after every command; set by DEFAULTDELAY. Reset per run().
uint32_t g_defaultDelayMs = 0;

bool ieq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return false;
        ++a; ++b;
    }
    return *a == 0 && *b == 0;
}

uint8_t resolveSpecial(const char* tok) {
    if (!tok || !*tok) return 0;
    for (auto& k : SPECIAL_KEYS) {
        if (ieq(tok, k.name)) return k.code;
    }
    return 0;
}

uint8_t resolveModifier(const char* tok) {
    if (ieq(tok, "CTRL") || ieq(tok, "CONTROL")) return KEY_MOD_LCTRL;
    if (ieq(tok, "ALT"))                          return KEY_MOD_LALT;
    if (ieq(tok, "SHIFT"))                        return KEY_MOD_LSHIFT;
    if (ieq(tok, "GUI") || ieq(tok, "WIN") ||
        ieq(tok, "WINDOWS") || ieq(tok, "CMD") ||
        ieq(tok, "COMMAND") || ieq(tok, "META"))  return KEY_MOD_LGUI;
    return 0;
}

uint16_t resolveMedia(const char* tok) {
    if (!tok || !*tok) return 0;
    for (auto& k : MEDIA_KEYS) {
        if (ieq(tok, k.name)) return k.code;
    }
    return 0;
}

uint8_t asciiToKey(char c) {
    if (c >= 'a' && c <= 'z') return KEY_A + (c - 'a');
    if (c >= 'A' && c <= 'Z') return KEY_A + (c - 'A');
    if (c >= '1' && c <= '9') return KEY_1 + (c - '1');
    if (c == '0')             return KEY_0;
    switch (c) {
        case ' ':  return KEY_SPACE;
        case '\t': return KEY_TAB;
        case '\n': return KEY_RETURN;
        case '-':  return KEY_MINUS;
        case '=':  return KEY_EQUAL;
        case '[':  return KEY_LEFTBRACE;
        case ']':  return KEY_RIGHTBRACE;
        case '\\': return KEY_BACKSLASH;
        case ';':  return KEY_SEMICOLON;
        case '\'': return KEY_APOSTROPHE;
        case '`':  return KEY_GRAVE;
        case ',':  return KEY_COMMA;
        case '.':  return KEY_DOT;
        case '/':  return KEY_SLASH;
    }
    return 0;
}

void trimLine(std::string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' ||
                          s.back() == ' '  || s.back() == '\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i) s.erase(0, i);
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == ' ' || c == '\t') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::string remainderAfter(const std::string& line, size_t skipTokens) {
    size_t i = 0;
    size_t skipped = 0;
    while (i < line.size() && skipped < skipTokens) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
        ++skipped;
    }
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    return line.substr(i);
}

void execCombo(uint8_t mods, uint8_t key) {
    if (key) {
        hidx::tap(key, mods);
    } else if (mods) {
        hidx::tap(KEY_NONE, mods);
    }
}

// Parse a sequence of modifier tokens followed by an optional key token,
// starting at toks[start]. Returns true if anything was resolved.
bool parseCombo(const std::vector<std::string>& toks, size_t start,
                uint8_t& mods, uint8_t& key) {
    mods = 0; key = 0;
    size_t i = start;
    while (i < toks.size()) {
        uint8_t m = resolveModifier(toks[i].c_str());
        if (!m) break;
        mods |= m;
        ++i;
    }
    if (i < toks.size()) {
        const char* kt = toks[i].c_str();
        key = resolveSpecial(kt);
        if (!key && !kt[1]) key = asciiToKey(kt[0]);
    }
    return mods || key;
}

bool execLine(const std::string& raw);

bool execLineWithRepeatContext(const std::string& raw, std::string& prevExecuted) {
    auto toks = tokenize(raw);
    if (toks.empty()) return true;

    if (ieq(toks[0].c_str(), "REPEAT")) {
        if (toks.size() < 2 || prevExecuted.empty()) return true;
        int n = atoi(toks[1].c_str());
        for (int i = 0; i < n; ++i) {
            if (!execLine(prevExecuted)) return false;
        }
        return true;
    }

    bool ok = execLine(raw);
    if (ok) prevExecuted = raw;
    return ok;
}

bool execLine(const std::string& raw) {
    auto toks = tokenize(raw);
    if (toks.empty()) return true;
    const char* head = toks[0].c_str();

    if (head[0] == '#' || ieq(head, "REM")) return true;

    if (ieq(head, "STRING")) {
        hidx::writeString(remainderAfter(raw, 1).c_str());
        return true;
    }
    if (ieq(head, "STRINGLN")) {
        hidx::writeString(remainderAfter(raw, 1).c_str());
        hidx::tap(KEY_RETURN);
        return true;
    }
    if (ieq(head, "DELAY")) {
        if (toks.size() >= 2) delay(atoi(toks[1].c_str()));
        return true;
    }
    if (ieq(head, "DEFAULTDELAY") || ieq(head, "DEFAULT_DELAY")) {
        if (toks.size() >= 2) g_defaultDelayMs = (uint32_t)atoi(toks[1].c_str());
        return true;
    }

    // Random character generators (DuckyScript 3).
    if (ieq(head, "RANDOM_LOWERCASE")) { hidx::writeChar('a' + random(26)); return true; }
    if (ieq(head, "RANDOM_UPPERCASE")) { hidx::writeChar('A' + random(26)); return true; }
    if (ieq(head, "RANDOM_NUMBER"))    { hidx::writeChar('0' + random(10)); return true; }
    if (ieq(head, "RANDOM_SPECIAL")) {
        static const char sp[] = "!@#$%^&*()-_=+[]{};:,.<>/?";
        hidx::writeChar(sp[random(sizeof(sp) - 1)]);
        return true;
    }
    if (ieq(head, "RANDOM_CHAR")) {
        int r = random(62);
        char c = r < 26 ? 'a' + r : (r < 52 ? 'A' + (r - 26) : '0' + (r - 52));
        hidx::writeChar(c);
        return true;
    }

    // Key holds: HOLD <mods/key>, RELEASE [mods/key], RELEASEALL.
    if (ieq(head, "RELEASEALL")) { hidx::releaseAll(); return true; }
    if (ieq(head, "HOLD") || ieq(head, "RELEASE")) {
        if (toks.size() == 1) { hidx::releaseAll(); return true; }
        uint8_t mods = 0, key = 0;
        parseCombo(toks, 1, mods, key);
        if (ieq(head, "HOLD")) hidx::pressKey(key, mods);
        else                   { if (key) hidx::releaseKey(key); else hidx::releaseAll(); }
        return true;
    }

    // Mouse (composite HID — same connection as keystrokes).
    if (ieq(head, "MOUSE_MOVE")) {
        if (toks.size() >= 3) hidx::mouseMove(atoi(toks[1].c_str()), atoi(toks[2].c_str()));
        return true;
    }
    if (ieq(head, "MOUSE_SCROLL")) {
        if (toks.size() >= 2) hidx::mouseScroll(atoi(toks[1].c_str()));
        return true;
    }
    if (ieq(head, "MOUSE_CLICK")) {
        uint8_t btn = HID_MOUSE_LEFT;
        if (toks.size() >= 2) {
            if (ieq(toks[1].c_str(), "RIGHT"))  btn = HID_MOUSE_RIGHT;
            else if (ieq(toks[1].c_str(), "MIDDLE")) btn = HID_MOUSE_MIDDLE;
        }
        hidx::mouseClick(btn);
        return true;
    }

    if (toks.size() == 1) {
        uint8_t k = resolveSpecial(head);
        if (k) { hidx::tap(k); return true; }
        uint16_t m = resolveMedia(head);
        if (m) { hidx::tapMedia(m); return true; }
        if (!head[1]) { hidx::writeChar(head[0]); return true; }
        Serial.printf("payload: unknown token '%s'\n", head);
        return true;
    }

    uint8_t mods = 0;
    size_t i = 0;
    while (i < toks.size()) {
        uint8_t m = resolveModifier(toks[i].c_str());
        if (!m) break;
        mods |= m;
        ++i;
    }
    if (i >= toks.size()) {
        execCombo(mods, 0);
        return true;
    }
    const char* keyTok = toks[i].c_str();
    uint8_t key = resolveSpecial(keyTok);
    if (!key && !keyTok[1]) key = asciiToKey(keyTok[0]);
    if (!key) {
        Serial.printf("payload: cannot resolve key '%s'\n", keyTok);
        return true;
    }
    execCombo(mods, key);
    return true;
}

}  // namespace

namespace payload {

bool run(const std::string& script) {
    if (!hidx::isConnected()) {
        Serial.println("payload: BLE host not connected, aborting");
        return false;
    }
    g_defaultDelayMs = 0;
    std::string prev;
    std::string line;
    line.reserve(128);
    for (size_t i = 0; i <= script.size(); ++i) {
        char c = (i < script.size()) ? script[i] : '\n';
        if (c == '\n') {
            trimLine(line);
            if (!line.empty()) {
                execLineWithRepeatContext(line, prev);
                bool wasDelay = (line.size() >= 5) &&
                    (line[0] == 'D' || line[0] == 'd') &&
                    (line[1] == 'E' || line[1] == 'e') &&
                    (line[2] == 'L' || line[2] == 'l') &&
                    (line[3] == 'A' || line[3] == 'a') &&
                    (line[4] == 'Y' || line[4] == 'y');
                if (!wasDelay) {
                    delay(BT_HID_MIN_ACTION_GAP_MS);
                    if (g_defaultDelayMs) delay(g_defaultDelayMs);
                }
            }
            line.clear();
        } else {
            line += c;
        }
    }
    return true;
}

}  // namespace payload
