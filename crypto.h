#pragma once
#include <Arduino.h>

// Includiamo direttamente l'header corretto invece di una dichiarazione anticipata.
// Questo risolve il conflitto principale.
#include "mbedtls/gcm.h"

class Crypto {
public:
    Crypto();
    ~Crypto();

    void begin(const unsigned char* key);
    String encrypt(const String& plaintext);
    String decrypt(const String& base64_ciphertext);

private:
    // Non usiamo più un puntatore, ma l'oggetto contesto direttamente.
    // La libreria stessa gestirà l'allocazione della memoria.
    mbedtls_gcm_context aes_ctx; 
    bool is_initialized;
};