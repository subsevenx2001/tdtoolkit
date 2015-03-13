#define _GNU_SOURCE
#include <sys/time.h>
#include <dlfcn.h>
#include <stdio.h>

#define DILATION_RATE 2
struct timeval zero_time = {0,0};
time_t ztime;
struct timeval prev  ={0,0};

static int (*orig_gettimeofday)(struct timeval *tv,struct timezone *tz) = NULL;
//clock_t (*orig_clock)(void);
time_t (*orig_time)(time_t* timer);

//struct timeval zerotime;

FILE *fp=NULL;
int tdf = 1;
int new_tdf = 1;
;

void update_tdf();

static void mtrace_init(void){
	printf("overriding time\n");
        orig_gettimeofday = dlsym(RTLD_NEXT,"gettimeofday");
        orig_gettimeofday(&zero_time,NULL);
        printf("Zero Time:\nsec=%ld\nusec=%ld\n",zero_time.tv_sec,zero_time.tv_usec);
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



int gettimeofday(struct timeval *tv,struct timezone *tz){

    if(orig_gettimeofday == NULL){
	mtrace_init();
    }

    update_tdf();
   

    orig_gettimeofday(tv,tz);

    tv->tv_sec-=zero_time.tv_sec;
    tv->tv_usec-=zero_time.tv_usec;
    timeval_normalize(tv);
    
    tv->tv_sec/=tdf;
    tv->tv_usec/=tdf;

    tv->tv_sec+=zero_time.tv_sec;
    tv->tv_usec+=zero_time.tv_usec;
    timeval_normalize(tv);
 


    return 0;

}

void update_tdf(){

    fp = fopen("/lib/tdf.txt","r");
    fscanf(fp,"%d",&new_tdf);
    if(tdf==new_tdf){
        return;
    }

    orig_gettimeofday(&zero_time,NULL);
    printf("TDF=%d\n",new_tdf);
    tdf=new_tdf;
   
    fclose(fp);    

}

//clock_t clock(void){
//    return orig_clock()/DILATION_RATE;
//}

/*time_t time(time_t *timer){

    time_t ntime = orig_time(timer);

    return orig_time(timer)/DILATION_RATE;
}*/



/*void _init(void){
    printf("overriding time\n");
    orig_gettimeofday = dlsym(RTLD_NEXT,"gettimeofday");
    orig_gettimeofday(&zero_time,NULL);
    printf("Zero Time:\nsec=%ld\nusec=%ld\n",zero_time.tv_sec,zero_time.tv_usec);
    //printf("TDF=%d\n",DILATION_RATE);
    //orig_time = dlsym(RTLD_NEXT, "time");
    //ztime = orig_time(NULL);
}*/
