#pragma once

#include <Preferences.h> // Includiamo la libreria standard dell'ESP32


// Definiamo i possibili layout di tastiera che il nostro dispositivo supporterà
enum class KeyboardLayout {
    ITALIANO,
    TEDESCO,
    FRANCESE,
    SPAGNOLO,
    USA
};

// Definiamo i possibili sistemi operativi di destinazione
enum class TargetOS : uint8_t {
    WINDOWS_LINUX,
    MACOS
};

// Dichiarazione della nostra classe per gestire le impostazioni
class SettingsManager {
public:
    SettingsManager();
    void begin();

    // Funzioni per gestire il layout della tastiera
    void setKeyboardLayout(KeyboardLayout layout);
    KeyboardLayout getKeyboardLayout() const;

    // Funzioni per gestire il sistema operativo
    void setTargetOS(TargetOS os);
    TargetOS getTargetOS() const;

    // Funzioni per gestire la modalità USB (già esistenti)
    void setHidMode(bool enabled);
    bool isHidModeEnabled() const;

    // --- NUOVE FUNZIONI PER LA SICUREZZA ---
    void setMaxPinAttempts(uint8_t count);
    uint8_t getMaxPinAttempts() const;
    void incrementFailedAttempts();
    void resetFailedAttempts();
    uint8_t getFailedAttempts() const;
    // ------------------------------------

private:
    Preferences preferences;
    KeyboardLayout m_currentLayout;
    TargetOS m_currentTargetOS;
    bool m_isHidEnabled;
    // --- NUOVE VARIABILI MEMBRO ---
    uint8_t m_max_pin_attempts;
    uint8_t m_current_failed_attempts;
    // ----------------------------
};

extern SettingsManager settingsManager;