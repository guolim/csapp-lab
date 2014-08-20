/**
 * proxy.c - A simple concurrent HTTP proxy with cache.
 *
 * Guoli Ma - guolim@andrew.cmu.edu
 *
 * Proxy receives client HTTP request, parses the url, builds up new HTTP request
 * and then sends new request to target server. After receiving server response,
 * cache it and forward it to the client.
 */

#include "csapp.h"
#include "cache.h"

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux"
" x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,"
"application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxt_connection_hdr = "Proxy-Connection: close\r\n";

/* Global variables */
static cache_t *cache;

/* Helper inline functions */
/**
 * start_with - Test whether string *str* starts with *start*.
 */
static inline int start_with(const char *str, const char *start) 
{
    return strncasecmp(str, start, strlen(start)) == 0;
}

/**
 * is_fixed_header - Whether a request header a fixed header.
 */
static inline int is_fixed_header(char *header_line)
{
    if (start_with(header_line, "User-Agent") ||
        start_with(header_line, "Accept") ||
        start_with(header_line, "Accept-Encoding") ||
        start_with(header_line, "Connection") ||
        start_with(header_line, "Proxy-Connection")) {
        /* Fixed headers */
        return 1;
    }
    return 0;
}

/**
 * build_fixed_header - Build up fixed proxy HTTP request header.
 */
static inline void build_fixed_header(char *request)
{
    sprintf(request, "%s%s", request, user_agent_hdr);
    sprintf(request, "%s%s", request, accept_hdr);
    sprintf(request, "%s%s", request, accept_encoding_hdr);
    sprintf(request, "%s%s", request, connection_hdr);
    sprintf(request, "%s%s", request, proxt_connection_hdr);
}

/* Function protocol */
void *thread(void *vargp);
void process_request(int fd);
int get_port(const char *port);
int parse_url(const char *url, char *host_name, int *port, char *uri);
void build_new_request(rio_t *rp,
                       char *new_request,
                       const char *host_name,
                       int port,
                       const char *uri);
ssize_t my_rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t my_rio_readn(int fd, void *usrbuf, size_t n);
ssize_t my_rio_writen(int fd, void *usrbuf, size_t n);
void clienterror(int fd,
                 char *cause,
                 char *errnum,
                 char *shortmsg,
                 char *longmsg);

int main(int argc, char *argv[])
{
    /* Check command line options */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Ignore SIGPIPE signal */
    Signal(SIGPIPE, SIG_IGN);

    /* Get listen port number */
    int port = get_port(argv[1]);

    cache = init_cache();

    const int listen_fd = Open_listenfd(port);  /* Proxy listen descriptor */
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *conn_fdp = (int *)Malloc(sizeof(int));
        *conn_fdp = Accept(listen_fd, (SA *)&client_addr, &client_len);

        /* Create thread to handle client request */
        pthread_t tid;
        Pthread_create(&tid, NULL, thread, conn_fdp);
    }

    return 0;
}

/**
 * thread - Handle client HTTP request, call process_request to process 
 *      request.
 */
void *thread(void *vargp) 
{
    int conn_fd = *((int *)vargp);
    Pthread_detach(Pthread_self());
    Free(vargp);
    process_request(conn_fd);
    Close(conn_fd);
    return NULL;
}

/**
 * process_request - Process one HTTP request from client. Send a new
 *      request to the server, receive the response, and forward it 
 *      back to client.
 */
void process_request(int fd) 
{
    rio_t rio_client;
    Rio_readinitb(&rio_client, fd);

    /* 1. Receive client request, build new request. */
    /* Read request line */
    char req_line[MAXLINE];
    my_rio_readlineb(&rio_client, req_line, MAXLINE);

    printf("Received request:\n");
    printf("%s", req_line);

    /* Extract <method> <url> <version> */
    char method[MAXLINE];
    char url[MAXLINE];
    char version[MAXLINE];
    if (sscanf(req_line, "%s %s %s", method, url, version) < 3) {
        return;
    }
    /* Not GET method */
    if (strcasecmp(method, "GET") != 0) {
        printf("Method is not GET, send errno to client\n");
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy dose not implement this method");
        return;
    }

    /* Parse url to get host, port, and uri */
    char uri[MAXLINE];
    char host_name[MAXLINE];
    int port = 80;
    if (parse_url(url, host_name, &port, uri) == 0) {
        clienterror(fd, "Parse url wrong", "400", "Bad request", 
                    "Proxy only support url started with <em>http://</em>");
        return;
    }
    char request[MAXLINE];
    /* Build proxy HTTP request for the server */
    build_new_request(&rio_client, request, host_name, port, uri);

    char content[MAX_OBJECT_SIZE];  /* Server response */
    size_t content_size = 0;        /* Response size */

    /* 2. Cache hit, send cached object back to client. */
    int hit = get_cached_object(cache, url, content, &content_size);
    if (hit) {
        my_rio_writen(fd, content, content_size);
        return;
    }

    /* 3. Cache miss, send request to server. */
    int server_fd = open_clientfd_r(host_name, port);
    if (server_fd < 0) {
        clienterror(fd, host_name, "500", "Internal Server Error",
                    "Proxy cannot connect to server");
        return;
    }
    my_rio_writen(server_fd, request, strlen(request));
    printf("Forward request:\n");
    printf("%s\n", request);

    /* 4. Receive server response, forward response to client. */
    char buf[MAX_OBJECT_SIZE];
    ssize_t read_len;
    int fit = 1;
    while ((read_len = my_rio_readn(server_fd, buf, MAX_OBJECT_SIZE)) != 0) {
        if ((content_size + read_len) < MAX_OBJECT_SIZE) {
            memcpy(content + content_size, buf, read_len);
            content_size += read_len;
        } else {
            fit = 0;
        }
        my_rio_writen(fd, buf, read_len);
        memset(buf, 0, sizeof(buf));
    }
    Close(server_fd);

    /* 5. Cache received content. */
    if (fit) {
        cache_insert(cache, url, content, content_size);
    }
}

/**
 * get_port - Convert a string represented port number to a integer.
 */
int get_port(const char *port_str) 
{
    char *end;
    const long port = strtol(port_str, &end, 10);
    if (end == port_str || *end != '\0' || errno == ERANGE) {
        fprintf(stderr, "%s not valid integer\n", port_str);
        exit(EXIT_FAILURE);
    }
    if (port < 0 || port > 65535) {
        fprintf(stderr, "port number out of range.\n");
        exit(EXIT_FAILURE);
    }
    return (int)port;
}

/**
 * parse_url - According to RFC 1945, proxy will get a absolute URL. So this
 *      function will help to extract server hostname, port, and uri.
 */
int parse_url(const char *url, char *host_name, int *port, char *uri)
{
    char *ptr = strstr(url, "http://");
    if (ptr == NULL || ptr != url) {
        /* url does not start with ``http://'' */
        return 0;
    } else {
        ptr += strlen("http://"); /* skip http:// */

        /* Extract uri */
        char buf[MAXLINE];
        char uri_buf[MAXLINE];
        memset(uri_buf, 0, sizeof(uri_buf));
        sscanf(ptr, "%[^/]%s", buf, uri_buf);
        if (strlen(uri_buf) == 0) {
            strcpy(uri_buf, "/");
        }
        strcpy(uri, uri_buf);

        /* Extract host name and optional port */
        char port_buf[7];
        memset(port_buf, 0, sizeof(port_buf));
        sscanf(buf, "%[^:]%s", host_name, port_buf);
        if (strlen(port_buf) != 0) {
            *port = get_port(port_buf + 1);
        }
        return 1;
    }
}

/**
 * build_new_request - Receive request headers from the client, build new
 *      request according to uri and these headers.
 */
void build_new_request(rio_t *rp,
                       char *new_request,
                       const char *host_name,
                       int port,
                       const char *uri) 
{
    /* Build request line */
    sprintf(new_request, "GET %s HTTP/1.0\r\n", uri);
    /* Build fixed request headers */
    build_fixed_header(new_request);

    /* Parse old request header and build new ones */
    char buf[MAXLINE];
    int req_has_host = 0;
    my_rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        printf("%s", buf);
        if (start_with(buf, "Host")) {
            /* Client request has Host header, use it instead of build one */
            req_has_host = 1;
        } 
        if (!is_fixed_header(buf)) {
            /* Not fixed headers, copy it to new proxy request */
            sprintf(new_request, "%s%s", new_request, buf);
        }
        memset(buf, 0, sizeof(buf));
        my_rio_readlineb(rp, buf, MAXLINE);
    }
    
    /* Client header does not have Host header */
    if (!req_has_host) {
        if (port == 80) {
            sprintf(new_request, "%sHost: %s\r\n", new_request, host_name);
        } else {
            sprintf(new_request,
                    "%sHost: %s:%d\r\n",
                    new_request,
                    host_name,
                    port);
        }
    }
    sprintf(new_request, "%s\r\n", new_request);
}

/**
 * my_rio_readlineb - Ignore ECONNRESET error.
 */
ssize_t my_rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
        if (errno == ECONNRESET) {
            fprintf(stderr, 
                    "readline error: Connection (%d) has been closed\n",
                    rp->rio_fd);
        }
    }
    return rc;
} 

/**
 * my_rio_readn - Do not terminate when encounters ECONNRESET errno
 */
ssize_t my_rio_readn(int fd, void *usrbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_readn(fd, usrbuf, n)) < 0) {
        if (errno == ECONNRESET) {
            fprintf(stderr, 
                    "readn error: Connection (%d) has been closed\n",
                    fd);
        } else {
            unix_error("my_rio_readlineb error");
        }
    }
    return rc;
}

/**
 * my_rio_writen - Do not terminate when encounters EPIPE errno
 */
ssize_t my_rio_writen(int fd, void *usrbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_writen(fd, usrbuf, n)) < 0) {
        if (errno == EPIPE) {
            fprintf(stderr, 
                    "write error: Connection (%d) has been closed\n",
                    fd);
        }
    }
    return rc;
}

/**
 * clienterror - Returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
            char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    my_rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    my_rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    my_rio_writen(fd, buf, strlen(buf));
    my_rio_writen(fd, body, strlen(body));
}
