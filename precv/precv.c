#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>   //Provides declarations for icmp header
#include <netinet/udp.h>   //Provides declarations for udp header
#include <netinet/tcp.h>   //Provides declarations for tcp header
#include <netinet/ip.h>    //Provides declarations for ip header
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <byteswap.h>
#include <time.h>

#define SIZE_ETHERNET 14
#define SIZE_IP 20
#define SIZE_ICMP_HEADER 16
#define ID_OFFSET 4
#define SEQ_OFFSET 6
#define PACKET_SIZE 4096

void send_reply();

char icmp_reply[PACKET_SIZE];
int pid;
int sockfd;
int remote_ip=-1;
uint16_t *icmpid;
uint16_t *seq;
struct in_addr src_ip;
int count = 0;

struct sniff_ip
{
        u_char ip_vhl;
        u_char ip_tos;
        u_short ip_len;
        u_short ip_id;
        u_short ip_off;
        #define IP_RF 0x8000
        #define IP_DF 0x4000
        #define IP_MF 0x2000
        #define IP_OFFMASK 0x1fff
        u_char ip_ttl;
         u_char ip_p;
        u_short ip_sum;
        struct in_addr ip_src,ip_dst;
};




unsigned short cal_chksum(unsigned short *addr,int len){       int nleft=len;
        int sum=0;
        unsigned short *w=addr;
        unsigned short answer=0;
		
/*把ICMP报头二进制数据以2字节为单位累加起来*/
        while(nleft>1)
        {       sum+=*w++;
                nleft-=2;
        }
		/*若ICMP报头为奇数个字节，会剩下最后一字节。把最后一个字节视为一个2字节数据的高字节，这个2字节数据的低字节为0，继续累加*/
        if( nleft==1)
        {       *(unsigned char *)(&answer)=*(unsigned char *)w;
                sum+=answer;
        }
        sum=(sum>>16)+(sum&0xffff);
        sum+=(sum>>16);
        answer=~sum;
        return answer;
}

void got_packet(u_char *args, const struct pcap_pkthdr *header,
	    const u_char *packet){
	printf("ICMP_RECV!\n");
	const struct sniff_ip *ip;

	//printf("PKT size = %d\n",sizeof(packet));
	
	ip = (struct sniff_ip*)(packet + SIZE_ETHERNET);
	
	icmpid = (uint16_t*)(packet + SIZE_ETHERNET + SIZE_IP + ID_OFFSET);
	seq = (uint16_t*)(packet + SIZE_ETHERNET + SIZE_IP + SEQ_OFFSET);
	//*seq = __bswap_16(*seq);
	src_ip = ip->ip_src;
	
	

	//memset(icmp_reply,0,sizeof(icmp_reply));

	struct icmp *ptr_icmp;
	struct timeval *t_recv;
	ptr_icmp = (struct icmp*)icmp_reply;
	ptr_icmp->icmp_type = ICMP_ECHOREPLY;
	ptr_icmp->icmp_code = 0;
	ptr_icmp->icmp_cksum = 0;
	ptr_icmp->icmp_seq = *seq;
	ptr_icmp->icmp_id =*icmpid;
	
	ptr_icmp->icmp_cksum=cal_chksum((unsigned short*)ptr_icmp,64);

	memcpy((icmp_reply+SIZE_ICMP_HEADER),(packet+50),48);

	int recvbuf_size = 50*1024;

	struct sockaddr_in dst;
	bzero(&dst,sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_addr = src_ip;
	setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&recvbuf_size,sizeof(recvbuf_size));
	t_recv = (struct timeval*)ptr_icmp->icmp_data;
	gettimeofday(t_recv,NULL);
	//printf("%ld\n%ld\n",t_recv->tv_sec,t_recv->tv_usec);
	int err = sendto(sockfd,ptr_icmp,64,0,(struct sockaddr *)&dst,sizeof(dst));
	if(err < 0 ){
		printf("Sendto failure, Code %d: %s\n",err,strerror(err));
	}
	printf("FROM: %s, SEQ: %d\n",inet_ntoa(src_ip),__bswap_16(*seq));
}


int main(int argc, char *argv[]){

    char *dev, errbuf[PCAP_ERRBUF_SIZE];
    char ip[20];
    struct ifreq ifr;
    pcap_t *handle;
    if(argc<2){
		dev = pcap_lookupdev(errbuf);
	}else{
		dev = argv[1];
	}
	if (dev == NULL) {
		fprintf(stderr, "Couldn't find default device: %s\n", errbuf);
		return(2);
	}
	printf("Device: %s\n", dev);

	handle = pcap_open_live(dev, BUFSIZ, 0, 500, errbuf);
    if (handle == NULL) {
		 fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
		 return(2);
	}
	if (pcap_datalink(handle) != DLT_EN10MB) {
		fprintf(stderr, "Device %s doesn't provide Ethernet headers - not supported\n", dev);
		return(2);
	}

	struct bpf_program fp;		/* The compiled filter expression */
	char filter_exp[100] = "(icmp) && (dst ";	/* The filter expression */
	bpf_u_int32 mask;		/* The netmask of our sniffing device */
	bpf_u_int32 net;		/* The IP of our sniffing device */
	
	sockfd = socket(AF_INET, SOCK_RAW, 1);
	if(sockfd<0){
		printf("socket create failure\n");
	}
        
        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name,dev,IFNAMSIZ-1);
        ioctl(sockfd,SIOCGIFADDR,&ifr);
        strcpy(ip,inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));
        printf("IP:%s\n",ip);
        strcat(filter_exp,ip);
        strcat(filter_exp,")");
        printf("Filter=%s\n",filter_exp);

        if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
		 fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
		 return(2);
	}

	if (pcap_setfilter(handle, &fp) == -1) {
		 fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
		 return(2);
	}

	pcap_loop(handle, -1, got_packet, NULL);





    return 0;
}
