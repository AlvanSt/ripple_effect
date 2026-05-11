#ifndef AUTH_H
#define AUTH_H
int  auth_hash_password(const char *password, char *out_hex65);
int  auth_generate_token(char *out_hex65);
int  auth_register(const char *username, const char *password);
int  auth_login(const char *username, const char *password, char *out_token);
int  auth_validate_token(const char *token, int *out_user_id);
int  auth_logout(const char *token);
#endif
