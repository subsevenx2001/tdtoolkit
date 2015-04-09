#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <dlfcn.h>

#include "remote_common.h"

//Function pointers to override
static int (*orig_gettimeofday)(struct timeval *tv,struct timezone *tz) = NULL;
static unsigned int (*orig_sleep)(unsigned int seconds) = NULL;
static int (*orig_usleep)(unsigned int micro_seconds) = NULL;

//Global variables for socket
int server_sock = -1; 
struct sockaddr_in server_addr;
unsigned pid; //PID of this procces for identification

//Communication Functions
int find_remote_master(){

    pid = getpid();
    printf("PID of this process is %d\n",pid);

    int bcast_sock = -1;
    int reply_sock = -1;
    
    if((bcast_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        printf("Sock create failed\n");
        return -1;
    }
    reply_sock = socket(AF_INET,SOCK_DGRAM,0);
    if(reply_sock == -1){
        printf("Reply Socket create failed: %s\n",strerror(errno));
        return -1;
    }
 

    const int opt = 1;

    int nb = 0;
    nb = setsockopt(bcast_sock, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt));
    if(nb == -1){
        printf("Sock set failed: %s\n",strerror(errno));
        return -2;
    }

    struct sockaddr_in addrto, addrfrom;
    bzero(&addrto, sizeof(struct sockaddr_in));
    bzero(&addrfrom, sizeof(struct sockaddr_in));
    addrto.sin_family = AF_INET;
    addrto.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    addrto.sin_port = htons(REQ_PORT);

    addrfrom.sin_family = AF_INET;
    addrfrom.sin_addr.s_addr = htonl(INADDR_ANY);
    addrfrom.sin_port = htons(REQ_PORT-1);
    if(bind(reply_sock,(struct sockaddr*)&addrfrom,sizeof(addrfrom)) == -1){
        printf("Bind Error: %s\n",strerror(errno));
        return -1;
    }

    unsigned int nlen = sizeof(addrto);
    unsigned int nlen2 = sizeof(addrfrom);

    char packet[REQUEST_LENGTH];
    *((int*)packet) = TYPE_GREETING; 
        int ret = sendto(bcast_sock, packet, sizeof(packet), 0, (struct sockaddr*)&addrto, nlen);
    if(ret<0){
        printf("Send failed:  %s\n",strerror(errno));
        return -1;
    }
    if(recvfrom(reply_sock, packet, REQUEST_LENGTH, 0,(struct sockaddr*)&addrfrom, (socklen_t*)&nlen2) == -1){
        printf("Receive Error: %s\n",strerror(errno));
        return -1;
    }
    close(bcast_sock);
    close(reply_sock);

    //Init connection to remote master    
    bzero(&server_addr, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = *(struct in_addr*)(packet+COMMAND_OFFSET); 
    server_addr.sin_port = htons(COMM_PORT);
    printf("Get server ip: %s\n",inet_ntoa(server_addr.sin_addr));
    if((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        printf("Open Server socket failed: %s\n",strerror(errno));
        return -1;
    }
    if(connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr))==-1){
        printf("Server connect failed: %s\n",strerror(errno));
        return -1;
    }

    return 0;
}

//Initialization functions

static void init_gtod(void){

    if(server_sock==-1){
        printf("Init Socket\n");
        find_remote_master();
    }
    printf("overriding time\n");
    orig_gettimeofday = dlsym(RTLD_NEXT,"gettimeofday");
}
static void init_sleep(void){

    if(server_sock==-1){
        printf("Init Socket\n");
        find_remote_master();
    }
    printf("overriding sleep\n");
    orig_sleep = dlsym(RTLD_NEXT, "sleep");
    
}
static void init_usleep(void){

    if(server_sock==-1){
        printf("Init Socket\n");
        find_remote_master();
    }
    printf("overriding usleep\n");
    orig_sleep = dlsym(RTLD_NEXT, "usleep");
    
}


static int send_request(int type, char *msg){

    *(int*)msg = type;
    *(unsigned int*)(msg+TYPE_OFFSET) = pid;

    if(send(server_sock, msg, REQUEST_LENGTH,0) == -1){
        printf("Send request error: %s\n",strerror(errno));
        return -1;
    }
    if(recv(server_sock, msg, REQUEST_LENGTH, 0)==-1){
        printf("Receive answer error: %s\n",strerror(errno));
        return -1;
    }

    return 0;

}

//Fake System calls
int gettimeofday(struct timeval *tv, struct timezone *tz){

    char msg[REQUEST_LENGTH];

    if(orig_gettimeofday == NULL){
        init_gtod();
    }
    if(send_request(TYPE_GETTIMEOFDAY,msg) == -1){
        return orig_gettimeofday(tv,tz);
    }
    memcpy(tv, msg+COMMAND_OFFSET,sizeof(struct timeval));
    orig_gettimeofday(NULL,tz);    

    return 1;
}

unsigned int sleep(unsigned int seconds){

    char msg[REQUEST_LENGTH];

    if(orig_sleep == NULL){
        init_sleep();
    }
    if(send_request(TYPE_TDF,msg) == -1){
        return orig_sleep(seconds);
    } 
    int tdf = *(int*)(msg+COMMAND_OFFSET);

    return orig_sleep(tdf*seconds);

}

int usleep(unsigned int micro_seconds){

    char msg[REQUEST_LENGTH];

    if(orig_usleep == NULL){
        init_usleep();
    }
    if(send_request(TYPE_TDF,msg) == -1){      
        return orig_usleep(micro_seconds);
    }
    int tdf = *(int*)(msg+COMMAND_OFFSET);
    return orig_usleep(tdf*micro_seconds);

}
