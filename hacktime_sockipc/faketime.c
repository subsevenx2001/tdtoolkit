#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <dlfcn.h>

#include "remote_common.h"

//Function pointers to override
static int (*orig_gettimeofday)(struct timeval *tv,struct timezone *tz) = NULL;
static unsigned int (*orig_sleep)(unsigned int seconds) = NULL;
static int (*orig_usleep)(unsigned int micro_seconds) = NULL;
static clock_t (*orig_clock)(void) = NULL;

//Global variables for socket
int server_sock = -1; 
struct sockaddr_un server_addr;
unsigned pid; //PID of this procces for identification

//Global variables for override functions
clock_t zero_clock;
clock_t start_clock;
int current_tdf = 1;

//Communication Functions
int find_remote_master(){
    //Init connection to remote master    
    bzero(&server_addr, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, SOCK_FILE_ADDR);
    //server_addr.sin_addr = *(struct in_addr*)(packet+COMMAND_OFFSET); 
    //server_addr.sin_port = htons(COMM_PORT);
    //printf("Get server ip: %s\n",inet_ntoa(server_addr.sin_addr));
    if((server_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
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

static void init_clock(void){

    if(server_sock==-1){
       printf("Init Socket\n");
       find_remote_master();
    }
    printf("overriding clock\n");
    orig_clock = dlsym(RTLD_NEXT, "clock");
    zero_clock = orig_clock();
    start_clock = orig_clock();

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

clock_t clock(void){

    char msg[REQUEST_LENGTH];
    if(orig_clock == NULL){
        init_clock();
    }
    if(send_request(TYPE_TDF,msg) == -1){
        return orig_clock();
    }
    int tdf = *(int*)(msg + COMMAND_OFFSET);
    if(current_tdf != tdf){
        zero_clock = orig_clock();    
        current_tdf = tdf;
    }

    return zero_clock + (orig_clock()-zero_clock)/tdf;



}
