#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <dlfcn.h>

#include "remote_common.h"

//Function pointers to override
static int (*orig_gettimeofday)(struct timeval *tv,struct timezone *tz) = NULL;

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



//Fake System calls
int gettimeofday(struct timeval *tv, struct timezone *tz){

    if(orig_gettimeofday == NULL){
        init_gtod();
    }
    msg.type = MSG_HOOK;
    //printf("msg key,id = %d,%d\n",mqkey,mqid);
    *(int*)(msg.content) = TYPE_GETTIMEOFDAY;
    if(msgsnd(mqid, &msg, REQUEST_LENGTH, 0) == -1){
        printf("Send IPC Message failed: %s\n",strerror(errno));
        return orig_gettimeofday(tv,tz);
    }
    if(msgrcv(mqid, &msg, REQUEST_LENGTH, MSG_CONTROLLER, 0) == -1){
        printf("Receive IPC Message failed: %s\n",strerror(errno));
        return orig_gettimeofday(tv,tz);
    }
    memcpy(tv, (msg.content+4), sizeof(struct timeval));
    orig_gettimeofday(NULL,tz);
    //printf("%ld %ld\n",tv->tv_sec, tv->tv_usec);    

    return 1;
}
