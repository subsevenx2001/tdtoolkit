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

int is_end = 0;
int server_sock_global;

void terminate_process(int signo){
    close(server_sock_global);
    remove(SOCK_FILE_ADDR);
    exit(0);
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
    //update_tdf(data);

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




void* change_tdf(void* arg){

    struct tdf_data *data = (struct tdf_data*)arg;
    struct timeval tv;
    int input = 0;
    while(1){
        scanf("%d",&input);
        if(input<1){
            is_end = 1;
            continue;
        }
        data->new_tdf = input;
        fake_gettimeofday(&tv, NULL, data);
        update_tdf(data);
    }

    return NULL;
}

/*
*
*Entry point -- Main loop
*
*/

int main(int argc, char *argv[]){

    pthread_t tid_change_tdf;
    struct tdf_data tdf_info;
    tdf_info.tdf = 1;
    tdf_info.new_tdf = 1;

    pthread_create(&tid_change_tdf, NULL, change_tdf, &tdf_info);

    signal(SIGINT, terminate_process);
    signal(SIGTERM, terminate_process);

    int server_sock;
    struct sockaddr_un dest;
    char buffer[REQUEST_LENGTH];
    if((server_sock = socket(AF_UNIX,SOCK_STREAM,0)) == -1){
        printf("Server Socket open failed: %s\n",strerror(errno));
        return -1;
    }
    server_sock_global = server_sock;
    bzero(&dest, sizeof(struct sockaddr_in));
    dest.sun_family = AF_UNIX;
    strcpy(dest.sun_path, SOCK_FILE_ADDR);

    int opt = 1;
    if(setsockopt(server_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt))==-1){
        printf("Set server socket failed: %s\n",strerror(errno));
    }

    if(bind(server_sock, (struct sockaddr*)&dest,sizeof(dest)) == -1){
        printf("Server bind failed: %s\n",strerror(errno));
    }
    if(listen(server_sock,MAX_CONNECTIONS)==-1){
        printf("Listen socket failed: %s\n",strerror(errno));
        return -1;
    }

    fd_set readfds; //selector for connection sockets
    int client_sock_array[MAX_CONNECTIONS];
    bzero(client_sock_array,MAX_CONNECTIONS*sizeof(int));
    int client_sock = -1;
    int max_sd = -1;
    struct sockaddr_un client_addr;
    int client_addr_len = sizeof(struct sockaddr);
 
    while(is_end==0){

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
            printf("New connection\n");
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
                        fake_gettimeofday(&tv, NULL, &tdf_info);
                        memcpy(buffer+COMMAND_OFFSET, &tv, sizeof(struct timeval));
                    }else if(command == TYPE_TDF){
                        memcpy(buffer+COMMAND_OFFSET, &tdf_info.tdf, sizeof(int));
                    }
                    send(client_sock_array[i], buffer, REQUEST_LENGTH,0);
                }
            }
        }

    }
    close(server_sock);
    
    


    return 0;

}
