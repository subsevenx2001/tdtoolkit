#define _GNU_SOURCE
#include <sys/time.h>
#include <dlfcn.h>
#include <stdio.h>

#define DILATION_RATE 2
struct timeval zero_time = {0,0};
struct timeval prev  ={0,0};

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

int (*orig_gettimeofday)(struct timeval *tv,struct timezone *tz);
//clock_t (*orig_clock)(void);
time_t (*orig_time)(time_t* timer);

//struct timeval zerotime;

int gettimeofday(struct timeval *tv,struct timezone *tz){

    orig_gettimeofday(tv,tz);

    tv->tv_sec-=zero_time.tv_sec;
    tv->tv_usec-=zero_time.tv_usec;
    timeval_normalize(tv);
    
    tv->tv_sec/=DILATION_RATE;
    tv->tv_usec/=DILATION_RATE;

    tv->tv_sec+=zero_time.tv_sec;
    tv->tv_usec+=zero_time.tv_usec;
    timeval_normalize(tv);
 
/*    printf("---------------------------------\n");
    printf("sec=%ld\nusec=%ld\n",tv->tv_sec,tv->tv_usec);
    printf("---------------------------------\n");   

    if(prev.tv_sec!=0){
	print_tv_diff(tv,&prev);
    }

    
    prev.tv_sec = tv->tv_sec;
    prev.tv_usec = tv->tv_usec;*/



    return 0;

}



//clock_t clock(void){
//    return orig_clock()/DILATION_RATE;
//}

//time_t time(time_t *timer){
//    return orig_time(timer)/DILATION_RATE;
//}



void _init(void){
    printf("overriding time\n");
    orig_gettimeofday = dlsym(RTLD_NEXT,"gettimeofday");
    orig_gettimeofday(&zero_time,NULL);
    printf("Zero Time:\nsec=%ld\nusec=%ld\n",zero_time.tv_sec,zero_time.tv_usec);
    //orig_time = dlsym(RTLD_NEXT, "time");
}
