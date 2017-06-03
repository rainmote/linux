#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef SIOCETHTOOL
#define SIOCETHTOOL             0x8946
#endif

#define ETHTOOL_GSET            0x00000001
#define ETHTOOL_GLINKSETTINGS   0x0000004c

#ifndef IFNAMSIZE
#define IFNAMSIZE               16
#endif

#ifndef SCHAR_MAX
#define SCHAR_MAX               127
#endif

#ifndef IP_ADDR_LEN
#define IP_ADDR_LEN             16
#endif

struct ethtool_cmd {
    __u32   cmd;
    __u32   supported;
    __u32   advertising;
    __u16   speed;
    __u8    duplex;
    __u8    port;
    __u8    phy_address;
    __u8    transceiver;
    __u8    autoneg;
    __u8    mdio_support;
    __u32   maxtxpkt;
    __u32   maxrxpkt;
    __u16   speed_hi;
    __u8    eth_tp_mdix;
    __u8    eth_tp_mdix_ctrl;
    __u32   lp_advertising;
    __u32   reserved[2];
};

struct ethtool_link_settings {
    __u32 cmd;
    __u32 speed;
    __u8 duplex;
    __u8 port;
    __u8 phy_address;
    __u8 autoneg;
    __u8 mdio_support;
    __u8 eth_tp_mdix;
    __u8 eth_tp_mdix_ctrl;
    __s8 link_mode_masks_nwords;
    __u32 reserved[8];
    __u32 link_mode_masks[0];
    /* layout of link_mode_masks fields:
     * * __u32 map_supported[link_mode_masks_nwords];
     * * __u32 map_advertising[link_mode_masks_nwords];
     * * __u32 map_lp_advertising[link_mode_masks_nwords];
     * */
};

#define ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32 (SCHAR_MAX)

struct ethtool_ecmd
{
    struct ethtool_link_settings* req;
    __u32 link_mode_data[3 * ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32];
};

int do_ioctl_glinksettings(int sockfd,
						   struct ifreq *ifr,
                           struct ethtool_link_settings *settings)
{
    struct ethtool_ecmd ecmd;
    memset(&ecmd, 0, sizeof(struct ethtool_ecmd));
    ecmd.req = settings;
    ecmd.req->cmd = ETHTOOL_GLINKSETTINGS;

    ifr->ifr_data = (caddr_t)&ecmd;
    int ret = ioctl(sockfd, SIOCETHTOOL, ifr);
    if (ret < 0)
    {
        printf("ioctl error: %s\n", strerror(errno));
        return -1;
    }
    printf("Speed: %d", ecmd.req->speed);
}

int do_ioctl_gset(int sockfd,
				  struct ifreq *ifr,
                  struct ethtool_link_settings *settings)
{
    struct ethtool_cmd ecmd;
    memset(&ecmd, 0, sizeof(struct ethtool_cmd));
    ecmd.cmd = ETHTOOL_GSET;

    ifr->ifr_data = (caddr_t)&ecmd;
    int ret = ioctl(sockfd, SIOCETHTOOL, ifr);
    if (ret < 0)
    {
        printf("ioctl error: %s\n", strerror(errno));
        return -1;
    }
    printf("Speed: %d", ecmd.speed);
    /* remember that ETHTOOL_GSET was used */
    settings->cmd = ETHTOOL_GSET;
    settings->link_mode_masks_nwords = 1;
    settings->speed = (ecmd.speed_hi << 16) | ecmd.speed;
    settings->duplex = ecmd.duplex;
    settings->port = ecmd.port;
    settings->phy_address = ecmd.phy_address;
    settings->autoneg = ecmd.autoneg;
    settings->mdio_support = ecmd.mdio_support;
    /* ignored (fully deprecated): maxrxpkt, maxtxpkt */
    settings->eth_tp_mdix = ecmd.eth_tp_mdix;
    settings->eth_tp_mdix_ctrl = ecmd.eth_tp_mdix_ctrl;
}

int GetDeviceBandwidth(int sockfd, char *dev)
{
    struct ifreq ifr;
    struct ethtool_link_settings settings;
    strncpy(ifr.ifr_name, dev, IFNAMSIZE - 1);

    do_ioctl_gset(sockfd, &ifr, &settings);
    do_ioctl_glinksettings(sockfd, &ifr, &settings);

    return 0;
}

int GetDeviceAddr(int sockfd, char *dev, char *ip)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, dev, IFNAMSIZE - 1);
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0)
    {
        printf("ioctl error: %s\n", strerror(errno));
        return -1;
    }
    struct sockaddr_in *addr = (struct sockaddr_in *)&(ifr.ifr_addr);
    char *sin_addr = inet_ntoa(addr->sin_addr);
    uint len = strlen(sin_addr);
    strncpy(ip, sin_addr, len > IP_ADDR_LEN ? IP_ADDR_LEN : len);
    return 0;
}

#define BUF_SIZE 512
int main()
{
    int sockfd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        printf("socket error");
        exit(1);
    }

    unsigned char buf[BUF_SIZE];
    struct ifconf ifconf;
    ifconf.ifc_len = BUF_SIZE;
    ifconf.ifc_buf = buf;
    if (ioctl(sockfd, SIOCGIFCONF, &ifconf) < 0)
    {
        printf("ioctl SIOCGIFCONF error: %s\n", strerror(errno));
        exit(1);
    }

    struct ifreq *ifr;
    ifr = (struct ifreq *)buf;

    int len = ifconf.ifc_len / sizeof(struct ifreq);
    for (int i = 0; i < len; i++)
    {
        char ip[IP_ADDR_LEN] = {0};
        GetDeviceAddr(sockfd, ifr->ifr_name, ip);
        printf("dev: %-12s\taddr: %s\n", ifr->ifr_name, ip);

        GetDeviceBandwidth(sockfd, ifr->ifr_name);

        ifr++;
    }

    return 0;
}
#undef BUF_SIZE
