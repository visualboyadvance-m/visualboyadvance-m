/*
 * BSD-style sockets for classic Mac OS (Retro68), built on MacTCP.
 *
 * Classic Mac OS has no socket API: TCP/IP is the MacTCP driver (".IPP"),
 * driven with PBControl parameter blocks, plus the DNR code resource for
 * name lookups. Open Transport (Mac OS 7.6 and later) still answers MacTCP
 * calls through its compatibility layer, so one implementation reaches
 * System 7 through Mac OS 9.
 *
 * This header gives the subset of the BSD API that SFML's network module
 * (and VBA-M's link code) is written against -- socket(), bind(), listen(),
 * accept(), connect(), send()/recv(), sendto()/recvfrom(), select(),
 * getsockname()/getpeername(), setsockopt()/getsockopt(), getaddrinfo() --
 * so that SFML's portable socket code compiles unchanged on top of it.
 * The <sys/socket.h>, <netinet/in.h>, <netinet/tcp.h>, <arpa/inet.h> and
 * <netdb.h> next to it all include this file.
 *
 * Socket descriptors are small integers in their own range (see
 * MACTCP_FIRST_FD) below newlib's FD_SETSIZE, so they fit an fd_set, but
 * they are not file descriptors: close them with mactcp_close(), not
 * close(), and use mactcp_set_nonblocking() in place of fcntl(O_NONBLOCK).
 *
 * Everything runs on the caller's thread: a blocking call polls the
 * driver, napping through usleep() between polls (the wx port's usleep()
 * waits through SDL, which keeps the sound going and lets other
 * applications run).
 */

#ifndef SFML_MACTCP_SOCKETS_H
#define SFML_MACTCP_SOCKETS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Socket descriptors are MACTCP_FIRST_FD and up (16 of them). */
#define MACTCP_FIRST_FD 32

/* ---- <sys/socket.h> ---------------------------------------------------- */

typedef __socklen_t socklen_t;
typedef __sa_family_t sa_family_t;

#define AF_UNSPEC 0
#define AF_INET 2
#define PF_UNSPEC AF_UNSPEC
#define PF_INET AF_INET

#define SOCK_STREAM 1
#define SOCK_DGRAM 2

#define SOL_SOCKET 0xffff
#define SO_DEBUG 0x0001
#define SO_REUSEADDR 0x0004
#define SO_KEEPALIVE 0x0008
#define SO_BROADCAST 0x0020
#define SO_LINGER 0x0080
#define SO_REUSEPORT 0x0200
#define SO_SNDBUF 0x1001
#define SO_RCVBUF 0x1002
#define SO_ERROR 0x1007
#define SO_TYPE 0x1008

#define MSG_OOB 0x1
#define MSG_PEEK 0x2
#define MSG_DONTWAIT 0x80

/* How many connections a listener keeps open for (see listen()). */
#define SOMAXCONN 4

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

struct sockaddr {
    uint8_t sa_len;
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_storage {
    uint8_t ss_len;
    sa_family_t ss_family;
    char ss_data[126];
};

int socket(int domain, int type, int protocol);
int bind(int fd, const struct sockaddr* addr, socklen_t len);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr* addr, socklen_t* len);
int connect(int fd, const struct sockaddr* addr, socklen_t len);
ssize_t send(int fd, const void* buf, size_t len, int flags);
ssize_t recv(int fd, void* buf, size_t len, int flags);
ssize_t sendto(int fd, const void* buf, size_t len, int flags,
               const struct sockaddr* to, socklen_t tolen);
ssize_t recvfrom(int fd, void* buf, size_t len, int flags,
                 struct sockaddr* from, socklen_t* fromlen);
int getsockname(int fd, struct sockaddr* addr, socklen_t* len);
int getpeername(int fd, struct sockaddr* addr, socklen_t* len);
int setsockopt(int fd, int level, int name, const void* value, socklen_t len);
int getsockopt(int fd, int level, int name, void* value, socklen_t* len);
int shutdown(int fd, int how);

/* close() and fcntl() for socket descriptors. */
int mactcp_close(int fd);
int mactcp_set_nonblocking(int fd, int nonblocking);

/* True when fd is a live socket descriptor of this layer. */
int mactcp_is_socket(int fd);

/* Whether MacTCP (or Open Transport's MacTCP emulation) can be opened.
   Returns 0 when it can, else a negative errno-style code. */
int mactcp_available(void);

/* The host's own IPv4 address, in network byte order (0 when unknown). */
uint32_t mactcp_local_address(void);

/* Abort and release every open stream. Registered with atexit() when the
   driver is first opened: MacTCP keeps pointers into the application heap
   (the stream buffers) and must not outlive the application's use of them. */
void mactcp_shutdown(void);

/* ---- <netinet/in.h> ---------------------------------------------------- */

#define IPPROTO_IP 0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

#define INADDR_ANY ((in_addr_t)0x00000000)
#define INADDR_LOOPBACK ((in_addr_t)0x7f000001)
#define INADDR_BROADCAST ((in_addr_t)0xffffffff)
#define INADDR_NONE ((in_addr_t)0xffffffff)

#define INET_ADDRSTRLEN 16

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    uint8_t sin_len;
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

/* Both MacTCP hosts (68k and PowerPC) are big-endian: network order is
   host order. */
#ifndef htonl
#define htonl(x) ((uint32_t)(x))
#define ntohl(x) ((uint32_t)(x))
#define htons(x) ((uint16_t)(x))
#define ntohs(x) ((uint16_t)(x))
#endif

/* ---- <netinet/tcp.h> --------------------------------------------------- */

#define TCP_NODELAY 0x01

/* ---- <arpa/inet.h> ----------------------------------------------------- */

int inet_pton(int af, const char* src, void* dst);
const char* inet_ntop(int af, const void* src, char* dst, socklen_t size);
in_addr_t inet_addr(const char* cp);
int inet_aton(const char* cp, struct in_addr* addr);

/* ---- <netdb.h> --------------------------------------------------------- */

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr* ai_addr;
    char* ai_canonname;
    struct addrinfo* ai_next;
};

#define AI_PASSIVE 0x0001
#define AI_CANONNAME 0x0002
#define AI_NUMERICHOST 0x0004

#define EAI_AGAIN 2
#define EAI_BADFLAGS 3
#define EAI_FAIL 4
#define EAI_FAMILY 5
#define EAI_MEMORY 6
#define EAI_NONAME 8
#define EAI_SERVICE 9
#define EAI_SOCKTYPE 10
#define EAI_SYSTEM 11

/* Name lookups go through the MacTCP domain name resolver (the 'dnrp' code
   resource of the MacTCP or TCP/IP control panel). */
int getaddrinfo(const char* node, const char* service,
                const struct addrinfo* hints, struct addrinfo** res);
void freeaddrinfo(struct addrinfo* res);
const char* gai_strerror(int error);

/* Fills name with the host's dotted address (MacTCP knows no host name). */
int gethostname(char* name, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SFML_MACTCP_SOCKETS_H */
