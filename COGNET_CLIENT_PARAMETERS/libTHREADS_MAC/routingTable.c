/*
Cognitive Network programm 
Copyright (C) 2014  Matteo Danieletto matteo.danieletto@dei.unipd.it
University of Padova, Italy +39 049 827 7778
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <net/route.h>

#include <arpa/inet.h>


//CHECK PRAGMA
#pragma pack(2)

// Structure for sending the request
typedef struct 
{
struct nlmsghdr nlMsgHdr;
struct rtmsg rtMsg;
char buf[1024];
}route_request;

// Structure for storing routes
struct RouteInfo
{
unsigned long dstAddr;
unsigned long gateWay;
unsigned long srcAddr;
char ifName[IF_NAMESIZE];
};

#ifdef __ANDROID__
struct ifreq {
    char    ifr_name[IFNAMSIZ];/* Interface name */
    union {
            struct sockaddrifr_addr;
            struct sockaddrifr_dstaddr;
            struct sockaddrifr_broadaddr;
            struct sockaddrifr_netmask;
            struct sockaddrifr_hwaddr;
            short   ifr_flags;
            int     ifr_ifindex;
            int     ifr_metric;
            int     ifr_mtu;
            struct ifmapifr_map;
            char    ifr_slave[IFNAMSIZ];
            char    ifr_newname[IFNAMSIZ];
            char *  ifr_data;
    };
};
#endif
// Function for accessing interface name
/*--------------------------------------------------------------
* To get the name of the interface provided the interface index
*--------------------------------------------------------------*/
int ifname(int if_index , char *ifNameVar)
{
	int fd;
	// struct sockaddr_in *sin;
	struct ifreq ifr;


	fd = socket(AF_INET, SOCK_DGRAM, 0); 
	if(fd == -1)
	{
		perror("socket");
		exit(1);
	}

	ifr.ifr_ifindex = if_index;

	if(ioctl(fd, SIOCGIFNAME, &ifr, sizeof(ifr)))
	{
		perror("ioctl");
		printf("ERROR ROUTING FIND INTERFACE\n");
		// exit(1);
	}

	strcpy(ifNameVar, ifr.ifr_name);

	close(fd);
	return 0;
}



int reportRoutinTable( int route_sock, FILE * fpRouting , struct timespec tv , char * ifNameVar)
{
	int i,j;
	route_request *request = (route_request *)malloc(sizeof(route_request));
	int retValue = -1,nbytes = 0,reply_len = 0;
	char reply_ptr[1024];
	ssize_t counter = 1024;
	// int count 		= 0;
	struct rtmsg *rtp;
	struct rtattr *rtap;
	struct nlmsghdr *nlp;
	int rtl;
	struct RouteInfo route[24];
	char* buf = reply_ptr;
	unsigned long bufsize ;
	char * outputRouting ;
    outputRouting = (char *)malloc(sizeof(char) * 1024);

	   
	bzero(request,sizeof(route_request));

	// Fill in the NETLINK header
	request->nlMsgHdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	request->nlMsgHdr.nlmsg_type = RTM_GETROUTE;
	request->nlMsgHdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

	// set the routing message header
	request->rtMsg.rtm_family = AF_INET;
	request->rtMsg.rtm_table = 254; 

	// Send routing request
	if ((retValue = send(route_sock, request, sizeof(route_request), 0)) < 0)
	{
		perror("send");
		exit(1);
	}

	for(;;)
	{
		if( counter < sizeof( struct nlmsghdr))
		{
			printf("Routing table is bigger than 1024\n");
			exit(1);
		}

		nbytes = recv(route_sock, &reply_ptr[reply_len], counter, 0);

		if(nbytes < 0 )
		{
			printf("Error in recv\n");
			break;
		}
	
		if(nbytes == 0)
			printf("EOF in netlink\n");

		nlp = (struct nlmsghdr*)(&reply_ptr[reply_len]);

		if (nlp->nlmsg_type == NLMSG_DONE)
		{
	// All data has been received.
	// Truncate the reply to exclude this message,
	// i.e. do not increase reply_len.
		break;
		}

		if (nlp->nlmsg_type == NLMSG_ERROR)
		{
		printf("Error in msg\n");
		exit(1);
		}

		reply_len += nbytes;
		counter -= nbytes;
	}

	/*======================================================*/
	bufsize = reply_len;
	// string to hold content of the route
	// table (i.e. one entry)
	// unsigned int flags;

	// outer loop: loops thru all the NETLINK
	// headers that also include the route entry
	// header
	nlp = (struct nlmsghdr *) buf;

	for(i= -1; NLMSG_OK(nlp, bufsize); nlp=NLMSG_NEXT(nlp, bufsize))
	{
	// 	// get route entry header
		rtp = (struct rtmsg *) NLMSG_DATA(nlp);
		// we are only concerned about the
		// tableId route table
		if(rtp->rtm_table != 254)
			continue;
		i++;
		// init all the strings
		bzero(&route[i], sizeof(struct RouteInfo));
		// flags = rtp->rtm_flags;
		// route[i].proto = rtp->rtm_protocol;

	// // inner loop: loop thru all the attributes of
	// // one route entry
		rtap = (struct rtattr *) RTM_RTA(rtp);
		rtl = RTM_PAYLOAD(nlp);
		for( ; RTA_OK(rtap, rtl); rtap = RTA_NEXT(rtap, rtl))
		{
			switch(rtap->rta_type)
			{
			// destination IPv4 address
				case RTA_DST:
					// count = 32 - rtp->rtm_dst_len;
					route[i].dstAddr = *(unsigned long *) RTA_DATA(rtap);
				break;
				case RTA_GATEWAY:
					route[i].gateWay = *(unsigned long *) RTA_DATA(rtap);
					//printf("gw:%s\t",inet_ntoa(route[i].gateWay));
				break;
				// // unique ID associated with the network
				// // interface
				case RTA_OIF:
					ifname(*((int *) RTA_DATA(rtap)),route[i].ifName);
					//printf( "ifname %s\n", route[i].ifName);
				break;
				default:
				break;
			}

		}
	} 
	

	
	for( j = 0; j<= i; j++)
	{

		if(strcmp(route[j].ifName, ifNameVar)==0)
		{
                   
			char ipbuf[INET_ADDRSTRLEN];	   
			char ipbuf2[INET_ADDRSTRLEN];	   
			inet_ntop(AF_INET, &route[j].dstAddr, ipbuf, INET_ADDRSTRLEN);
			inet_ntop(AF_INET, &route[j].gateWay, ipbuf2, INET_ADDRSTRLEN);
			sprintf(outputRouting, "%lu.%lu:%s:%s", (unsigned long) tv.tv_sec,(unsigned long) tv.tv_nsec, ipbuf ,ipbuf2);
			fprintf(fpRouting, "%s\n" ,outputRouting);
		}
	}
    fflush(fpRouting);
	// close(route_sock);
	return 0;
}
