#include "web.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "settings.h"

static void base64(const unsigned char *in, size_t len, char *out)
{
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        unsigned v = (unsigned)in[i] << 16 | (i + 1 < len ? (unsigned)in[i + 1] << 8 : 0) |
                     (i + 2 < len ? in[i + 2] : 0);
        out[o++] = tbl[(v >> 18) & 63];
        out[o++] = tbl[(v >> 12) & 63];
        out[o++] = i + 1 < len ? tbl[(v >> 6) & 63] : '=';
        out[o++] = i + 2 < len ? tbl[v & 63] : '=';
    }
    out[o] = '\0';
}

bool web_auth_ok(const char *authorization, const char *token)
{
    if (!authorization || !token || !token[0] || strncmp(authorization, "Basic ", 6) != 0) {
        return false;
    }
    // Compare encoded forms: encoding is simpler (and safer) than decoding input.
    char plain[64], expected[96];
    int n = snprintf(plain, sizeof(plain), "%s:%s", WEB_AUTH_USER, token);
    if (n <= 0 || (size_t)n >= sizeof(plain)) {
        return false;
    }
    base64((const unsigned char *)plain, (size_t)n, expected);
    const char *given = authorization + 6;
    while (*given == ' ') {
        given++;
    }
    return strcmp(given, expected) == 0;
}

char *web_settings_body(const char *version, bool ota)
{
    char *settings = settings_to_json();
    cJSON *root = cJSON_Parse(settings);
    free(settings);
    if (!root) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "version", version ? version : "");
    cJSON_AddBoolToObject(root, "ota", ota);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

char *web_apply_settings(const char *body, bool *ok)
{
    char err[160] = "";
    *ok = settings_apply_json(body ? body : "", err, sizeof(err));
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", *ok);
    if (!*ok) {
        cJSON_AddStringToObject(root, "error", err);   // escaped by cJSON
    }
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}
