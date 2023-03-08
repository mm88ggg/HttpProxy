#include <stdio.h>
#include "csapp.h"
#include "cache.h"
#include "sbuf.h"
#include "proxy.h"

void sigpipe_handler(int sig) {
    printf("SIGPIPE caught!\n");
}

void *thread(void *vargp);
void doit(int);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void response_with_cache(int clientfd, cache_block *block);
void response_without_cache(int clientfd, char *buf, char *method, char *host, char *port, char *path);

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

sbuf_t sbuf;
cache_t cache;
cache_policy policy = lfu;

int main(int argc, char *argv[]) {
    // Check command line args
    if (argc < 2) {
        fprintf(stderr, "usage: %s <port> <eviction policy>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    printf("Proxy start on %s port.\n", argv[1]);

    // Set the eviction policy.
    if (argc == 2) {
        fprintf(stderr, "You didn't specify the eviction policy, so we use LFU as default.\n");
    } else {
        if (strcasecmp(argv[2], "LRU") == 0) {
            policy = lru;
        } else if (strcasecmp(argv[2], "LFU") == 0) {
            policy = lfu;
        } else {
            fprintf(stderr, "Unknown cache policy, use LFU instead.\n");
        }
    }

    // Ignore SIGPIPE.
    signal(SIGPIPE, sigpipe_handler);

    socklen_t clientlen;
    int listenfd = open_listenfd(argv[1]);
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    // Initialize the pre-thread pool.
    sbuf_init(&sbuf, SBUF_SIZE);
    for (int i = 0; i < THREAD_NUM; ++i) {
        pthread_create(&tid, NULL, thread, (void *) i);
    }

    // Initialize the cache.
    cache_init(&cache);

    // Loop forever
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        int connfd = accept(listenfd, (SA *) &clientaddr, &clientlen);
        // Insert the connfd into the buffer.
        sbuf_insert(&sbuf, connfd);
    }
    return 0;
}

void *thread(void *vargp) {
    // Detach the thread.
    pthread_detach(pthread_self());
    // Get the thread id.
    int t_id = (int) vargp;
    while (1) {
        int connfd = sbuf_remove(&sbuf);
        printf("******A thread %d received request.******\n", t_id);
        doit(connfd);
        Close(connfd);
    }
    return NULL;
}

void doit(int clientfd) {
    rio_t client_rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];

    // Read request line
    rio_readinitb(&client_rio, clientfd);
    if (!rio_readlineb(&client_rio, buf, MAXLINE)) {
        clienterror(clientfd, "client_rio", "400", "Bad Request", "Proxy couldn't read request");
        return;
    }
    // Parse the request line.
    sscanf(buf, "%s %s %s", method, uri, version);

    // Only support GET method
    if (strcasecmp(method, "GET") != 0) {
        clienterror(clientfd, method, "501", "Not Implemented", "Proxy does not implement this method");
        return;
    }

    // Find the host, port and path begin *****************************************************
    char host[MAXLINE], port[6], path[MAXLINE];
    // Skip the http://
    char *host_start = strstr(uri, "//");
    if (host_start == NULL) {
        host_start = uri;
    } else {
        host_start += 2;
    }

    char *host_end = strstr(host_start, ":");
    // If ":" appears after the "/", it will not be the position of port.
    if (host_end > strstr(host_start, "/")) {
        host_end = NULL;
    }
    if (host_end == NULL) {
        host_end = strstr(host_start, "/");
        if (host_end == NULL) {
            strcpy(host, host_start);
            strcpy(port, "80");
            strcpy(path, "/");
        } else {
            strncpy(host, host_start, host_end - host_start);
            host[host_end - host_start] = '\0';
            strcpy(port, "80");
            strcpy(path, host_end);
        }
    } else {
        strncpy(host, host_start, host_end - host_start);
        host[host_end - host_start] = '\0';
        host_end += 1;
        char *port_end = strstr(host_end, "/");
        if (port_end == NULL) {
            strcpy(port, host_end);
            strcpy(path, "/");
        } else {
            strncpy(port, host_end, port_end - host_end);
            port[port_end - host_end] = '\0';
            strcpy(path, port_end);
        }
    }
    // Find the host, port and path end *****************************************************

    printf("host: %s, port: %s, path: %s\r\n", host, port, path);

    // Check the cache.
    cache_block *block = find_cache_block(&cache, host, port, path, policy);

    // If the block is not in the cache, we need to fetch it from the server.
    // Otherwise, we can just send the response to the client.
    if (block != NULL) {
        printf("Cache hit!\n");
        response_with_cache(clientfd, block);
    } else {
        printf("Cache miss!\n");
        response_without_cache(clientfd, buf, method, host, port, path);
    }
}

void response_with_cache(int clientfd, cache_block *block) {
    rio_writen(clientfd, block->content, block->size);
}

void response_without_cache(int clientfd, char *buf, char *method, char *host, char *port, char *path) {
    char *version = "HTTP/1.0";
    int tinyfd = open_clientfd(host, port);
    if (tinyfd < 0) {
        clienterror(clientfd, host, "404", "Not Found", "Proxy couldn't find the server");
        return;
    }

    ssize_t n;
    rio_t tiny_rio;
    rio_readinitb(&tiny_rio, tinyfd);

    // Send the request to the server.
    sprintf(buf, "%s %s %s\r\n", method, path, version);
    rio_writen(tinyfd, buf, strlen(buf));

    // Send the request headers of proxy
    sprintf(buf, "Host: %s\r\n", host);
    rio_writen(tinyfd, buf, strlen(buf));
    rio_writen(tinyfd, (void *)user_agent_hdr, strlen(user_agent_hdr));
    sprintf(buf, "Connection: close\r\n");
    rio_writen(tinyfd, buf, strlen(buf));
    sprintf(buf, "Proxy-Connection: close\r\n");
    rio_writen(tinyfd, buf, strlen(buf));
    rio_writen(tinyfd, "\r\n", 2);

    // Read the response from tiny and send it to client and cache it
    char response[MAX_OBJECT_SIZE], *p_response = response;
    ssize_t response_size = 0;
    int can_cache = 1;
    while ((n = rio_readnb(&tiny_rio, buf, MAXLINE)) > 0) {
        if (response_size + n <= MAX_OBJECT_SIZE) {
            memcpy(p_response, buf, n);
            p_response += n;
        } else {
            can_cache = 0;
        }
        response_size += n;
        rio_writen(clientfd, buf, n);
    }

    // If the response is not too large, cache it.
    // Otherwise, do nothing.
    if (can_cache) {
        add_cache_block(&cache, host, port, path, response, response_size, policy);
    } else {
        printf("The response is too large to cache\r\n");
    }

    Close(tinyfd);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Proxy Error</title>");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Proxy server</em>\r\n");
    rio_writen(fd, buf, strlen(buf));
}
