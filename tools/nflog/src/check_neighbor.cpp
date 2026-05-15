#include <arpa/inet.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define BUF_SIZE 8192

static std::vector<std::string> g_monitorIPs;
static std::mutex g_mutex;
static std::atomic<bool> g_running{false};
static std::thread g_monitorThread;

static const char *nud_state_to_string(unsigned short state)
{
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

static bool is_reachable(unsigned short state)
{
    return (state == NUD_REACHABLE ||
            state == NUD_STALE    ||
            state == NUD_DELAY    ||
            state == NUD_PROBE    ||
            state == NUD_PERMANENT);
}

static bool is_monitored_ip(const std::string &ip)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return std::find(g_monitorIPs.begin(), g_monitorIPs.end(), ip) != g_monitorIPs.end();
}

/* ------------------------------------------------------------------ */
/*  Initial one-shot dump: check current state of all monitored IPs   */
/* ------------------------------------------------------------------ */
static void check_initial_state()
{
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) return;

    struct sockaddr_nl local = {};
    local.nl_family = AF_NETLINK;
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        close(sock);
        return;
    }

    struct {
        struct nlmsghdr nlh;
        struct ndmsg    ndm;
    } req = {};

    req.nlh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ndmsg));
    req.nlh.nlmsg_type  = RTM_GETNEIGH;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nlh.nlmsg_seq   = 1;
    req.ndm.ndm_family  = AF_INET;

    struct sockaddr_nl kernel = {};
    kernel.nl_family = AF_NETLINK;

    if (sendto(sock, &req, req.nlh.nlmsg_len, 0,
               (struct sockaddr *)&kernel, sizeof(kernel)) < 0) {
        close(sock);
        return;
    }

    std::vector<std::string> found_ips;
    char buf[BUF_SIZE];
    bool done = false;

    while (!done) {
        ssize_t rlen = recv(sock, buf, sizeof(buf), 0);
        if (rlen <= 0) break;

        int len = static_cast<int>(rlen);
        for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
             NLMSG_OK(nlh, len);
             nlh = NLMSG_NEXT(nlh, len)) {

            if (nlh->nlmsg_type == NLMSG_DONE)  { done = true; break; }
            if (nlh->nlmsg_type == NLMSG_ERROR)  { done = true; break; }

            struct ndmsg *ndm = (struct ndmsg *)NLMSG_DATA(nlh);
            if (ndm->ndm_family != AF_INET)
                continue;

            int attrlen = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*ndm));
            struct rtattr *rta =
                (struct rtattr *)((char *)ndm + NLMSG_ALIGN(sizeof(*ndm)));

            char ipstr[INET_ADDRSTRLEN] = {};
            bool found = false;

            for (; RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
                if (rta->rta_type == NDA_DST) {
                    inet_ntop(AF_INET, RTA_DATA(rta), ipstr, sizeof(ipstr));
                    found = true;
                }
            }

            if (found && is_monitored_ip(ipstr)) {
                found_ips.push_back(ipstr);
                if (!is_reachable(ndm->ndm_state))
                    printf("[GW-MONITOR] INITIAL: %s is NOT reachable (state=%s)\n",
                           ipstr, nud_state_to_string(ndm->ndm_state));
                else
                    printf("[GW-MONITOR] INITIAL: %s is reachable (state=%s)\n",
                           ipstr, nud_state_to_string(ndm->ndm_state));
            }
        }
    }

    /* Report monitored IPs that have no neighbor entry at all */
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto &ip : g_monitorIPs) {
        if (std::find(found_ips.begin(), found_ips.end(), ip) == found_ips.end())
            printf("[GW-MONITOR] INITIAL: %s not found in neighbor table\n",
                   ip.c_str());
    }

    close(sock);
}

/* ------------------------------------------------------------------ */
/*  Process a single RTM_NEWNEIGH / RTM_DELNEIGH message              */
/* ------------------------------------------------------------------ */
static void process_neigh_message(struct nlmsghdr *nlh)
{
    struct ndmsg *ndm = (struct ndmsg *)NLMSG_DATA(nlh);
    if (ndm->ndm_family != AF_INET)
        return;

    int attrlen = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*ndm));
    struct rtattr *rta =
        (struct rtattr *)((char *)ndm + NLMSG_ALIGN(sizeof(*ndm)));

    char ipstr[INET_ADDRSTRLEN] = {};
    unsigned char mac[6] = {};
    bool found_ip  = false;
    bool found_mac = false;

    for (; RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
        if (rta->rta_type == NDA_DST) {
            inet_ntop(AF_INET, RTA_DATA(rta), ipstr, sizeof(ipstr));
            found_ip = true;
        } else if (rta->rta_type == NDA_LLADDR && RTA_PAYLOAD(rta) >= 6) {
            memcpy(mac, RTA_DATA(rta), 6);
            found_mac = true;
        }
    }

    if (!found_ip || !is_monitored_ip(ipstr))
        return;

    char ifname[IF_NAMESIZE] = "unknown";
    if (ndm->ndm_ifindex > 0)
        if_indextoname(ndm->ndm_ifindex, ifname);

    const char *event     = (nlh->nlmsg_type == RTM_NEWNEIGH) ? "NEIGH_UPDATE"
                                                               : "NEIGH_DELETE";
    const char *state_str = nud_state_to_string(ndm->ndm_state);

    printf("[GW-MONITOR] %s: IP=%s iface=%s state=%s", event, ipstr, ifname, state_str);
    if (found_mac)
        printf(" mac=%02x:%02x:%02x:%02x:%02x:%02x",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("\n");

    if (nlh->nlmsg_type == RTM_DELNEIGH) {
        printf("[GW-MONITOR] WARNING: monitored gateway %s removed from neighbor table\n",
               ipstr);
    } else if (!is_reachable(ndm->ndm_state)) {
        printf("[GW-MONITOR] WARNING: monitored gateway %s is NOT reachable (state=%s)\n",
               ipstr, state_str);
    }
}

/* ------------------------------------------------------------------ */
/*  Monitor loop – runs in its own thread                             */
/* ------------------------------------------------------------------ */
static void monitor_loop()
{
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        perror("[GW-MONITOR] socket");
        return;
    }

    struct sockaddr_nl addr = {};
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_NEIGH;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[GW-MONITOR] bind");
        close(sock);
        return;
    }

    printf("[GW-MONITOR] started, listening for neighbor table changes\n");

    check_initial_state();

    char buf[BUF_SIZE];
    while (g_running.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        struct timeval tv = {1, 0};

        int ret = select(sock + 1, &rfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("[GW-MONITOR] select");
            break;
        }
        if (ret == 0) continue;   /* timeout – recheck g_running */

        {   /* skip processing when no IPs are registered */
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_monitorIPs.empty()) {
                /* drain the socket so it does not pile up */
                recv(sock, buf, sizeof(buf), 0);
                continue;
            }
        }

        ssize_t rlen = recv(sock, buf, sizeof(buf), 0);
        if (rlen <= 0) {
            if (errno == EINTR) continue;
            break;
        }

        int len = static_cast<int>(rlen);
        for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
             NLMSG_OK(nlh, len);
             nlh = NLMSG_NEXT(nlh, len)) {
            if (nlh->nlmsg_type == RTM_NEWNEIGH ||
                nlh->nlmsg_type == RTM_DELNEIGH) {
                process_neigh_message(nlh);
            }
        }
    }

    close(sock);
    printf("[GW-MONITOR] stopped\n");
}

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

void registerMonitorIP(const std::string &ip)
{
    struct in_addr tmp;
    if (inet_pton(AF_INET, ip.c_str(), &tmp) != 1) {
        fprintf(stderr, "[GW-MONITOR] invalid IP address: %s\n", ip.c_str());
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (std::find(g_monitorIPs.begin(), g_monitorIPs.end(), ip) == g_monitorIPs.end()) {
        g_monitorIPs.push_back(ip);
        printf("[GW-MONITOR] registered IP for monitoring: %s\n", ip.c_str());
    }
}

void startGwMonitor()
{
    if (g_running.load()) {
        printf("[GW-MONITOR] already running\n");
        return;
    }
    g_running.store(true);
    g_monitorThread = std::thread(monitor_loop);
}

void stopGatewayMonitor()
{
    if (!g_running.load()) {
        printf("[GW-MONITOR] not running\n");
        return;
    }
    g_running.store(false);
    if (g_monitorThread.joinable())
        g_monitorThread.join();
}

/* ================================================================== */
/*  main – demonstration / CLI usage                                  */
/* ================================================================== */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <gateway-ip> [gateway-ip2 ...]\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++)
        registerMonitorIP(argv[i]);

    startGwMonitor();

    printf("Monitoring... press Enter to stop\n");
    getchar();

    stopGatewayMonitor();
    return 0;
}
