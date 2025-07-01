#pragma once
#include "crypto.h"

// Costanti per il file di credenziali
#define CREDENTIALS_FILE "/credentials.bin"
#define MAX_TITLE_LEN 64
#define MAX_USERNAME_LEN 64
#define MAX_ENCRYPTED_PASS_LEN 256
#define CREDENTIAL_RECORD_SIZE (MAX_TITLE_LEN + MAX_USERNAME_LEN + MAX_ENCRYPTED_PASS_LEN)

struct Credential {
    char title[MAX_TITLE_LEN];
    char username[MAX_USERNAME_LEN];
    char encrypted_password[MAX_ENCRYPTED_PASS_LEN];
};

class CredentialsManager {
public:
    CredentialsManager();
    void begin();
    bool importFromSD(const char* filepath, Crypto& crypto);
    size_t getCount() const;
    bool getCredential(size_t index, Credential* cred) const;
    void clear();

private:
    size_t m_credential_count;
};