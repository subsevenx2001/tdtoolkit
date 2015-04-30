#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
int main(){

    struct timeval tv;
    while(1){
        gettimeofday(&tv,NULL);
        printf("%ld,%ld\n",tv.tv_sec, tv.tv_usec);
        sleep(1);
    }
    return 0;

}
