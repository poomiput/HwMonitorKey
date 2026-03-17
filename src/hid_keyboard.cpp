#include "hid_keyboard.h"

// Forward declare factory functions from the separated compilation units
extern IHIDBackend* createUSBBackend();
extern IHIDBackend* createBLEBackend();

HIDKeyboard::HIDKeyboard() : _mode(HID_MODE_USB), _backend(nullptr) {
}

HIDKeyboard::~HIDKeyboard() {
  if (_backend) {
    delete _backend;
  }
}

void HIDKeyboard::begin(HIDMode mode) {
  _mode = mode;
  if (_backend) {
    delete _backend;
  }

  if (mode == HID_MODE_USB) {
    _backend = createUSBBackend();
  } else {
    _backend = createBLEBackend();
  }

  if (_backend) {
    _backend->begin();
  }
}

void HIDKeyboard::onEvent(hid_led_event_handler_t handler) {
  if (_backend) _backend->onEvent(handler);
}

void HIDKeyboard::press(uint8_t key) {
  if (_backend) _backend->press(key);
}

void HIDKeyboard::release(uint8_t key) {
  if (_backend) _backend->release(key);
}

void HIDKeyboard::releaseAll() {
  if (_backend) _backend->releaseAll();
}

size_t HIDKeyboard::print(const char* text) {
  if (_backend) return _backend->print(text);
  return 0;
}

size_t HIDKeyboard::print(const String& text) {
  if (_backend) return _backend->print(text.c_str());
  return 0;
}

size_t HIDKeyboard::write(uint8_t c) {
  if (_backend) return _backend->write(c);
  return 0;
}

bool HIDKeyboard::isConnected() {
  if (_backend) return _backend->isConnected();
  return false;
}

void HIDKeyboard::sendRawReport(uint8_t modifier, const uint8_t* keys) {
  if (_backend) _backend->sendRawReport(modifier, keys);
}

String HIDKeyboard::getModeName() {
  return (_mode == HID_MODE_USB) ? "USB HID" : "BLE HID";
}
