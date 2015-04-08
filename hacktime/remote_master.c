//using namespace std


//#include <map>
#include <pthread.h>
#include "remote_common.h"


//using namespace std;

/*
*
*Time dilation Factor data format
*
*/
struct tdf_data{
    int tdf;
    int new_tdf;
};
//int tdf = 1;
//static int new_tdf = 1;

/*
*
*Fake systemcalls to controll remote VM nodes
*
*/
int fake_gettimeofday(struct timeval *tv, void *tv1, struct tdf_data *data);
int fake_time(time_t *time);
//int fake_ftime(struct timeb *tb);
int fake_clock_gettime(clockid_t clk_id, struct timespec *p);

/*
*
*Socket map for multiple connections
*
*/
//map<unsigned int, int> sockmap = map<unsigned int, int>();

//map<struct in_addr, int> ipmap;

/*
*
*Utilities
*
*/

unsigned int getDeviceIP(char *dev_name){

    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM,0);
    ifr.ifr_addr.sa_family = AF_INET;

    strncpy(ifr.ifr_name, dev_name, IFNAMSIZ-1);

    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    printf("Local IP: %s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

    struct in_addr ip = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
    
    return (unsigned int) ip.s_addr;
}

void timeval_diff(struct timeval *tv1, struct timeval *tv2){
    tv1->tv_sec-=tv2->tv_sec;
    tv1->tv_usec-=tv2->tv_usec;
}
void timeval_normalize(struct timeval *tv){
    while(tv->tv_usec>=1000000){
        tv->tv_sec++;
        tv->tv_usec-=10000000;
    }

    while(tv->tv_usec<0){
        tv->tv_sec--;
        tv->tv_usec+=1000000;
    }
}



/*
*
*Global variables for fake_gettimeoafday
*
*/
struct timeval *tdf_start_time=NULL;
struct timeval *tdf_ref_time=NULL;
struct timeval *tdf_current_time=NULL;

void update_tdf(struct tdf_data *data){

    if(data->tdf==data->new_tdf){
        return;
    }
  
    memcpy(tdf_start_time, tdf_current_time, sizeof(struct timeval));
    gettimeofday(tdf_ref_time,NULL);
    data->tdf = data->new_tdf;
}

/*
*
*Fake time syscalls implementaitons
*
*/
int fake_gettimeofday(struct timeval *tv, void *tvi, struct tdf_data *data){

    struct timeval realtime;

    if(tdf_start_time==NULL){//If tdf_start_time not set, means first call
        printf("Init fake time\n");
        tdf_start_time = (struct timeval*)malloc(sizeof(struct timeval));
        tdf_ref_time = (struct timeval*)malloc(sizeof(struct timeval));
        tdf_current_time = (struct timeval*)malloc(sizeof(struct timeval));
        gettimeofday(tdf_start_time,NULL);
        gettimeofday(tdf_ref_time,NULL);
        gettimeofday(tdf_current_time,NULL);
    }
    update_tdf(data);

    gettimeofday(&realtime,(struct timezone*)tvi);
    timeval_diff(&realtime,tdf_ref_time);    

    tv->tv_sec = tdf_start_time->tv_sec + realtime.tv_sec/data->tdf;
    tv->tv_usec = tdf_start_time->tv_usec + realtime.tv_usec/data->tdf; 
    timeval_normalize(tv);
    memcpy(tdf_current_time, tv, sizeof(struct timeval));

    return 1;

}

/*
*
*Other fake syscalls: to be implemented
*
*/

/*
*
*Global variables for network related functions
*
*/
unsigned int local_ip;

/*
*
*Request process callback function
*
*/
void process_request(u_char *args, const struct pcap_pkthdr *header, const u_char *packet){
    struct in_addr src_ip = ((struct sniff_ip*)(packet+SIZE_ETHERNET))->ip_src; //Get Request IP
    struct in_addr dst_ip = ((struct sniff_ip*)(packet+SIZE_ETHERNET))->ip_dst;
    printf("Request Received from %s\n",inet_ntoa(src_ip));

/*Receive request broadcast, reply to notify our IP address*/
    int client_sock;
    struct sockaddr_in client_addr;

    if((client_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        printf("Reply socket open failed: %s\n",strerror(errno));
    }
  
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr = src_ip;
    client_addr.sin_port = htons(REQ_PORT-1);
    

    struct in_addr ip;
    ip.s_addr = local_ip; 
    char buff[REQUEST_LENGTH];
    *(int*)buff = REPLY_MAGIC_NUM;
    memcpy(buff+4, &ip, sizeof(struct in_addr));
    //printf("IP info to send = %s\n",inet_ntoa(*(struct in_addr*)buff));

    if(sendto(client_sock, buff, REQUEST_LENGTH, 0, (struct sockaddr*)&client_addr, sizeof(client_addr))==-1){
        printf("Send reply failed: %s\n",strerror(errno));
    }

    close(client_sock);

}
/*
*
*Process remote time request in a new thread
*
*/
void* process_function_request(void *arg){
    int server_sock;
    struct sockaddr_in dest;
    char buffer[REQUEST_LENGTH];
    if((server_sock = socket(AF_INET,SOCK_STREAM,0)) == -1){
        printf("Server Socket open failed: %s\n",strerror(errno));
    }
    bzero(&dest, sizeof(struct sockaddr_in));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(COMM_PORT);
    dest.sin_addr.s_addr = INADDR_ANY;
    int opt = 1;
    if(setsockopt(server_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt))==-1){
        printf("Set server socket failed: %s\n",strerror(errno));
    }

    if(bind(server_sock, (struct sockaddr*)&dest,sizeof(dest)) == -1){
        printf("Server bind failed: %s\n",strerror(errno));
    }
    if(listen(server_sock,MAX_CONNECTIONS)==-1){
        printf("Listen socket failed: %s\n",strerror(errno));
        return NULL;
    }

    fd_set readfds; //selector for connection sockets
    int client_sock_array[MAX_CONNECTIONS];
    bzero(client_sock_array,MAX_CONNECTIONS*sizeof(int));
    int client_sock = -1;
    int max_sd = -1;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(struct sockaddr);
 
    while(1){

        FD_ZERO(&readfds); //Reset FD_SET

        FD_SET(server_sock, &readfds); //Server socket must be in FD_SET
        max_sd = server_sock;
        for(int i=0;i<MAX_CONNECTIONS; i++){
            int sd = client_sock_array[i];
            if(sd>0){ //If there's valid socket in the array, add it to FD_SET
                FD_SET(sd, &readfds);
            }
            if(sd>server_sock){ //Maintain maximum socket number
                max_sd = sd;
            }
        }
        int activity = select(max_sd+1, &readfds, NULL, NULL, NULL);
        if((activity<0) && (errno!=EINTR)){
            printf("Select active socket error: %s\n",strerror(errno));
        }
        if(FD_ISSET(server_sock,&readfds)){
            client_sock = accept(server_sock,(struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
            if(client_sock==-1){
                printf("Accept connection failed: %s\n",strerror(errno));
            }
            printf("New connection from %s\n",inet_ntoa(client_addr.sin_addr));
            for(int i=0;i<MAX_CONNECTIONS; i++){
                if(client_sock_array[i]==0){
                    client_sock_array[i] = client_sock;
                    break;
                }
            }
        }
        
        for(int i=0; i<MAX_CONNECTIONS; i++){
            if(FD_ISSET(client_sock_array[i],&readfds)){
                int rlen; //length of read data
                if((rlen = read( client_sock_array[i], buffer, REQUEST_LENGTH))==0){
                    close(client_sock_array[i]);
                    client_sock_array[i] = 0;
                }else{
                    int command = *(int*)buffer;
                    if(command == TYPE_GETTIMEOFDAY){
                        struct timeval tv;
                        fake_gettimeofday(&tv, NULL, (struct tdf_data*)arg);
                        memcpy(buffer+4, &tv, sizeof(struct timeval));
                    }else if(command == TYPE_TDF){
                        memcpy(buffer+4, &(((struct tdf_data*)arg)->tdf), sizeof(int));
                    }
                    send(client_sock_array[i], buffer, REQUEST_LENGTH,0);
                }
            }
        }

    }
    close(server_sock);

    return NULL;
}

void* change_tdf(void* arg){

    struct tdf_data *data = (struct tdf_data*)arg;

    while(1){
        scanf("%d",&(data->new_tdf));
    }

    return NULL;
}

/*
*
*Entry point -- Main loop
*
*/

int main(int argc, char *argv[]){

    char *dev, errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *pcap_handle;
    struct bpf_program fp;
    char filter[] = "(port 64645) && (dst host 255.255.255.255)";  

    if(argc>=2){
        dev = argv[1];
    }else{
        dev = pcap_lookupdev(errbuf);
    }
    if(dev == NULL){
        fprintf(stderr, "Can't fetch device: %s\n",errbuf);
        return -1;
    }
    printf("Listen Device: %s\n",dev);

    pcap_handle = pcap_open_live(dev, BUFSIZ, 1, 1, errbuf);
    if(pcap_handle == NULL){
         fprintf(stderr,"Can't open device: %s",errbuf);
        return -2;
    }
    local_ip = getDeviceIP(dev);
    if(pcap_compile(pcap_handle, &fp, filter, 0, local_ip) == -1){
         fprintf(stderr, "Incorrect Filter: %s\n",pcap_geterr(pcap_handle));
         return -3;
    }
    if(pcap_setfilter(pcap_handle, &fp) == -1){
        fprintf(stderr, "Can't set filter: %s\n",pcap_geterr(pcap_handle));
    }
 
    struct tdf_data tdf_info;
    tdf_info.tdf = 1;
    tdf_info.new_tdf=1;

    pthread_t tid_request, tid_change_tdf;
    pthread_create(&tid_request, NULL,process_function_request, &tdf_info);
    pthread_create(&tid_change_tdf, NULL, change_tdf, &tdf_info);


    while(1){
        pcap_dispatch(pcap_handle, -1, process_request,NULL);
    }

    


    return 0;

}
