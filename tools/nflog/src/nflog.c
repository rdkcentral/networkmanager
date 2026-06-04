#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <net/if.h>
#include <time.h>
#include <errno.h>
#include <malloc.h>

#include <libnetfilter_log/libnetfilter_log.h>

#define NFLOG_GROUP 10

/*
 * Copy only packet headers from kernel to userspace.
 * 80 bytes is enough for:
 * - IPv4 + TCP base header (40 bytes) plus TCP options
 * - IPv6 + TCP base header (60 bytes) plus small options
 * - IPv4/IPv6 + UDP headers (28/48 bytes)
 * This avoids full payload copies (for example 1500-byte frames or larger),
 * which is the primary driver of high NFLOG memory usage.
 */
#define NFLOG_COPY_SIZE 80

static struct nflog_handle *g_h = NULL;
static struct nflog_g_handle *g_gh = NULL;

static volatile int running = 1;

static void cleanup(void)
{
    if (g_gh) {
        nflog_set_mode(g_gh, NFULNL_COPY_NONE, 0);
        nflog_unbind_group(g_gh);
        g_gh = NULL;
    }

    if (g_h) {
        nflog_unbind_pf(g_h, AF_INET);
        nflog_unbind_pf(g_h, AF_INET6);
        nflog_close(g_h);
        g_h = NULL;
    }
}

static void handle_signal(int sig)
{
    (void)sig;
    running = 0;
}

static const char *port_to_service(unsigned short port, const char *proto)
{
    switch (port) {
        case 123: return "NTP";
        case 80:  return "HTTP";
        case 443:
            if (strcmp(proto, "UDP") == 0) return "HTTP3/QUIC";
            else                          return "HTTPS/TLS";
        default:  return "UNKNOWN";
    }
}

static int get_http_response_code(const char *data, int len)
{
    if (len < 12)                        /* "HTTP/1.1 200" = 12 bytes minimum */
        return -1;
    if (strncmp(data, "HTTP/", 5) != 0)  /* must start with HTTP/             */
        return -1;
    return atoi(data + 9);               /* code starts at position 9         */
}

static int callback(struct nflog_g_handle *gh,
                    struct nfgenmsg *nfmsg,
                    struct nflog_data *nfa,
                    void *data)
{
    (void)gh;
    (void)nfmsg;
    (void)data;

    /* ---- Time (monotonic clock, immune to NTP jumps) ---- */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    char timestr[64];
    snprintf(timestr, sizeof(timestr), "%ld.%03ld", (long)ts.tv_sec, ts.tv_nsec / 1000000);

    /* ---- Prefix ---- */
    char *prefix = nflog_get_prefix(nfa);
    if (!prefix) prefix = "no_prefix";

    /* ---- Packet header ---- */
    struct nfulnl_msg_packet_hdr *ph = nflog_get_msg_packet_hdr(nfa);
    if (!ph) return 0;

    /* ---- Payload ---- */
    char *payload = NULL;
    int payload_len = nflog_get_payload(nfa, &payload);
    if (!payload || payload_len <= 0) return 0;

    /* ---- Interface names ---- */
    char indev_name[IF_NAMESIZE]  = "none";
    char outdev_name[IF_NAMESIZE] = "none";

    uint32_t indev  = nflog_get_indev(nfa);
    uint32_t outdev = nflog_get_outdev(nfa);

    if (indev > 0) {
        if (!if_indextoname(indev, indev_name)) {
            snprintf(indev_name, sizeof(indev_name), "if%u", indev);
        }
    }
    if (outdev > 0) {
        if (!if_indextoname(outdev, outdev_name)) {
            snprintf(outdev_name, sizeof(outdev_name), "if%u", outdev);
        }
    }

    /* ---- Variables to fill ---- */
    char src_ip[INET6_ADDRSTRLEN] = "???";
    char dst_ip[INET6_ADDRSTRLEN] = "???";
    unsigned short src_port = 0;
    unsigned short dst_port = 0;
    const char *proto_str  = "???";
    const char *ip_ver_str = "???";
    const char *service    = "???";
    const char *http_data  = NULL;   /* points to TCP payload when proto=TCP */
    int http_data_len      = 0;

    /* ---- Parse IPv4 ---- */
    if (ntohs(ph->hw_protocol) == 0x0800) {
        ip_ver_str = "IPv4";
        if (payload_len < (int)sizeof(struct iphdr)) return 0;

        struct iphdr *ip = (struct iphdr *)payload;
        inet_ntop(AF_INET, &ip->saddr, src_ip, sizeof(src_ip));
        inet_ntop(AF_INET, &ip->daddr, dst_ip, sizeof(dst_ip));

        int ip_hdr_len = ip->ihl * 4;
        char *transport = payload + ip_hdr_len;
        int remaining = payload_len - ip_hdr_len;

        if (ip->protocol == IPPROTO_TCP && remaining >= (int)sizeof(struct tcphdr)) {
            proto_str = "TCP";
            struct tcphdr *tcp = (struct tcphdr *)transport;
            src_port = ntohs(tcp->source);
            dst_port = ntohs(tcp->dest);
            int tcp_hdr_len = tcp->doff * 4;
            if (remaining > tcp_hdr_len) {
                http_data     = transport + tcp_hdr_len;
                http_data_len = remaining  - tcp_hdr_len;
            }
        }
        else if (ip->protocol == IPPROTO_UDP && remaining >= (int)sizeof(struct udphdr)) {
            proto_str = "UDP";
            struct udphdr *udp = (struct udphdr *)transport;
            src_port = ntohs(udp->source);
            dst_port = ntohs(udp->dest);
        }
        else {
            proto_str = "OTHER";
        }
    }
    /* ---- Parse IPv6 ---- */
    else if (ntohs(ph->hw_protocol) == 0x86DD) {
        ip_ver_str = "IPv6";
        if (payload_len < (int)sizeof(struct ip6_hdr)) return 0;

        struct ip6_hdr *ip6 = (struct ip6_hdr *)payload;
        inet_ntop(AF_INET6, &ip6->ip6_src, src_ip, sizeof(src_ip));
        inet_ntop(AF_INET6, &ip6->ip6_dst, dst_ip, sizeof(dst_ip));

        char *transport = payload + sizeof(struct ip6_hdr);
        int remaining = payload_len - (int)sizeof(struct ip6_hdr);

        if (ip6->ip6_nxt == IPPROTO_TCP && remaining >= (int)sizeof(struct tcphdr)) {
            proto_str = "TCP";
            struct tcphdr *tcp = (struct tcphdr *)transport;
            src_port = ntohs(tcp->source);
            dst_port = ntohs(tcp->dest);
            int tcp_hdr_len = tcp->doff * 4;
            if (remaining > tcp_hdr_len) {
                http_data     = transport + tcp_hdr_len;
                http_data_len = remaining  - tcp_hdr_len;
            }
        }
        else if (ip6->ip6_nxt == IPPROTO_UDP && remaining >= (int)sizeof(struct udphdr)) {
            proto_str = "UDP";
            struct udphdr *udp = (struct udphdr *)transport;
            src_port = ntohs(udp->source);
            dst_port = ntohs(udp->dest);
        }
        else {
            proto_str = "OTHER";
        }
    }
    else {
        return 0;
    }

    /* ---- Identify service ---- */
    service = port_to_service(
        (src_port == 123 || src_port == 80 || src_port == 443) ? src_port : dst_port,
        proto_str
    );

    /* ---- Print ---- */
    printf("┌──────────────────────────────────────────────────\n");
    printf("│ [%s] Packet Received!\n", timestr);
    printf("│\n");
    printf("│  Prefix   : %s\n", prefix);
    printf("│  IP Ver   : %s\n", ip_ver_str);
    printf("│  Protocol : %s\n", proto_str);
    printf("│  Service  : %s\n", service);
    printf("│  Source    : %s:%u\n", src_ip, src_port);
    printf("│  Dest     : %s:%u\n", dst_ip, dst_port);
    printf("│  In Iface : %s (index %u)\n", indev_name, indev);
    printf("│  Out Iface: %s (index %u)\n", outdev_name, outdev);
    printf("│  CopySize : %d bytes (cap %d)\n", payload_len, NFLOG_COPY_SIZE);

    /* ---- HTTP response code (only when service=HTTP and TCP payload exists) ---- */
    if (strcmp(service, "HTTP") == 0 && http_data != NULL) {
        int http_code = get_http_response_code(http_data, http_data_len);
        if (http_code > 0) {
            printf("│  ────────────────────────────────────────────────\n");
            printf("│  HTTP Code: %d  ", http_code);
            switch (http_code) {
                case 200: printf("OK\n");                    break;
                case 201: printf("Created\n");               break;
                case 301: printf("Moved Permanently\n");     break;
                case 302: printf("Found (Redirect)\n");      break;
                case 304: printf("Not Modified\n");          break;
                case 400: printf("Bad Request\n");           break;
                case 401: printf("Unauthorized\n");          break;
                case 403: printf("Forbidden\n");             break;
                case 404: printf("Not Found\n");             break;
                case 500: printf("Internal Server Error\n"); break;
                case 502: printf("Bad Gateway\n");           break;
                case 503: printf("Service Unavailable\n");   break;
                default:  printf("Unknown\n");               break;
            }
            printf("│  ────────────────────────────────────────────────\n");
        }
    }

    printf("└──────────────────────────────────────────────────\n\n");

    return 0;
}

int main(void)
{
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(cleanup);

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║       NFLOG Packet Listener (Group %d)          ║\n", NFLOG_GROUP);
    printf("║       Press Ctrl+C to stop                      ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    g_h = nflog_open();
    if (!g_h) { fprintf(stderr, "ERROR: nflog_open() failed\n"); return 1; }

    nflog_unbind_pf(g_h, AF_INET);
    nflog_unbind_pf(g_h, AF_INET6);

    if (nflog_bind_pf(g_h, AF_INET) < 0)  { fprintf(stderr, "ERROR: bind AF_INET\n");  return 1; }
    if (nflog_bind_pf(g_h, AF_INET6) < 0) { fprintf(stderr, "ERROR: bind AF_INET6\n"); return 1; }

    g_gh = nflog_bind_group(g_h, NFLOG_GROUP);
    if (!g_gh) { fprintf(stderr, "ERROR: bind group %d\n", NFLOG_GROUP); return 1; }

    if (nflog_set_mode(g_gh, NFULNL_COPY_PACKET, NFLOG_COPY_SIZE) < 0) {
        fprintf(stderr, "ERROR: set mode\n"); return 1;
    }

    /*
     * Queue restriction controls (per libnetfilter_log docs):
     *
     * 1. qthresh: Max number of log entries the kernel batches in the
     *    internal nflog buffer before flushing to the netlink socket.
     *    Setting to 1 = flush immediately per packet, minimising the
     *    amount of kernel memory held at any moment.
     *
     * 2. timeout: Max time (in hundredths of a second) kernel holds a
     *    batch before flushing, even if qthresh isn't reached.
     *    Safety net so packets don't sit in kernel memory indefinitely.
     *
     * NOTE: nflog_set_nlbufsiz() exists but the official docs say:
     *   "The use of this function is strongly discouraged.
     *    The default buffer size (one memory page) provides the
     *    optimum results in terms of performance."
     *
     * The primary memory control should be done at the iptables layer
     * using "-m limit" to restrict how many packets reach NFLOG.
     */
    nflog_set_qthresh(g_gh, 1);        /* flush every packet immediately    */
    nflog_set_timeout(g_gh, 10);       /* 0.1s max batch hold time          */

    nflog_callback_register(g_gh, &callback, NULL);

    int fd = nflog_fd(g_h);
    printf("[INIT] Listening on fd=%d\n\n", fd);

    char buf[4096];
    int rv;
    int pkt_count = 0;

    while (running) {
        rv = recv(fd, buf, sizeof(buf), 0);
        if (rv > 0) {
            nflog_handle_packet(g_h, buf, rv);
            /*
             * Every 10 packets, release freed heap pages back to the OS.
             * Without this, glibc holds freed memory mapped indefinitely
             * and RSS stays high even when the app is idle in recv().
             */
            if (++pkt_count >= 10) {
                malloc_trim(0);
                pkt_count = 0;
            }
        }
        else if (rv < 0) {
            if (errno == ENOBUFS) {
                fprintf(stderr, "WARN: recv() ENOBUFS \xe2\x80\x93 kernel dropped packets, continuing\n");
                malloc_trim(0);
                continue;
            }
            if (running) perror("recv()");
            break;
        }
        else              break;
    }

    printf("[CLEANUP] Done.\n");
    cleanup();
    return 0;
}
