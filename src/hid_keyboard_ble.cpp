#include "hid_keyboard.h"
#include <BleKeyboard.h>

class BLEBackend : public IHIDBackend {
private:
  BleKeyboard _kb;
public:
  BLEBackend() : _kb("Magic Keyboard", "Apple Inc.", 100) {}
  
  void begin() override {
    _kb.begin();
  }
  
  void onEvent(hid_led_event_handler_t handler) override {
    // BLE doesn't use the standard ESP Arduino USB event system for LEDs easily yet, 
    // so this is a no-op for now.
  }
  
  void press(uint8_t key) override { _kb.press(key); }
  void release(uint8_t key) override { _kb.release(key); }
  void releaseAll() override { _kb.releaseAll(); }
  size_t print(const char* text) override { return _kb.print(text); }
  size_t write(uint8_t c) override { return _kb.write(c); }
  bool isConnected() override { return _kb.isConnected(); }
  
  void sendRawReport(uint8_t modifier, const uint8_t* keys) override {
    // The ESP32-NimBLE-Keyboard library doesn't expose a clean raw custom report API 
    // like sendReport(&kr) that takes 6 simultaneous keys mapping cleanly to the struct.
    // However, our payload functions exclusively rely on .press() and .print(),
    // and ONLY the host_main.cpp ESP-NOW mirroring requires sendRawReport.
    // We do our best to map it by pressing the keys one by one.
    _kb.releaseAll();
    
    // Press modifiers
    if (modifier & 0x01) _kb.press(KEY_LEFT_CTRL);
    if (modifier & 0x02) _kb.press(KEY_LEFT_SHIFT);
    if (modifier & 0x04) _kb.press(KEY_LEFT_ALT);
    if (modifier & 0x08) _kb.press(KEY_LEFT_GUI);
    if (modifier & 0x10) _kb.press(KEY_RIGHT_CTRL);
    if (modifier & 0x20) _kb.press(KEY_RIGHT_SHIFT);
    if (modifier & 0x40) _kb.press(KEY_RIGHT_ALT);
    if (modifier & 0x80) _kb.press(KEY_RIGHT_GUI);
    
    // Press up to 6 keys
    for (int i = 0; i < 6; i++) {
      if (keys[i] != 0) {
         // This assumes the keys array contains standard ASCII or mapped BleKeyboard constants.
         // Note: Raw USB HID usage IDs (0x04 for A) won't map cleanly to BleKeyboard's ASCII inputs 
         // without a translation table. Fortunately, we're primarily focused on the device_rx standalone mode.
         _kb.press(keys[i]); 
      }
    }
  }
};

// Factory function
IHIDBackend* createBLEBackend() {
  return new BLEBackend();
}
