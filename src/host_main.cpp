/**
 * =============================================================================
 * host_main.cpp — ESP32-S3 #1: USB Host + ESP-NOW Transmitter
 * =============================================================================
 *
 * This board reads a USB Keyboard via USB OTG using the EspUsbHost library,
 * extracts the standard 8-byte HID report, and broadcasts it over ESP-NOW
 * to the Receiver board.
 *
 * Hardware:
 *   - ESP32-S3 DevKitC-1 with USB OTG port connected to a USB Keyboard
 *   - Wi-Fi set to STA mode on a fixed channel for ESP-NOW
 *
 * =============================================================================
 */

#include "shared_protocol.h"
#include <Arduino.h>
#include <EspUsbHost.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// *** TEST MODE — sends simulated keystrokes without a real keyboard ***
// Comment out the next line to disable test mode and use real USB keyboard.
#define TEST_MODE


// ---------------------------------------------------------------------------
// ESP-NOW Peer MAC Address — REPLACE with your Device/RX board's MAC address!
// To find the MAC, flash the device_rx firmware and check its Serial output.
// ---------------------------------------------------------------------------
static uint8_t peerMacAddress[] = {0xFF, 0xFF, 0xFF,
                                   0xFF, 0xFF, 0xFF}; // Broadcast

// --- Global State ---
static kbd_report_t lastReport;
static bool reportChanged = false;

// =====================================================================
// USB Host Keyboard Class — extends EspUsbHost
// =====================================================================
class MyKeyboard : public EspUsbHost {
public:
  /**
   * Callback fired on every keyboard HID report change.
   * We capture the full 8-byte report for ESP-NOW transmission.
   *
   * @param report      Current HID keyboard report
   * @param last_report Previous HID keyboard report
   */
  void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override {
    kbd_report_t newReport;
    newReport.modifier = report.modifier;
    newReport.reserved = report.reserved;
    memcpy(newReport.keycodes, report.keycode, 6);

    // Only send if the report actually changed (avoid flooding)
    if (memcmp(&newReport, &lastReport, sizeof(kbd_report_t)) != 0) {
      memcpy(&lastReport, &newReport, sizeof(kbd_report_t));
      reportChanged = true;
    }
  }
};

static MyKeyboard usbHost;

// =====================================================================
// ESP-NOW Send Callback
// =====================================================================
static void onEspNowSend(const wifi_tx_info_t *info,
                         esp_now_send_status_t status) {
  // Optional: uncomment for send debugging
  // Serial.printf("[ESP-NOW] Send %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK"
  // : "FAIL");
}

// =====================================================================
// Wi-Fi & ESP-NOW Initialization
// =====================================================================
static void initWiFiAndEspNow() {
  // Set Wi-Fi to Station mode (required for ESP-NOW on TX side)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Don't connect to any AP, just use STA mode

  // Lock the Wi-Fi channel to match the Device/RX AP channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  Serial.printf("[WiFi] STA mode, Channel %d\n", ESPNOW_WIFI_CHANNEL);
  Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Init FAILED! Restarting...");
    delay(1000);
    ESP.restart();
  }
  Serial.println("[ESP-NOW] Initialized OK");

  // Register send callback
  esp_now_register_send_cb(onEspNowSend);

  // Add peer (broadcast or specific MAC)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerMacAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ESP-NOW] Add peer FAILED!");
  } else {
    Serial.printf("[ESP-NOW] Peer added: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  peerMacAddress[0], peerMacAddress[1], peerMacAddress[2],
                  peerMacAddress[3], peerMacAddress[4], peerMacAddress[5]);
  }
}

// =====================================================================
// Debug: Print HID Report
// =====================================================================
static void printReport(const kbd_report_t &report) {
  Serial.printf("[HID] Mod:0x%02X Keys:", report.modifier);
  for (int i = 0; i < 6; i++) {
    Serial.printf(" 0x%02X", report.keycodes[i]);
  }
  Serial.println();
}

// =====================================================================
// Setup
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("========================================");
  Serial.println("  ESP32-S3 USB HID Bridge — HOST / TX");
  Serial.println("========================================");

  // Initialize Wi-Fi + ESP-NOW first
  initWiFiAndEspNow();

  // Initialize USB Host
  memset(&lastReport, 0, sizeof(lastReport));
#ifndef TEST_MODE
  usbHost.begin();
  Serial.println("[USB Host] Started — plug in a USB keyboard");
#else
  pinMode(0, INPUT_PULLUP); // BOOT button = GPIO0
  Serial.println("[TEST MODE] Press BOOT button to send 'a'");
#endif
}

// =====================================================================
// Main Loop
// =====================================================================
void loop() {
#ifdef TEST_MODE
  // --- BOOT button (GPIO0) test: press = send 'a', release = key release ---
  static bool lastPressed = false;
  bool pressed = (digitalRead(0) == LOW); // BOOT button is active LOW

  if (pressed && !lastPressed) {
    // Button just pressed → send 'a' key down
    kbd_report_t press = {};
    press.keycodes[0] = 0x04; // HID keycode for 'a'
    Serial.println("[TEST] BOOT pressed → send 'a'");
    esp_now_send(peerMacAddress, (uint8_t *)&press, sizeof(kbd_report_t));
  } else if (!pressed && lastPressed) {
    // Button just released → send key release (all zeros)
    kbd_report_t release = {};
    Serial.println("[TEST] BOOT released → key release");
    esp_now_send(peerMacAddress, (uint8_t *)&release, sizeof(kbd_report_t));
  }
  lastPressed = pressed;
  delay(20); // debounce
#else
  // Let the USB Host library process events
  usbHost.task();

  // If the HID report changed, send it via ESP-NOW
  if (reportChanged) {
    reportChanged = false;

    printReport(lastReport);

    esp_err_t result = esp_now_send(peerMacAddress, (uint8_t *)&lastReport,
                                    sizeof(kbd_report_t));
    if (result != ESP_OK) {
      Serial.printf("[ESP-NOW] Send error: %d\n", result);
    }
  }
#endif
}
