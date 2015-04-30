#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common.h"



int mqid;
MSG_BUF msg={0,0};

void terminate_handler(int signo){

    msgctl(mqid,IPC_RMID,NULL);
    exit(0);

}

int main(){

    int tmp=1;
    key_t mqkey;
    
    mqkey = ftok(TDT_FNAME,TDT_ID);
    if(mqkey==-1){
        printf("ftok error\n");
	//return -1;
    }

    mqid = msgget(mqkey, IPC_CREAT | IPC_EXCL | 0666);
    if(mqid == -1){
	printf("msgget error: %s\n",strerror(errno));
	//return -1;
    }

    printf("mqid = %d\n",mqid);
    
    signal(SIGINT,terminate_handler);
    signal(SIGTERM,terminate_handler);

    FILE *fp;
    while(1){
        scanf("%d",&tmp);
        //printf("TEST1\n");
        if(tmp<=0){
            break;
        }
        msg.tdf = tmp;
        //printf("TEST\n");
	msg.mtype = CONTROLLER;
        msgsnd(mqid,&msg,sizeof(int),0);
    }

    exit(0);
}
