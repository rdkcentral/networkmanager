    #include <arpa/inet.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 8192

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int signum) {
    (void)signum;
    g_running = 0;
}

static const char *nud_state_to_string(unsigned short state) {
    switch (state) {
        case NUD_INCOMPLETE: return "INCOMPLETE";
        case NUD_REACHABLE:  return "REACHABLE";
        case NUD_STALE:      return "STALE";
        case NUD_DELAY:      return "DELAY";
        case NUD_PROBE:      return "PROBE";
        case NUD_FAILED:     return "FAILED";
        case NUD_NOARP:      return "NOARP";
        case NUD_PERMANENT:  return "PERMANENT";
        default:             return "UNKNOWN";
    }
}

static bool is_router_reachable_by_state(unsigned short state) {
    return (state == NUD_REACHABLE ||
            state == NUD_STALE ||
            state == NUD_DELAY ||
            state == NUD_PROBE ||
            state == NUD_PERMANENT);
}

static bool is_monitored_interface(int ifindex) {
    char ifname[IF_NAMESIZE] = {0};

    if (if_indextoname(ifindex, ifname) == NULL) {
        return false;
    }

    return (strcmp(ifname, "eth0") == 0 || strcmp(ifname, "wlan0") == 0);
}

static void print_neighbor_row(const struct nlmsghdr *nlh) {
    const struct ndmsg *ndm = (const struct ndmsg *)NLMSG_DATA(nlh);
    int attrlen = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*ndm));
    const struct rtattr *rta = (const struct rtattr *)((const char *)ndm + NLMSG_ALIGN(sizeof(*ndm)));

    int family = ndm->ndm_family;
    unsigned char dst_buf[sizeof(struct in6_addr)] = {0};
    bool found_dst = false;
    unsigned char mac[6] = {0};
    bool found_mac = false;

    size_t expected_addr_len = (family == AF_INET) ? sizeof(struct in_addr)
                                                   : sizeof(struct in6_addr);

    for (; RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
        if (rta->rta_type == NDA_DST) {
            if (RTA_PAYLOAD(rta) >= expected_addr_len) {
                memcpy(dst_buf, RTA_DATA(rta), expected_addr_len);
                found_dst = true;
            }
        } else if (rta->rta_type == NDA_LLADDR) {
            size_t maclen = RTA_PAYLOAD(rta);
            if (maclen >= 6) {
                memcpy(mac, RTA_DATA(rta), 6);
                found_mac = true;
            }
        }
    }

    if (!found_dst) {
        return;
    }

    char ipstr[INET6_ADDRSTRLEN] = {0};
    char ifname[IF_NAMESIZE] = {0};
    const char *family_str = (family == AF_INET) ? "IPv4" : "IPv6";

    if (inet_ntop(family, dst_buf, ipstr, sizeof(ipstr)) == NULL) {
        snprintf(ipstr, sizeof(ipstr), "invalid-ip");
    }

    if (if_indextoname(ndm->ndm_ifindex, ifname) == NULL) {
        snprintf(ifname, sizeof(ifname), "if#%d", ndm->ndm_ifindex);
    }

    printf("%-4s IP=%-39s IF=%-8s STATE=%-10s REACHABLE=%s",
           family_str,
           ipstr,
           ifname,
           nud_state_to_string(ndm->ndm_state),
           is_router_reachable_by_state(ndm->ndm_state) ? "yes" : "no");

    if (found_mac) {
        printf(" MAC=%02x:%02x:%02x:%02x:%02x:%02x",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        printf(" MAC=not-available");
    }

    printf("\n");
}

static int request_neighbor_dump(int sock, unsigned int seq) {
    struct {
        struct nlmsghdr nlh;
        struct ndmsg ndm;
    } req = {0};

    struct sockaddr_nl kernel = {0};
    kernel.nl_family = AF_NETLINK;

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ndmsg));
    req.nlh.nlmsg_type = RTM_GETNEIGH;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nlh.nlmsg_seq = seq;
    req.ndm.ndm_family = AF_UNSPEC;

    if (sendto(sock, &req, req.nlh.nlmsg_len, 0,
               (struct sockaddr *)&kernel, sizeof(kernel)) < 0) {
        perror("sendto");
        return -1;
    }

    return 0;
}

static int dump_neighbors(int sock, unsigned int seq) {
    char buf[BUF_SIZE];

    if (request_neighbor_dump(sock, seq) != 0) {
        return -1;
    }

    printf("---- Current ARP/Neighbor Table ----\n");

    while (g_running) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len < 0) {
            if (errno == EINTR && !g_running) {
                break;
            }
            perror("recv");
            return -1;
        }

        for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
             NLMSG_OK(nlh, len);
             nlh = NLMSG_NEXT(nlh, len)) {

            if (nlh->nlmsg_seq != seq) {
                continue;
            }

            if (nlh->nlmsg_type == NLMSG_DONE) {
                printf("------------------------------------\n");
                fflush(stdout);
                return 0;
            }

            if (nlh->nlmsg_type == NLMSG_ERROR) {
                const struct nlmsgerr *err = (const struct nlmsgerr *)NLMSG_DATA(nlh);
                if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
                    fprintf(stderr, "Malformed netlink error message\n");
                } else if (err->error != 0) {
                    errno = -err->error;
                    perror("netlink dump error");
                }
                return -1;
            }

            if (nlh->nlmsg_type != RTM_NEWNEIGH) {
                continue;
            }

            struct ndmsg *ndm = (struct ndmsg *)NLMSG_DATA(nlh);
            if (ndm->ndm_family != AF_INET && ndm->ndm_family != AF_INET6) {
                continue;
            }

            if (!is_monitored_interface(ndm->ndm_ifindex)) {
                continue;
            }

            print_neighbor_row(nlh);
        }
    }

    return 0;
}

int monitor_neighbors(void) {
    int sock = -1;
    struct sockaddr_nl local = {0};
    unsigned int seq = 1;
    char buf[BUF_SIZE];

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    local.nl_family = AF_NETLINK;
    local.nl_groups = RTMGRP_NEIGH;
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    if (dump_neighbors(sock, seq++) != 0) {
        close(sock);
        return -1;
    }

    printf("Monitoring ARP/neighbor table changes... Press Ctrl+C to stop.\n");
    fflush(stdout);

    while (g_running) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len < 0) {
            if (errno == EINTR && !g_running) {
                break;
            }
            perror("recv");
            close(sock);
            return -1;
        }

        bool table_changed = false;
        for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
             NLMSG_OK(nlh, len);
             nlh = NLMSG_NEXT(nlh, len)) {

            if (nlh->nlmsg_type == NLMSG_ERROR) {
                fprintf(stderr, "Netlink event error\n");
                close(sock);
                return -1;
            }

            if (nlh->nlmsg_type != RTM_NEWNEIGH && nlh->nlmsg_type != RTM_DELNEIGH) {
                continue;
            }

            struct ndmsg *ndm = (struct ndmsg *)NLMSG_DATA(nlh);
            if (ndm->ndm_family != AF_INET && ndm->ndm_family != AF_INET6) {
                continue;
            }

            if (!is_monitored_interface(ndm->ndm_ifindex)) {
                continue;
            }

            table_changed = true;
        }

        if (table_changed) {
            printf("\nNeighbor table changed. Refreshing status...\n");
            if (dump_neighbors(sock, seq++) != 0) {
                close(sock);
                return -1;
            }
        }
    }

    close(sock);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        fprintf(stderr, "Runs continuously and monitors IPv4/IPv6 neighbor table changes.\n");
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int ret = monitor_neighbors();
    if (ret != 0) {
        fprintf(stderr, "Monitor failed\n");
        return 2;
    }

    return 0;
}
