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

#include <libnetfilter_log/libnetfilter_log.h>

#define NFLOG_GROUP 10

static volatile int running = 1;

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

    /* ---- Time ---- */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%H:%M:%S", tm);

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
    printf("│  Size     : %d bytes\n", payload_len);

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

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║       NFLOG Packet Listener (Group %d)          ║\n", NFLOG_GROUP);
    printf("║       Press Ctrl+C to stop                      ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    struct nflog_handle *h = nflog_open();
    if (!h) { fprintf(stderr, "ERROR: nflog_open() failed\n"); return 1; }

    nflog_unbind_pf(h, AF_INET);
    nflog_unbind_pf(h, AF_INET6);

    if (nflog_bind_pf(h, AF_INET) < 0)  { fprintf(stderr, "ERROR: bind AF_INET\n");  return 1; }
    if (nflog_bind_pf(h, AF_INET6) < 0) { fprintf(stderr, "ERROR: bind AF_INET6\n"); return 1; }

    struct nflog_g_handle *gh = nflog_bind_group(h, NFLOG_GROUP);
    if (!gh) { fprintf(stderr, "ERROR: bind group %d\n", NFLOG_GROUP); return 1; }

    if (nflog_set_mode(gh, NFULNL_COPY_PACKET, 0xFFFF) < 0) {
        fprintf(stderr, "ERROR: set mode\n"); return 1;
    }

    nflog_callback_register(gh, &callback, NULL);

    int fd = nflog_fd(h);
    printf("[INIT] Listening on fd=%d\n\n", fd);

    char buf[65535];
    int rv;

    while (running) {
        rv = recv(fd, buf, sizeof(buf), 0);
        if (rv > 0)       nflog_handle_packet(h, buf, rv);
        else if (rv < 0)  { if (running) perror("recv()"); break; }
        else              break;
    }

    printf("\n[CLEANUP] Done.\n");
    nflog_unbind_group(gh);
    nflog_close(h);
    return 0;
}
