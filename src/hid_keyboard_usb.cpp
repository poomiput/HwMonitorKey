#include "hid_keyboard.h"
#include <USB.h>
#include <USBHIDKeyboard.h>

static hid_led_event_handler_t _userLedHandler = nullptr;

static void _usbLedCallback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data) {
  if (_userLedHandler) {
    arduino_usb_hid_keyboard_event_data_t *data = (arduino_usb_hid_keyboard_event_data_t *)event_data;
    hid_kbd_led_event_t mappedEvent;
    mappedEvent.numlock = data->numlock;
    mappedEvent.capslock = data->capslock;
    mappedEvent.scrolllock = data->scrolllock;
    _userLedHandler(&mappedEvent);
  }
}

class USBBackend : public IHIDBackend {
private:
  USBHIDKeyboard _kb;
public:
  USBBackend() {}
  
  void begin() override {
    USB.begin();
    _kb.begin();
  }
  
  void onEvent(hid_led_event_handler_t handler) override {
    _userLedHandler = handler;
    _kb.onEvent(ARDUINO_USB_HID_KEYBOARD_LED_EVENT, _usbLedCallback);
  }
  
  void press(uint8_t key) override { _kb.press(key); }
  void release(uint8_t key) override { _kb.release(key); }
  void releaseAll() override { _kb.releaseAll(); }
  size_t print(const char* text) override { return _kb.print(text); }
  size_t write(uint8_t c) override { return _kb.write(c); }
  bool isConnected() override { return true; } // Always "connected"
  
  void sendRawReport(uint8_t modifier, const uint8_t* keys) override {
    KeyReport kr;
    kr.modifiers = modifier;
    kr.reserved = 0;
    memcpy(kr.keys, keys, 6);
    _kb.sendReport(&kr);
  }
};

// Factory function
IHIDBackend* createUSBBackend() {
  return new USBBackend();
}
