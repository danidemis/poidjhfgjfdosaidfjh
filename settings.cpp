#include "settings.h"
#include <Arduino.h>

SettingsManager settingsManager;

SettingsManager::SettingsManager() : 
    m_currentLayout(KeyboardLayout::ITALIANO),     // Default layout
    m_currentTargetOS(TargetOS::WINDOWS_LINUX),    // Default OS
    m_isHidEnabled(false)
{}

// begin(): carichiamo TUTTE le impostazioni dalla memoria
void SettingsManager::begin() {
    preferences.begin("settings", false);
    
    m_currentLayout = (KeyboardLayout)preferences.getUChar("layout", (uint8_t)KeyboardLayout::ITALIANO);
    m_currentTargetOS = (TargetOS)preferences.getUChar("target_os", (uint8_t)TargetOS::WINDOWS_LINUX);
    m_isHidEnabled = preferences.getBool("hid_mode", false); 
    
    // Carichiamo le nuove impostazioni di sicurezza, con i loro default
    m_max_pin_attempts = preferences.getUChar("max_attempts", 5);
    m_current_failed_attempts = preferences.getUChar("failed_attempts", 0);

    preferences.end();

    Serial.println("SettingsManager inizializzato.");
    Serial.printf(" - Layout corrente: %d\n", (int)m_currentLayout);
    Serial.printf(" - OS Target corrente: %d\n", (int)m_currentTargetOS);
    Serial.printf(" - Modalita' HID: %s\n", m_isHidEnabled ? "ATTIVA" : "DISATTIVA");
    Serial.printf(" - Tentativi PIN massimi: %d\n", m_max_pin_attempts);
    Serial.printf(" - Tentativi falliti correnti: %d\n", m_current_failed_attempts);
}


// --- Layout Tastiera ---
void SettingsManager::setKeyboardLayout(KeyboardLayout layout) {
    m_currentLayout = layout;
    preferences.begin("settings", false);
    preferences.putUChar("layout", (uint8_t)layout);
    preferences.end();
    Serial.printf("INFO Settings: Layout salvato: %d\n", (int)layout);
}

KeyboardLayout SettingsManager::getKeyboardLayout() const {
    return m_currentLayout;
}

// --- Sistema Operativo Target ---
void SettingsManager::setTargetOS(TargetOS os) {
    m_currentTargetOS = os;
    preferences.begin("settings", false);
    preferences.putUChar("target_os", (uint8_t)os);
    preferences.end();
    Serial.printf("INFO Settings: OS Target salvato: %d\n", (int)os);
}

TargetOS SettingsManager::getTargetOS() const {
    return m_currentTargetOS;
}

// --- Modalità HID ---
void SettingsManager::setHidMode(bool enabled) {
    m_isHidEnabled = enabled;
    preferences.begin("settings", false);
    preferences.putBool("hid_mode", enabled);
    preferences.end();
    Serial.printf("INFO Settings: Modalita' HID salvata: %s\n", enabled ? "ATTIVA" : "DISATTIVA");
}

bool SettingsManager::isHidModeEnabled() const {
    return m_isHidEnabled;
}


void SettingsManager::setMaxPinAttempts(uint8_t count) {
    if (count >= 3 && count <= 10) {
        m_max_pin_attempts = count;
        preferences.begin("settings", false);
        preferences.putUChar("max_attempts", m_max_pin_attempts);
        preferences.end();
        Serial.printf("INFO Settings: Numero massimo tentativi impostato a %d\n", m_max_pin_attempts);
    }
}

uint8_t SettingsManager::getMaxPinAttempts() const {
    return m_max_pin_attempts;
}

void SettingsManager::incrementFailedAttempts() {
    m_current_failed_attempts++;
    preferences.begin("settings", false);
    preferences.putUChar("failed_attempts", m_current_failed_attempts);
    preferences.end();
    Serial.printf("INFO Settings: Tentativi falliti incrementati a %d\n", m_current_failed_attempts);
}

void SettingsManager::resetFailedAttempts() {
    if (m_current_failed_attempts > 0) {
        m_current_failed_attempts = 0;
        preferences.begin("settings", false);
        preferences.putUChar("failed_attempts", 0);
        preferences.end();
        Serial.println("INFO Settings: Contatore tentativi falliti azzerato.");
    }
}

uint8_t SettingsManager::getFailedAttempts() const {
    return m_current_failed_attempts;
}