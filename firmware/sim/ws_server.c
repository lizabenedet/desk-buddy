#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_link.h"
#include "web.h"
#include "ws_server.h"

#define MAX_CLIENTS 4
#define BUF_SIZE    8192
#define FRAME_MAX   2048
#define WS_GUID     "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef struct {
    SOCKET sock;
    bool open;
    bool upgraded;
    unsigned char buf[BUF_SIZE];
    size_t len;
} client_t;

static SOCKET listener = INVALID_SOCKET;
static client_t clients[MAX_CLIENTS];
static pc_session_t session;

/* ---------- SHA-1 + base64 for the handshake ---------- */

static uint32_t rol(uint32_t v, int n)
{
    return (v << n) | (v >> (32 - n));
}

static void sha1(const unsigned char *data, size_t len, unsigned char out[20])
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    size_t padded = ((len + 8) / 64 + 1) * 64;
    unsigned char msg[256] = {0};   // handshake input is ~60 bytes
    if (padded > sizeof(msg)) {
        return;
    }
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) {
        msg[padded - 1 - i] = (unsigned char)(bits >> (8 * i));
    }
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            const unsigned char *p = msg + off + 4 * i;
            w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    for (int i = 0; i < 5; i++) {
        out[4 * i] = (unsigned char)(h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(h[i] >> 8);
        out[4 * i + 3] = (unsigned char)h[i];
    }
}

static void base64(const unsigned char *in, size_t len, char *out)
{
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < len ? (uint32_t)in[i + 1] << 8 : 0) |
                     (i + 2 < len ? in[i + 2] : 0);
        out[o++] = tbl[(v >> 18) & 63];
        out[o++] = tbl[(v >> 12) & 63];
        out[o++] = i + 1 < len ? tbl[(v >> 6) & 63] : '=';
        out[o++] = i + 2 < len ? tbl[v & 63] : '=';
    }
    out[o] = '\0';
}

/* ---------- connections ---------- */

static void send_all(client_t *c, const void *data, size_t len)
{
    const char *p = data;
    while (len) {
        int n = send(c->sock, p, (int)len, 0);
        if (n <= 0) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                Sleep(1);
                continue;
            }
            return;
        }
        p += n;
        len -= (size_t)n;
    }
}

static void send_frame(client_t *c, int opcode, const void *payload, size_t len)
{
    unsigned char hdr[4] = {(unsigned char)(0x80 | opcode)};
    size_t hlen = 2;
    if (len < 126) {
        hdr[1] = (unsigned char)len;
    } else {
        hdr[1] = 126;
        hdr[2] = (unsigned char)(len >> 8);
        hdr[3] = (unsigned char)len;
        hlen = 4;
    }
    send_all(c, hdr, hlen);
    send_all(c, payload, len);
}

static void close_client(int i)
{
    if (!clients[i].open) {
        return;
    }
    closesocket(clients[i].sock);
    clients[i].open = false;
    pc_session_closed(&session, i);
}

static void consume(client_t *c, size_t n)
{
    memmove(c->buf, c->buf + n, c->len - n);
    c->len -= n;
}

static void send_http(client_t *c, const char *status, const char *type, const char *body, const char *extra)
{
    char head[512];
    size_t len = body ? strlen(body) : 0;
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n%sConnection: close\r\n\r\n",
                     status, type, (unsigned)len, extra ? extra : "");
    send_all(c, head, (size_t)n);
    if (len) {
        send_all(c, body, len);
    }
}

// Value of header `name` (lowercase, with ':') from the original request.
static void header_value(const char *raw, const char *lower, const char *name, char *out, size_t len)
{
    out[0] = '\0';
    const char *at = strstr(lower, name);
    if (!at) {
        return;
    }
    const char *p = raw + (at - lower) + strlen(name);
    while (*p == ' ') {
        p++;
    }
    size_t k = 0;
    while (*p && *p != '\r' && k + 1 < len) {
        out[k++] = *p++;
    }
    out[k] = '\0';
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    if (buf && fread(buf, 1, (size_t)len, f) == (size_t)len) {
        buf[len] = '\0';
    } else {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

// The buddy.local page, same routes as the board minus OTA. Always closes.
static void handle_http(client_t *c, const char *lower, size_t header_len, const char *body)
{
    char auth[256];
    header_value((const char *)c->buf, lower, "\nauthorization:", auth, sizeof(auth));
    if (!web_auth_ok(auth, session.token)) {
        send_http(c, "401 Unauthorized", "text/plain", "token errado",
                  "WWW-Authenticate: Basic realm=\"" WEB_AUTH_REALM "\", charset=\"UTF-8\"\r\n");
        return;
    }
    (void)header_len;
    if (strncmp(lower, "get / ", 6) == 0) {
        char *page = read_file(SIM_WEB_DIR "/index.html");
        if (page) {
            send_http(c, "200 OK", "text/html; charset=utf-8", page, NULL);
            free(page);
        } else {
            send_http(c, "500 Internal Server Error", "text/plain", "index.html not found", NULL);
        }
    } else if (strncmp(lower, "get /api/settings ", 18) == 0) {
        char *json = web_settings_body("simulador", false);
        send_http(c, "200 OK", "application/json", json, NULL);
        free(json);
    } else if (strncmp(lower, "post /api/settings ", 19) == 0) {
        bool ok;
        char *reply = web_apply_settings(body, &ok);
        send_http(c, ok ? "200 OK" : "400 Bad Request", "application/json", reply, NULL);
        free(reply);
    } else if (strncmp(lower, "post /api/ota ", 14) == 0) {
        send_http(c, "501 Not Implemented", "application/json",
                  "{\"ok\":false,\"error\":\"o simulador não atualiza firmware\"}", NULL);
    } else {
        send_http(c, "404 Not Found", "text/plain", "not found", NULL);
    }
}

// Returns false if the connection must be closed.
static bool handle_handshake(client_t *c)
{
    c->buf[c->len < BUF_SIZE ? c->len : BUF_SIZE - 1] = '\0';
    char *end = strstr((char *)c->buf, "\r\n\r\n");
    if (!end) {
        return c->len < BUF_SIZE - 1;   // wait for more, unless it's absurdly long
    }
    size_t header_len = (size_t)(end - (char *)c->buf) + 4;

    char lower[BUF_SIZE];
    for (size_t i = 0; i < header_len; i++) {
        lower[i] = (char)tolower(c->buf[i]);
    }
    lower[header_len] = '\0';
    const char *key_at = strstr(lower, "sec-websocket-key:");
    if (strncmp(lower, "get /ws ", 8) != 0 || !key_at) {
        // Plain HTTP: wait for the whole body, answer, close.
        char cl[16];
        header_value((const char *)c->buf, lower, "\ncontent-length:", cl, sizeof(cl));
        size_t body_len = (size_t)atoi(cl);
        if (header_len + body_len >= BUF_SIZE) {
            send_http(c, "413 Payload Too Large", "text/plain", "too large", NULL);
            return false;
        }
        if (c->len < header_len + body_len) {
            return true;
        }
        char body[BUF_SIZE];
        memcpy(body, c->buf + header_len, body_len);
        body[body_len] = '\0';
        handle_http(c, lower, header_len, body);
        return false;
    }
    // Read the key from the original (case matters).
    const char *p = (const char *)c->buf + (key_at - lower) + strlen("sec-websocket-key:");
    while (*p == ' ') {
        p++;
    }
    char key[128];
    size_t k = 0;
    while (*p && *p != '\r' && k < 60) {
        key[k++] = *p++;
    }
    key[k] = '\0';
    strcat(key, WS_GUID);

    unsigned char digest[20];
    char accept[32];
    sha1((const unsigned char *)key, strlen(key), digest);
    base64(digest, sizeof(digest), accept);

    char resp[256];
    int n = snprintf(resp, sizeof(resp),
                     "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n",
                     accept);
    send_all(c, resp, (size_t)n);
    c->upgraded = true;
    consume(c, header_len);
    return true;
}

// Processes complete frames in the buffer. Returns false to close.
static bool handle_frames(int i)
{
    client_t *c = &clients[i];
    while (c->len >= 2) {
        unsigned char b0 = c->buf[0], b1 = c->buf[1];
        int opcode = b0 & 0x0F;
        size_t len = b1 & 0x7F, hdr = 2;
        if (!(b0 & 0x80) || !(b1 & 0x80)) {
            return false;   // fragmented or unmasked client frame: unsupported
        }
        if (len == 126) {
            if (c->len < 4) return true;
            len = (size_t)c->buf[2] << 8 | c->buf[3];
            hdr = 4;
        } else if (len == 127) {
            return false;   // way beyond FRAME_MAX
        }
        if (len >= FRAME_MAX) {
            return false;
        }
        if (c->len < hdr + 4 + len) {
            return true;    // wait for the rest
        }
        unsigned char *mask = c->buf + hdr;
        char payload[FRAME_MAX];
        for (size_t j = 0; j < len; j++) {
            payload[j] = (char)(c->buf[hdr + 4 + j] ^ mask[j % 4]);
        }
        payload[len] = '\0';
        consume(c, hdr + 4 + len);

        if (opcode == 0x1) {
            char reply[160];
            pc_session_action_t a = pc_session_frame(&session, i, payload, GetTickCount(), reply, sizeof(reply));
            if (reply[0]) {
                send_frame(c, 0x1, reply, strlen(reply));
            }
            if (a.close_other >= 0) {
                close_client(a.close_other);
            }
            if (a.close_this) {
                return false;
            }
        } else if (opcode == 0x8) {
            send_frame(c, 0x8, payload, len);
            return false;
        } else if (opcode == 0x9) {
            send_frame(c, 0xA, payload, len);
        }
    }
    return true;
}

void ws_server_start(uint16_t port)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return;
    }
    char token[PC_LINK_TOKEN_LEN];
    pc_link_token(token);
    pc_session_init(&session, token);

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(port)};
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    u_long nonblocking = 1;
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(listener, 4) != 0) {
        fprintf(stderr, "ws_server: can't listen on 127.0.0.1:%u\n", port);
        closesocket(listener);
        listener = INVALID_SOCKET;
        return;
    }
    ioctlsocket(listener, FIONBIO, &nonblocking);
    printf("ws_server: ws://127.0.0.1:%u/ws  token %s\n", port, token);
    fflush(stdout);
}

void ws_server_poll(void)
{
    if (listener == INVALID_SOCKET) {
        return;
    }
    SOCKET s;
    while ((s = accept(listener, NULL, NULL)) != INVALID_SOCKET) {
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].open) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            closesocket(s);
            continue;
        }
        u_long nonblocking = 1;
        ioctlsocket(s, FIONBIO, &nonblocking);
        clients[slot] = (client_t){.sock = s, .open = true};
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t *c = &clients[i];
        if (!c->open) {
            continue;
        }
        int n = recv(c->sock, (char *)c->buf + c->len, (int)(BUF_SIZE - 1 - c->len), 0);
        if (n == 0 || (n < 0 && WSAGetLastError() != WSAEWOULDBLOCK)) {
            close_client(i);
            continue;
        }
        if (n > 0) {
            c->len += (size_t)n;
            bool ok = c->upgraded ? handle_frames(i) : handle_handshake(c) && handle_frames(i);
            if (!ok) {
                close_client(i);
            }
        }
    }

    int expired = pc_session_expired(&session, GetTickCount());
    if (expired >= 0) {
        close_client(expired);
    }
}
