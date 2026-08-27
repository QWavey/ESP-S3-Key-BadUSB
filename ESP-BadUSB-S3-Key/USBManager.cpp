#include "USBManager.h"
#include "DuckyInterpreter.h"
#include "esp32-hal-tinyusb.h"   // pulls in tusb.h -> tud_connect/tud_disconnect/tud_mounted

// ---- Stealth HID (silent startup) ------------------------------------------
// The ESP32-S3 USB-Serial/JTAG peripheral is separate hardware, so detaching
// the TinyUSB HID device does NOT affect flashing/serial. tud_disconnect()
// electrically removes the keyboard from the bus (host sees an unplug);
// tud_connect() re-presents it.

void hidDetach() {
  tud_disconnect();
  hidConnected = false;
  delay(50);
}

bool hidAttach() {
  tud_connect();
  hidConnected = true;
  unsigned long start = millis();
  while (!tud_mounted() && (millis() - start) < 2000) {
    delay(10);
  }
  delay(150);                 // let the host bind the HID driver before typing
  return tud_mounted();
}

void ensureHidReady() {
  if (!hidConnected || !tud_mounted()) {
    hidAttach();
  }
}

void hidReleaseIfSilent() {
  if (silentStartup) {
    delay(80);                 // flush any in-flight report before unplugging
    hidDetach();
  }
}

KeyCode parseKeyCode(String keyCodeStr) {
  KeyCode result = {0, 0};

  int firstComma = keyCodeStr.indexOf(',');
  int secondComma = keyCodeStr.indexOf(',', firstComma + 1);

  if (firstComma > 0 && secondComma > firstComma) {
    String modStr = keyCodeStr.substring(0, firstComma);
    String keyStr = keyCodeStr.substring(secondComma + 1);

    result.modifier = strtol(modStr.c_str(), NULL, 16);
    result.key = strtol(keyStr.c_str(), NULL, 16);
  }

  return result;
}

void fastPressKey(String key) {
  if (stopRequested) return;

  if (currentKeymap.find(key) != currentKeymap.end()) {
    KeyCode kc = parseKeyCode(currentKeymap[key]);

    if (kc.key == 0 && kc.modifier > 0) {
      if (kc.modifier & 0x01) { keyboard.press(KEY_LEFT_CTRL); delay(5); keyboard.release(KEY_LEFT_CTRL); }
      if (kc.modifier & 0x02) { keyboard.press(KEY_LEFT_SHIFT); delay(5); keyboard.release(KEY_LEFT_SHIFT); }
      if (kc.modifier & 0x04) { keyboard.press(KEY_LEFT_ALT); delay(5); keyboard.release(KEY_LEFT_ALT); }
      if (kc.modifier & 0x08) { keyboard.press(KEY_LEFT_GUI); delay(5); keyboard.release(KEY_LEFT_GUI); }
      if (kc.modifier & 0x10) { keyboard.press(KEY_RIGHT_CTRL); delay(5); keyboard.release(KEY_RIGHT_CTRL); }
      if (kc.modifier & 0x20) { keyboard.press(KEY_RIGHT_SHIFT); delay(5); keyboard.release(KEY_RIGHT_SHIFT); }
      if (kc.modifier & 0x40) { keyboard.press(KEY_RIGHT_ALT); delay(5); keyboard.release(KEY_RIGHT_ALT); }
      if (kc.modifier & 0x80) { keyboard.press(KEY_RIGHT_GUI); delay(5); keyboard.release(KEY_RIGHT_GUI); }
    } else {
      if (kc.modifier > 0) {
        if (kc.modifier & 0x01) keyboard.press(KEY_LEFT_CTRL);
        if (kc.modifier & 0x02) keyboard.press(KEY_LEFT_SHIFT);
        if (kc.modifier & 0x04) keyboard.press(KEY_LEFT_ALT);
        if (kc.modifier & 0x08) keyboard.press(KEY_LEFT_GUI);
        if (kc.modifier & 0x10) keyboard.press(KEY_RIGHT_CTRL);
        if (kc.modifier & 0x20) keyboard.press(KEY_RIGHT_SHIFT);
        if (kc.modifier & 0x40) keyboard.press(KEY_RIGHT_ALT);
        if (kc.modifier & 0x80) keyboard.press(KEY_RIGHT_GUI);
      }

      if (kc.key > 0) {
        keyboard.pressRaw(kc.key);
      }

      delay(5);
      keyboard.releaseAll();
    }
  } else {
    Serial.println("Key not found in keymap: " + key);
    lastError = "Key not found: " + key;
    errorCount++;
  }
}

void fastPressKeyCombination(std::vector<String> keys) {
  if (stopRequested) return;

  uint8_t combinedModifier = 0;
  uint8_t mainKey = 0;

  for (String key : keys) {
    if (currentKeymap.find(key) != currentKeymap.end()) {
      KeyCode kc = parseKeyCode(currentKeymap[key]);
      combinedModifier |= kc.modifier;
      if (kc.key > 0 && mainKey == 0) {
        mainKey = kc.key;
      }
    } else {
      Serial.println("Key not found in keymap: " + key);
      lastError = "Key not found: " + key;
      errorCount++;
    }
  }

  if (combinedModifier > 0) {
    if (combinedModifier & 0x01) keyboard.press(KEY_LEFT_CTRL);
    if (combinedModifier & 0x02) keyboard.press(KEY_LEFT_SHIFT);
    if (combinedModifier & 0x04) keyboard.press(KEY_LEFT_ALT);
    if (combinedModifier & 0x08) keyboard.press(KEY_LEFT_GUI);
    if (combinedModifier & 0x10) keyboard.press(KEY_RIGHT_CTRL);
    if (combinedModifier & 0x20) keyboard.press(KEY_RIGHT_SHIFT);
    if (combinedModifier & 0x40) keyboard.press(KEY_RIGHT_ALT);
    if (combinedModifier & 0x80) keyboard.press(KEY_RIGHT_GUI);
  }

  if (mainKey > 0) {
    keyboard.pressRaw(mainKey);
  }

  delay(5);
  keyboard.releaseAll();
}

void fastTypeString(String text) {
  if (stopRequested) return;

  String processedText = processVariables(text);

  for (int i = 0; i < processedText.length(); i++) {
    if (stopRequested) break;

    String ch = String(processedText.charAt(i));

    if (currentKeymap.find(ch) != currentKeymap.end()) {
      KeyCode kc = parseKeyCode(currentKeymap[ch]);

      if (kc.modifier > 0) {
        if (kc.modifier & 0x01) keyboard.press(KEY_LEFT_CTRL);
        if (kc.modifier & 0x02) keyboard.press(KEY_LEFT_SHIFT);
        if (kc.modifier & 0x04) keyboard.press(KEY_LEFT_ALT);
        if (kc.modifier & 0x08) keyboard.press(KEY_LEFT_GUI);
        if (kc.modifier & 0x10) keyboard.press(KEY_RIGHT_CTRL);
        if (kc.modifier & 0x20) keyboard.press(KEY_RIGHT_SHIFT);
        if (kc.modifier & 0x40) keyboard.press(KEY_RIGHT_ALT);
        if (kc.modifier & 0x80) keyboard.press(KEY_RIGHT_GUI);
      }

      if (kc.key > 0) {
        keyboard.pressRaw(kc.key);
      }

      delay(2);
      keyboard.releaseAll();

      if (delayBetweenKeys > 0) {
        delay(delayBetweenKeys);
      } else {
        delay(2);
      }
    } else {
      Serial.println("Character not found in keymap: " + ch);
      lastError = "Character not found: " + ch;
      errorCount++;
    }
  }
}

void handleKeyInput(String line) {
  std::vector<String> keys;
  int startIdx = 0;
  int spaceIdx = line.indexOf(' ');

  while (spaceIdx != -1) {
    keys.push_back(line.substring(startIdx, spaceIdx));
    startIdx = spaceIdx + 1;
    spaceIdx = line.indexOf(' ', startIdx);
  }
  if (startIdx < line.length()) {
    keys.push_back(line.substring(startIdx));
  }

  if (keys.size() == 1) {
    fastPressKey(keys[0]);
  } else {
    fastPressKeyCombination(keys);
  }
}

void pressKeyOnly(String key) {
  if (stopRequested) return;
  if (currentKeymap.find(key) != currentKeymap.end()) {
    KeyCode kc = parseKeyCode(currentKeymap[key]);
    if (kc.modifier > 0) {
      if (kc.modifier & 0x01) keyboard.press(KEY_LEFT_CTRL);
      if (kc.modifier & 0x02) keyboard.press(KEY_LEFT_SHIFT);
      if (kc.modifier & 0x04) keyboard.press(KEY_LEFT_ALT);
      if (kc.modifier & 0x08) keyboard.press(KEY_LEFT_GUI);
      if (kc.modifier & 0x10) keyboard.press(KEY_RIGHT_CTRL);
      if (kc.modifier & 0x20) keyboard.press(KEY_RIGHT_SHIFT);
      if (kc.modifier & 0x40) keyboard.press(KEY_RIGHT_ALT);
      if (kc.modifier & 0x80) keyboard.press(KEY_RIGHT_GUI);
    }
    if (kc.key > 0) keyboard.pressRaw(kc.key);
  }
}

void releaseAllKeys() {
  keyboard.releaseAll();
}
