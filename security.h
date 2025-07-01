#pragma once

#include <vector>
#include <Preferences.h>
#include "HWCDC.h"

// Includi questi due file per avere accesso alle altre classi
#include "credentials.h"
#include "crypto.h"

enum class SecurityState {
    LOCKED,
    UNLOCKED
};

class SecurityManager {
public:
    SecurityManager();
    void begin();

    // Controlla un PIN. Se nessun PIN è salvato, lo salva come master.
    bool checkPin(const String& attempt);

    // Controlla se un PIN è già stato impostato
    bool isPinSet();

    // Processo di cambio PIN che gestisce la ri-cifratura
    bool changePin(const String& oldPin, const String& newPin, CredentialsManager& credManager, Crypto& crypto);

    void lock(); 

    SecurityState getState() const;
    const unsigned char* getUserKey() const; // Getter per la chiave derivata

private:
    void _hashPin(const String& pin, unsigned char* outHash);
    
    void _deriveKey(const String& pin, const unsigned char* salt, unsigned char* outKey);

    SecurityState m_currentState;
    Preferences m_preferences;
    unsigned char m_storedPinHash[32];
    unsigned char m_salt[16]; // Memorizziamo il salt per PBKDF2
    unsigned char m_userKey[32]; // La chiave di cifratura derivata
    bool m_isPinSet;
    bool m_isSaltSet;
};