#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <dlfcn.h>

#include "remote_common.h"

//Function pointers to override
static int (*orig_gettimeofday)(struct timeval *tv,struct timezone *tz) = NULL;
static unsigned int (*orig_sleep)(unsigned int seconds) = NULL;
static int (*orig_usleep)(unsigned int micro_seconds) = NULL;

//Global variables for IPC
int mqid = -1;
key_t mqkey = -1;
struct ipc_msg msg;

//Initialization functions
static void init_ipc(void){
    mqkey = ftok(MSG_NAME, MSG_ID);
    if(mqkey == -1){
        printf("MQKEY create error: %s\n",strerror(errno));
        return;
    }
    mqid = msgget(mqkey, 0);
    if(mqid == -1){
        printf("MQID create error: %s\n",strerror(errno));
        return;
    }
}
static void init_gtod(void){

    if(mqkey==-1){
        printf("Init IPC\n");
        init_ipc();
    }
    printf("overriding time\n");
    orig_gettimeofday = dlsym(RTLD_NEXT,"gettimeofday");
}
static void init_sleep(void){

    if(mqkey==-1){
        printf("Init IPC\n");
        init_ipc();
    }
    printf("overriding sleep\n");
    orig_sleep = dlsym(RTLD_NEXT, "sleep");
    
}
static void init_usleep(void){

    if(mqkey==-1){
        printf("Init IPC\n");
        init_ipc();
    }
    printf("overriding usleep\n");
    orig_sleep = dlsym(RTLD_NEXT, "usleep");
    
}


static int send_request(int type){

    msg.type = MSG_HOOK;
    *(int*)(msg.content) = type;
    if(msgsnd(mqid, &msg, REQUEST_LENGTH, 0) == -1){
        printf("Send IPC Message failed: %s\n",strerror(errno));
        return -1;
    }
    if(msgrcv(mqid, &msg, REQUEST_LENGTH, MSG_CONTROLLER, 0) == -1){
        printf("Receive IPC Message failed: %s\n",strerror(errno));
        return -1;
    }

    return 0;

}

//Fake System calls
int gettimeofday(struct timeval *tv, struct timezone *tz){

    if(orig_gettimeofday == NULL){
        init_gtod();
    }
    if(send_request(TYPE_GETTIMEOFDAY) == -1){
        return orig_gettimeofday(tv,tz);
    }
    memcpy(tv, (msg.content+4), sizeof(struct timeval));
    orig_gettimeofday(NULL,tz);    

    return 1;
}

unsigned int sleep(unsigned int seconds){

    if(orig_sleep == NULL){
        init_sleep();
    }
    if(send_request(TYPE_TDF) == -1){
        return orig_sleep(seconds);
    } 
    int tdf = *(int*)(msg.content+4);

    return orig_sleep(tdf*seconds);

}

int usleep(unsigned int micro_seconds){

    if(orig_usleep == NULL){
        init_usleep();
    }
    if(send_request(TYPE_TDF) == -1){      
        return orig_usleep(micro_seconds);
    }
    int tdf = *(int*)(msg.content+4);
    return orig_usleep(tdf*micro_seconds);

}
