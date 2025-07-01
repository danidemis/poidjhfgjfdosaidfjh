#include "crypto.h"
// L'include di gcm.h non è più necessario qui, perché è già in crypto.h
#include "mbedtls/base64.h"
#include "esp_random.h"

extern HWCDC USBSerial; // Aggiungiamo il riferimento a USBSerial

#define IV_SIZE 12
#define TAG_SIZE 16



// Il costruttore ora inizializza l'oggetto direttamente, senza 'new'
Crypto::Crypto() : is_initialized(false) {
    mbedtls_gcm_init(&aes_ctx);
}

// Il distruttore libera l'oggetto direttamente, senza 'delete'
Crypto::~Crypto() {
    mbedtls_gcm_free(&aes_ctx);
}

// Tutte le funzioni ora passano l'indirizzo del nostro oggetto aes_ctx
void Crypto::begin(const unsigned char* key) {
    int ret = mbedtls_gcm_setkey(&aes_ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
    is_initialized = (ret == 0);
}

String Crypto::encrypt(const String& plaintext) {
    if (!is_initialized || plaintext.length() == 0) return "";
    
    unsigned char iv[IV_SIZE];
    esp_fill_random(iv, IV_SIZE);
    
    size_t plain_len = plaintext.length();
    unsigned char* ciphertext = new unsigned char[plain_len];
    unsigned char tag[TAG_SIZE];

    if (mbedtls_gcm_crypt_and_tag(&aes_ctx, MBEDTLS_GCM_ENCRYPT, plain_len, iv, IV_SIZE, NULL, 0, (unsigned char*)plaintext.c_str(), ciphertext, TAG_SIZE, tag) != 0) {
        delete[] ciphertext;
        return "";
    }
    
    // Il resto della funzione rimane identico...
    size_t combined_len = IV_SIZE + TAG_SIZE + plain_len;
    unsigned char* combined_buf = new unsigned char[combined_len];
    memcpy(combined_buf, iv, IV_SIZE);
    memcpy(combined_buf + IV_SIZE, tag, TAG_SIZE);
    memcpy(combined_buf + IV_SIZE + TAG_SIZE, ciphertext, plain_len);
    
    size_t b64_len;
    mbedtls_base64_encode(NULL, 0, &b64_len, combined_buf, combined_len);
    unsigned char* b64_buf = new unsigned char[b64_len];
    mbedtls_base64_encode(b64_buf, b64_len, &b64_len, combined_buf, combined_len);
    
    String result((char*)b64_buf, b64_len);
    
    delete[] ciphertext;
    delete[] combined_buf;
    delete[] b64_buf;
    
    return result;
}


String Crypto::decrypt(const String& base64_ciphertext) {
    if (!is_initialized || base64_ciphertext.length() == 0) return "";

    // 1. Decodifica da Base64
    size_t combined_len;
    unsigned char* decoded_buf = new unsigned char[base64_ciphertext.length()];
    int ret = mbedtls_base64_decode(decoded_buf, base64_ciphertext.length(), &combined_len, (unsigned char*)base64_ciphertext.c_str(), base64_ciphertext.length());
    if (ret != 0) {
        delete[] decoded_buf;
        return "";
    }

    // 2. Controlla che il buffer sia abbastanza grande per IV e Tag
    if (combined_len <= (IV_SIZE + TAG_SIZE)) {
        delete[] decoded_buf;
        return "";
    }

    // 3. Estrai IV, Tag e testo cifrato
    unsigned char iv[IV_SIZE], tag[TAG_SIZE];
    size_t cipher_len = combined_len - IV_SIZE - TAG_SIZE;
    unsigned char* ciphertext = new unsigned char[cipher_len];
    
    memcpy(iv, decoded_buf, IV_SIZE);
    memcpy(tag, decoded_buf + IV_SIZE, TAG_SIZE);
    memcpy(ciphertext, decoded_buf + IV_SIZE + TAG_SIZE, cipher_len);
    delete[] decoded_buf; // Non più necessario

    // 4. Decifra e autentica in un unico passaggio con la funzione corretta
    unsigned char* decrypted_buf = new unsigned char[cipher_len + 1];

    ret = mbedtls_gcm_auth_decrypt(
        &aes_ctx,                   // Contesto GCM
        cipher_len,                 // Lunghezza del testo cifrato
        iv, IV_SIZE,                // IV e sua lunghezza
        NULL, 0,                    // Dati aggiuntivi (non usati)
        tag, TAG_SIZE,              // Tag di autenticazione e sua lunghezza
        ciphertext,                 // Input (testo cifrato)
        decrypted_buf               // Output (buffer per il testo in chiaro)
    );

    delete[] ciphertext; // Non più necessario

    // 5. Controlla il risultato
    if (ret != 0) { // Se ret non è 0, l'autenticazione è fallita
        delete[] decrypted_buf;
        return ""; // Restituisce stringa vuota in caso di errore
    }

    // 6. Restituisci il risultato
    decrypted_buf[cipher_len] = '\0';
    String result = String((char*)decrypted_buf);
    delete[] decrypted_buf;
    
    return result;
}