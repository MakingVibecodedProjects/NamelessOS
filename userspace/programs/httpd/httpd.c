#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PORT        80
#define BACKLOG     4
#define RBUF_SIZE   512
#define SBUF_SIZE   1024

/* ── Static response body ─────────────────────────────────────────── */
static const char INDEX_BODY[] =
    "<!DOCTYPE html><html><head><title>NamelessOS</title></head>"
    "<body><h1>NamelessOS</h1>"
    "<p>Kernel-served HTTP/1.0 from bare metal.</p>"
    "</body></html>\r\n";

/* ── handle_client — read one request, send one response ─────────── */
static void handle_client(int cfd) {
    char rbuf[RBUF_SIZE];
    int  n = 0;

    /* Read until we see the end of the HTTP request header (\r\n\r\n
       or \n\n), or until the buffer fills up.  We only serve one
       request per connection (HTTP/1.0 semantics). */
    while (n < (int)sizeof(rbuf) - 1) {
        int got = recv(cfd, rbuf + n, (uint32_t)(sizeof(rbuf) - 1 - (uint32_t)n), 0);
        if (got <= 0) goto done;
        n += got;
        rbuf[n] = '\0';
        /* Look for end-of-headers: \r\n\r\n or \n\n */
        int found = 0;
        for (int j = 0; j + 3 < n; j++) {
            if (rbuf[j]=='\r' && rbuf[j+1]=='\n' &&
                rbuf[j+2]=='\r' && rbuf[j+3]=='\n') { found = 1; break; }
            if (rbuf[j]=='\n' && rbuf[j+1]=='\n') { found = 1; break; }
        }
        if (found) break;
    }

    /* Only handle GET / — all others get 200 with the same body */
    char sbuf[SBUF_SIZE];
    int  body_len = (int)strlen(INDEX_BODY);
    int  hdr_len  = snprintf(sbuf, sizeof(sbuf),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        body_len);

    /* Send headers */
    int sent = 0;
    while (sent < hdr_len) {
        int w = send(cfd, sbuf + sent, (uint32_t)(hdr_len - sent), 0);
        if (w <= 0) goto done;
        sent += w;
    }

    /* Send body */
    sent = 0;
    while (sent < body_len) {
        int w = send(cfd, INDEX_BODY + sent, (uint32_t)(body_len - sent), 0);
        if (w <= 0) goto done;
        sent += w;
    }

done:
    shutdown(cfd, 0);
    close(cfd);
}

/* ── main ──────────────────────────────────────────────────────────── */
int main(void) {
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        printf("httpd: socket failed\n");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)PORT);
    addr.sin_addr   = 0;   /* INADDR_ANY */

    if (bind(lfd, &addr, (uint32_t)sizeof(addr)) < 0) {
        printf("httpd: bind failed\n");
        return 1;
    }

    if (listen(lfd, BACKLOG) < 0) {
        printf("httpd: listen failed\n");
        return 1;
    }

    printf("httpd: listening on port %d\n", PORT);

    /* Accept loop — one connection at a time (polled accept) */
    for (;;) {
        struct sockaddr_in peer;
        uint32_t plen = (uint32_t)sizeof(peer);
        int cfd = accept(lfd, &peer, &plen);
        if (cfd < 0) {
            /* EAGAIN — no connection ready yet; yield by doing a tiny read */
            char dummy;
            read(STDIN_FILENO, &dummy, 0);
            continue;
        }
        handle_client(cfd);
    }

    return 0;
}
