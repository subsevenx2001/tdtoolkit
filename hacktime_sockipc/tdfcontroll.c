#include "remote_common.h"

key_t mqkey=-1;
int mqid=-1;
void terminate_ipc(int signo){
    msgctl(mqid, IPC_RMID, NULL);
    exit(0);
}


int main(){

    //Init hook IPC
    struct ipc_msg msg; 
    char ipc_fname[] = MSG_NAME;
    if((mqkey = ftok(ipc_fname, MSG_ID)) == -1){
        printf("MQKEY create error: %s\n",strerror(errno)); 
        return -1;
    }
    if((mqid = msgget(mqkey, IPC_CREAT | IPC_EXCL | 0666))==-1){
        printf("Can't create IPC: %s\n",strerror(errno));
        return -1;
    }
    signal(SIGINT, terminate_ipc);
    signal(SIGTERM, terminate_ipc);
    //Init remote discovery
    int bcast_sock = -1;
    int reply_sock = -1;
    
    if((bcast_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        printf("Sock create failed\n");
        return -1;
    }
    reply_sock = socket(AF_INET,SOCK_DGRAM,0);
    if(reply_sock == -1){
        printf("Reply Socket failed: %s\n",strerror(errno));
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
    }

    int nlen = sizeof(addrto);
    int nlen2 = sizeof(addrfrom);

    char packet[REQUEST_LENGTH];
    *((int*)packet) = TYPE_GREETING; 
        int ret = sendto(bcast_sock, packet, sizeof(packet), 0, (struct sockaddr*)&addrto, nlen);
    if(ret<0){
        printf("Send failed:  %s\n",strerror(errno));
    }
    if(recvfrom(reply_sock, packet, REQUEST_LENGTH, 0,(struct sockaddr*)&addrfrom, &nlen2) == -1){
        printf("Receive Error: %s\n",strerror(errno));
    }
    close(bcast_sock);
    close(reply_sock);
    //Find Remote Master End
    
    //Init connection to remote master 
    int server_sock = -1; 
    struct sockaddr_in server_addr;
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
   
    bzero(packet, REQUEST_LENGTH); 
    while(1){
        //Receive request from hook
        int request_type;
        //msg.type = MSG_CONTROLLER;
        if(msgrcv(mqid, &msg, REQUEST_LENGTH, MSG_HOOK,0) == -1){
            printf("Receive IPC message form HOOK failed: %s\n", strerror(errno));
        }
        request_type = *(int*)(msg.content);
        *(int*)packet = request_type;

        if(send(server_sock, packet, REQUEST_LENGTH,0) == -1){
            printf("Send request error: %s\n",strerror(errno));
            terminate_ipc(0);
            break;
        }
        if(recv(server_sock, packet, REQUEST_LENGTH, 0)==-1){
            printf("Receive answer error: %s\n",strerror(errno));
            terminate_ipc(0);
            break;
        }
        //Tell hook the result
        struct timeval *tv_ans = (struct timeval*)(packet+COMMAND_OFFSET);
        //printf("%ld %ld\n",tv_ans->tv_sec,tv_ans->tv_usec);
        memcpy((msg.content+COMMAND_OFFSET), tv_ans, sizeof(struct timeval));
        msg.type = MSG_CONTROLLER;
        if(msgsnd(mqid, &msg, REQUEST_LENGTH, 0) == -1){//Send IPC to HOOK
            printf("Send IPC message error: %s\n",strerror(errno));
        }
        /*One time test*/
        //break; 
    }

    return 0;
}
