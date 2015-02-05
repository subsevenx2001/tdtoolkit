#define _GNU_SOURCE
#include <sys/time.h>
#include <dlfcn.h>
#include <stdio.h>

#define DILATION_RATE 2


int (*orig_gettimeofday)(struct timeval *tv,struct timezone *tz);
//clock_t (*orig_clock)(void);
time_t (*orig_time)(time_t* timer);

struct timeval zerotime;

int gettimeofday(struct timeval *tv,struct timezone *tz){

    printf("fake gettimeofday2!\n");
    orig_gettimeofday(tv,tz);

    //tv->tv_sec-=zerotime.tv_sec;
    //tv->tv_usec-=zerotime.tv_usec;

    //while(tv->tv_usec<0){
    //    tv->tv_usec+=1000000;
    //    tv->tv_sec-=1;
    //}

    
    tv->tv_sec/=DILATION_RATE;
    tv->tv_usec/=DILATION_RATE;

    return 0;

}



//clock_t clock(void){
//    return orig_clock()/DILATION_RATE;
//}

time_t time(time_t *timer){
    return orig_time(timer)/DILATION_RATE;
}



void _init(void){
    printf("overriding time\n");
    orig_gettimeofday = dlsym(RTLD_NEXT,"gettimeofday");
    orig_gettimeofday(&zerotime,NULL);
    orig_time = dlsym(RTLD_NEXT, "time");
}
