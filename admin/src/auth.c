#include "auth.h"
#include "database.h"
#include <openssl/sha.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

int auth_hash_password(const char *password, char *out_hex65) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)password, strlen(password), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(out_hex65 + i * 2, "%02x", hash[i]);
    out_hex65[64] = '\0';
    return 0;
}

int auth_generate_token(char *out_hex65) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return -1;
    unsigned char buf[TOKEN_LEN];
    read(fd, buf, TOKEN_LEN);
    close(fd);
    for (int i = 0; i < TOKEN_LEN; i++)
        sprintf(out_hex65 + i * 2, "%02x", buf[i]);
    out_hex65[TOKEN_LEN * 2] = '\0';
    return 0;
}

int auth_register(const char *username, const char *password) {
    char hash[65];
    auth_hash_password(password, hash);
    return db_create_user(username, hash);
}

int auth_login(const char *username, const char *password, char *out_token) {
    User u;
    if (db_get_user(username, &u) != 0) return -1;
    char hash[65];
    auth_hash_password(password, hash);
    if (strcmp(hash, u.password_hash) != 0) return -1;
    auth_generate_token(out_token);
    long expires = (long)time(NULL) + 86400; /* 24h */
    db_save_session(out_token, u.id, expires);
    return 0;
}

int auth_validate_token(const char *token, int *out_user_id) {
    Session s;
    if (db_get_session(token, &s) != 0) return -1;
    if ((long)time(NULL) > s.expires_at) { db_delete_session(token); return -1; }
    *out_user_id = s.user_id;
    return 0;
}

int auth_logout(const char *token) {
    return db_delete_session(token);
}
