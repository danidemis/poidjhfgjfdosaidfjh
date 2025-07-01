#include "security.h"
#include <Arduino.h>
#include "mbedtls/sha256.h"
#include <SD_MMC.h>
#include "mbedtls/pkcs5.h" // Aggiungi per PBKDF2
#include "esp_random.h"    // Aggiungi per generare il salt

extern HWCDC USBSerial;

// Impostazioni per PBKDF2
#define PBKDF2_ITERATIONS 10000 // Numero di iterazioni. Più alto è, più è sicuro.

SecurityManager::SecurityManager() : 
    m_currentState(SecurityState::LOCKED), 
    m_isPinSet(false),
    m_isSaltSet(false)
{
    memset(m_storedPinHash, 0, 32);
    memset(m_salt, 0, 16);
    memset(m_userKey, 0, 32);
}

void SecurityManager::begin() {
    USBSerial.println("Inizializzazione SecurityManager (modalita' PIN)...");
    m_preferences.begin("security", false);
    
    m_isPinSet = m_preferences.getBytes("pin_hash", m_storedPinHash, 32) == 32;

    // Carica il salt. Se non esiste, ne crea uno nuovo.
    if (m_preferences.getBytes("pin_salt", m_salt, 16) == 16) {
        m_isSaltSet = true;
    } else if (m_isPinSet) {
        // Se c'è un PIN ma non un salt (vecchia installazione), crea e salva un salt
        USBSerial.println("ATTENZIONE: PIN trovato senza salt. Genero un nuovo salt.");
        esp_fill_random(m_salt, 16);
        m_preferences.putBytes("pin_salt", m_salt, 16);
        m_isSaltSet = true;
    }

    if (m_isPinSet) {
        USBSerial.println("OK: Trovato un PIN di sblocco e un salt salvati.");
    } else {
        USBSerial.println("ATTENZIONE: Nessun PIN trovato. Il primo inserimento verra' salvato.");
    }
    m_preferences.end();
}

bool SecurityManager::checkPin(const String& attempt) {
    if (attempt.length() != 6) return false;

    USBSerial.print("Controllo PIN: "); USBSerial.println(attempt);

    unsigned char attemptHash[32];
    _hashPin(attempt, attemptHash);

    if (m_isPinSet) {
        if (memcmp(attemptHash, m_storedPinHash, 32) == 0) {
            USBSerial.println("OK: PIN corretto. Derivazione chiave in corso...");
            _deriveKey(attempt, m_salt, m_userKey); // Deriva la chiave
            m_currentState = SecurityState::UNLOCKED;
            USBSerial.println("OK: Chiave derivata. Dispositivo sbloccato.");
            return true;
        }
        return false;
    } else {
        USBSerial.println("Nessun PIN master trovato. Questo verra' salvato.");
        
        // Se non c'è un salt, crealo ora
        if (!m_isSaltSet) {
            esp_fill_random(m_salt, 16);
            m_isSaltSet = true;
        }

        m_preferences.begin("security", false);
        m_preferences.putBytes("pin_hash", attemptHash, 32);
        m_preferences.putBytes("pin_salt", m_salt, 16); // Salva anche il salt
        m_preferences.end();
        
        memcpy(m_storedPinHash, attemptHash, 32);
        m_isPinSet = true;
        
        USBSerial.println("OK: Nuovo PIN salvato. Derivazione chiave in corso...");
        _deriveKey(attempt, m_salt, m_userKey); // Deriva la chiave per la prima volta
        m_currentState = SecurityState::UNLOCKED;
        USBSerial.println("OK: Chiave derivata. Dispositivo sbloccato.");
        return true;
    }
}

bool SecurityManager::changePin(const String& oldPin, const String& newPin, CredentialsManager& credManager, Crypto& crypto) {
    USBSerial.println("Inizio procedura di cambio PIN...");

    unsigned char oldPinHash[32];
    _hashPin(oldPin, oldPinHash);
    if (!m_isPinSet || memcmp(oldPinHash, m_storedPinHash, 32) != 0) {
        USBSerial.println("ERRORE: Il vecchio PIN non è corretto.");
        return false;
    }

    // Deriva la vecchia e la nuova chiave
    unsigned char oldKey[32];
    unsigned char newKey[32];
    _deriveKey(oldPin, m_salt, oldKey);
    _deriveKey(newPin, m_salt, newKey);
    USBSerial.println("OK: Vecchia e nuova chiave derivate.");

    size_t count = credManager.getCount();
    if (count > 0) {
        USBSerial.printf("Trovate %d credenziali da ri-cifrare...\n", count);

        File file = SD_MMC.open(CREDENTIALS_FILE, FILE_READ);
        if (!file) {
            USBSerial.println("ERRORE: Impossibile aprire il file delle credenziali per la lettura.");
            return false;
        }
        String tempFilePath = String(CREDENTIALS_FILE) + ".tmp";
        File tempFile = SD_MMC.open(tempFilePath.c_str(), FILE_WRITE);
        if (!tempFile) {
            USBSerial.println("ERRORE: Impossibile creare il file temporaneo.");
            file.close();
            return false;
        }

        // Usa due oggetti Crypto per rendere la logica più pulita
        Crypto crypto_old;
        crypto_old.begin(oldKey);

        Crypto crypto_new;
        crypto_new.begin(newKey);

        for (size_t i = 0; i < count; i++) {
            Credential cred;
            file.read((uint8_t*)&cred, sizeof(Credential));
            
            // Decifra con l'oggetto crypto che usa la VECCHIA chiave
            String plain_pass = crypto_old.decrypt(cred.encrypted_password);
            
            if (plain_pass.length() > 0) {
                // Ri-cifra con l'oggetto crypto che usa la NUOVA chiave
                String new_encrypted_pass = crypto_new.encrypt(plain_pass);
                strncpy(cred.encrypted_password, new_encrypted_pass.c_str(), MAX_ENCRYPTED_PASS_LEN - 1);
            } else {
                 USBSerial.printf("ATTENZIONE: Impossibile decifrare la credenziale #%d con la vecchia chiave. Verrà copiata così com'è.\n", i);
            }
            // Scrivi il record (modificato o no) nel file temporaneo
            tempFile.write((uint8_t*)&cred, sizeof(Credential));
        }
        file.close();
        tempFile.close();
        USBSerial.println("OK: Tutte le credenziali sono state processate nel file temporaneo.");

        // Sostituisci il vecchio file con quello nuovo
        SD_MMC.remove(CREDENTIALS_FILE);
        SD_MMC.rename(tempFilePath.c_str(), CREDENTIALS_FILE);
    }

    // Aggiorna il PIN e la chiave master del dispositivo
    unsigned char newPinHash[32];
    _hashPin(newPin, newPinHash);
    m_preferences.begin("security", false);
    m_preferences.putBytes("pin_hash", newPinHash, 32);
    m_preferences.end();
    memcpy(m_storedPinHash, newPinHash, 32);
    memcpy(m_userKey, newKey, 32); 

    // Infine, aggiorna l'oggetto crypto principale con la nuova chiave per le operazioni future
    crypto.begin(m_userKey);

    USBSerial.println("SUCCESS: PIN cambiato e credenziali ri-cifrate con successo!");
    return true;
}

bool SecurityManager::isPinSet() {
    return m_isPinSet;
}

void SecurityManager::lock() {
    m_currentState = SecurityState::LOCKED;
    USBSerial.println("INFO SecMan: Dispositivo bloccato.");
}

SecurityState SecurityManager::getState() const {
    return m_currentState;
}

void SecurityManager::_hashPin(const String& pin, unsigned char* outHash) {
    mbedtls_sha256((const unsigned char*)pin.c_str(), pin.length(), outHash, 0);
}

const unsigned char* SecurityManager::getUserKey() const {
    return m_userKey;
}

void SecurityManager::_deriveKey(const String& pin, const unsigned char* salt, unsigned char* outKey) {
    //mbedtls_md_context_t sha_ctx;
    //mbedtls_md_init(&sha_ctx);
    //mbedtls_md_setup(&sha_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    
  mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA256,          // Tipo di hash da usare (passato direttamente)
        (const unsigned char*)pin.c_str(), pin.length(), // Il PIN
        salt, 16,                   // Il salt e la sua lunghezza
        PBKDF2_ITERATIONS,          // Numero di iterazioni
        32,                         // Lunghezza della chiave di output in byte (256 bit)
        outKey                      // Buffer di output per la chiave
    );

    //mbedtls_md_free(&sha_ctx);
}