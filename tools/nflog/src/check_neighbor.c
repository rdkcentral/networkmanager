    #include <arpa/inet.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 8192

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

int check_neighbor(const char *target_ip) {
    int sock = -1;
    char buf[BUF_SIZE];
    struct sockaddr_nl local = {0};
    struct sockaddr_nl kernel = {0};
    struct in_addr target_addr;

    if (inet_pton(AF_INET, target_ip, &target_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", target_ip);
        return -1;
    }

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    local.nl_family = AF_NETLINK;
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    struct {
        struct nlmsghdr nlh;
        struct ndmsg ndm;
    } req = {0};

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ndmsg));
    req.nlh.nlmsg_type = RTM_GETNEIGH;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nlh.nlmsg_seq = 1;
    req.ndm.ndm_family = AF_INET;

    kernel.nl_family = AF_NETLINK;

    if (sendto(sock, &req, req.nlh.nlmsg_len, 0,
               (struct sockaddr *)&kernel, sizeof(kernel)) < 0) {
        perror("sendto");
        close(sock);
        return -1;
    }

    while (1) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len < 0) {
            perror("recv");
            close(sock);
            return -1;
        }

        for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
             NLMSG_OK(nlh, len);
             nlh = NLMSG_NEXT(nlh, len)) {

            if (nlh->nlmsg_type == NLMSG_DONE) {
                close(sock);
                return 0; // not found
            }

            if (nlh->nlmsg_type == NLMSG_ERROR) {
                fprintf(stderr, "Netlink error\n");
                close(sock);
                return -1;
            }

            struct ndmsg *ndm = (struct ndmsg *)NLMSG_DATA(nlh);
            if (ndm->ndm_family != AF_INET)
                continue;

            int attrlen = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*ndm));
            struct rtattr *rta = (struct rtattr *)((char *)ndm + NLMSG_ALIGN(sizeof(*ndm)));

            struct in_addr dst_addr = {0};
            bool found_dst = false;
            unsigned char mac[6] = {0};
            bool found_mac = false;

            for (; RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
                if (rta->rta_type == NDA_DST) {
                    memcpy(&dst_addr, RTA_DATA(rta), sizeof(dst_addr));
                    found_dst = true;
                } else if (rta->rta_type == NDA_LLADDR) {
                    size_t maclen = RTA_PAYLOAD(rta);
                    if (maclen >= 6) {
                        memcpy(mac, RTA_DATA(rta), 6);
                        found_mac = true;
                    }
                }
            }

            if (found_dst && dst_addr.s_addr == target_addr.s_addr) {
                char ipstr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &dst_addr, ipstr, sizeof(ipstr));

                printf("Neighbor entry found for %s\n", ipstr);
                printf("Interface index: %d\n", ndm->ndm_ifindex);
                printf("State: %s\n", nud_state_to_string(ndm->ndm_state));

                if (found_mac) {
                    printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                } else {
                    printf("MAC: not available\n");
                }

                if (is_router_reachable_by_state(ndm->ndm_state)) {
                    printf("Result: router is likely reachable\n");
                    close(sock);
                    return 1;
                } else {
                    printf("Result: router is not reachable\n");
                    close(sock);
                    return 0;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <router-ip>\n", argv[0]);
        return 1;
    }

    int ret = check_neighbor(argv[1]);
    if (ret < 0) {
        fprintf(stderr, "Check failed\n");
        return 2;
    }

    return ret ? 0 : 3;
}
