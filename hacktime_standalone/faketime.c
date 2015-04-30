#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <dlfcn.h>
#include <errno.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#include "common.h"


#define DILATION_RATE 2
struct timeval zero_time = {0,0};
struct timeval start_time = {0,0};
time_t ztime;
struct timeval prev  ={0,0};

static int (*orig_gettimeofday)(struct timeval *tv,struct timezone *tz) = NULL;
//clock_t (*orig_clock)(void);
time_t (*orig_time)(time_t* timer);

//struct timeval zerotime;

FILE *fp=NULL;
int tdf = 1;
int new_tdf = 1;
int is_updating=0;

void update_tdf();

key_t mqkey;
int mqid;

MSG_BUF msg;

static void mtrace_init(void){
	printf("overriding time\n");
        orig_gettimeofday = dlsym(RTLD_NEXT,"gettimeofday");
        orig_gettimeofday(&zero_time,NULL);
	orig_gettimeofday(&start_time,NULL);
        printf("Zero Time:\nsec=%ld\nusec=%ld\n",zero_time.tv_sec,zero_time.tv_usec);

	mqkey = ftok(TDT_FNAME,TDT_ID);
 	if(mqkey==-1){
        	printf("ftok error\n");
		//return -1;
	}

	mqid = msgget(mqkey, 0);
	if(mqid == -1){
		printf("msgget error\n");
		//return -1;
    	}
	printf("mqid = %d\n",mqid);

}

void timeval_normalize(struct timeval* tv){



	while(tv->tv_usec<0){
	    tv->tv_usec+=1000000;
	    tv->tv_sec--;
	}

	while(tv->tv_usec>=1000000){
	    tv->tv_usec-=1000000;
	    tv->tv_sec++;
	}



}

void print_tv_diff(struct timeval *tv1, struct timeval *tv2){

	struct timeval diff;
	diff.tv_sec = tv1->tv_sec - tv2->tv_sec;
	diff.tv_usec = tv1->tv_usec - tv2->tv_usec;

	timeval_normalize(&diff);

	double diff_ms;
	
	diff_ms = (double)1000*(double)diff.tv_sec + (double)diff.tv_usec/(double)1000;

	printf("Diff = %fms\n",diff_ms);

}

static int __gettimeofday_noupdate(struct timeval *tv,struct timezone *tz){

    struct timeval real_tv;

    if(orig_gettimeofday == NULL){
	mtrace_init();
    }

    orig_gettimeofday(&real_tv,NULL);

   
    tv->tv_sec = start_time.tv_sec + (real_tv.tv_sec-zero_time.tv_sec)/tdf; 
    tv->tv_usec = start_time.tv_usec + (real_tv.tv_usec-zero_time.tv_usec)/tdf;
    timeval_normalize(tv);
 
    return 0;

}


int gettimeofday(struct timeval *tv,struct timezone *tz){

    struct timeval real_tv;

    if(orig_gettimeofday == NULL){
	mtrace_init();
    }

    update_tdf();
   

    orig_gettimeofday(&real_tv,NULL);

   
    tv->tv_sec = start_time.tv_sec + (real_tv.tv_sec-zero_time.tv_sec)/tdf; 
    tv->tv_usec = start_time.tv_usec + (real_tv.tv_usec-zero_time.tv_usec)/tdf;
    timeval_normalize(tv);
 
    return 0;

}



void update_tdf(){

//    if(tdf == new_tdf){
//        return;
//    }

//    if(is_updating==1){
//        return;
//    }

    if(msgrcv(mqid,&msg,sizeof(int),CONTROLLER,IPC_NOWAIT) !=-1){
//        is_updating = 1;
        if(tdf == msg.tdf){
            return;
        }
        if(msg.tdf<1){
	    printf("warning: illegal tdf\n");
            new_tdf = tdf;
            return;
        }
        __gettimeofday_noupdate(&start_time,NULL);
        orig_gettimeofday(&zero_time,NULL);
    	tdf = msg.tdf;
        //printf("UPDATETEST\n");
//        is_updating = 0;
    }
    
//    if(tdf==new_tdf){
 //       return;
 //   }

//    orig_gettimeofday(&zero_time,NULL);
//    printf("TDF=%d\n",new_tdf);
//    tdf=new_tdf;
   
       

}

//clock_t clock(void){
//    return orig_clock()/DILATION_RATE;
//}

/*time_t time(time_t *timer){

    time_t ntime = orig_time(timer);

    return orig_time(timer)/DILATION_RATE;
}*/




