#pragma once
/**
 * =============================================================================
 * hid_keyboard.h — Unified HID Keyboard Abstraction Layer
 * =============================================================================
 * Wraps both USB HID and BLE HID behind a single interface.
 * Uses Pimpl/Opaque pointers to prevent USBHIDKeyboard.h and BleKeyboard.h 
 * from colliding in the same translation unit.
 * =============================================================================
 */

#include <Arduino.h>

// Forward declaration of esp_event_handler_t
typedef void (*esp_event_handler_t)(void *handler_args, const char *base, int32_t id, void *event_data);

// =====================================================================
// Standard HID Key Constants (Shared across both backends)
// =====================================================================
#define HK_KEY_LEFT_CTRL   0x80
#define HK_KEY_LEFT_SHIFT  0x81
#define HK_KEY_LEFT_ALT    0x82
#define HK_KEY_LEFT_GUI    0x83
#define HK_KEY_RIGHT_CTRL  0x84
#define HK_KEY_RIGHT_SHIFT 0x85
#define HK_KEY_RIGHT_ALT   0x86
#define HK_KEY_RIGHT_GUI   0x87

#define HK_KEY_UP_ARROW    0xDA
#define HK_KEY_DOWN_ARROW  0xD9
#define HK_KEY_LEFT_ARROW  0xD8
#define HK_KEY_RIGHT_ARROW 0xD7
#define HK_KEY_BACKSPACE   0xB2
#define HK_KEY_TAB         0xB3
#define HK_KEY_RETURN      0xB0
#define HK_KEY_ESC         0xB1
#define HK_KEY_INSERT      0xD1
#define HK_KEY_DELETE      0xD4
#define HK_KEY_PAGE_UP     0xD3
#define HK_KEY_PAGE_DOWN   0xD6
#define HK_KEY_HOME        0xD2
#define HK_KEY_END         0xD5
#define HK_KEY_CAPS_LOCK   0xC1
#define HK_KEY_F1          0xC2
#define HK_KEY_F2          0xC3
#define HK_KEY_F3          0xC4
#define HK_KEY_F4          0xC5
#define HK_KEY_F5          0xC6
#define HK_KEY_F6          0xC7
#define HK_KEY_F7          0xC8
#define HK_KEY_F8          0xC9
#define HK_KEY_F9          0xCA
#define HK_KEY_F10         0xCB
#define HK_KEY_F11         0xCC
#define HK_KEY_F12         0xCD
#define HK_KEY_PRINT_SCREEN 0xCE

// =====================================================================
// Custom Event Data Struct (mirrors arduino_usb_hid_keyboard_event_data_t)
// =====================================================================
struct hid_kbd_led_event_t {
  bool numlock;
  bool capslock;
  bool scrolllock;
};

// Forward declaration of custom event handler
typedef void (*hid_led_event_handler_t)(hid_kbd_led_event_t*);

// =====================================================================
// HID Mode Enum
// =====================================================================
enum HIDMode {
  HID_MODE_USB = 0,
  HID_MODE_BLE = 1
};

// =====================================================================
// Opaque Implementation Interfaces
// =====================================================================
class IHIDBackend {
public:
  virtual ~IHIDBackend() = default;
  virtual void begin() = 0;
  virtual void onEvent(hid_led_event_handler_t handler) = 0;
  virtual void press(uint8_t key) = 0;
  virtual void release(uint8_t key) = 0;
  virtual void releaseAll() = 0;
  virtual size_t print(const char* text) = 0;
  virtual size_t write(uint8_t c) = 0;
  virtual bool isConnected() = 0;
  virtual void sendRawReport(uint8_t modifier, const uint8_t* keys) = 0;
};

// =====================================================================
// Unified HID Keyboard Class
// =====================================================================
class HIDKeyboard {
public:
  HIDKeyboard();
  ~HIDKeyboard();

  void begin(HIDMode mode = HID_MODE_USB);
  void onEvent(hid_led_event_handler_t handler);
  void press(uint8_t key);
  void release(uint8_t key);
  void releaseAll();
  size_t print(const char* text);
  size_t print(const String& text);
  size_t write(uint8_t c);
  bool isConnected();
  void sendRawReport(uint8_t modifier, const uint8_t* keys);

  HIDMode getMode() { return _mode; }
  String getModeName();

private:
  HIDMode _mode;
  IHIDBackend* _backend;
};
