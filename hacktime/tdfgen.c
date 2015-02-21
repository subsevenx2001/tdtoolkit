#include <stdio.h>
#include <stdlib.h>

int main(){

    int tdf=1;
    FILE *fp;
    while(1){
        scanf("%d",&tdf);
        if(tdf<=0){
            break;
        }
        fp = fopen("/lib/tdf.txt","w");
        fprintf(fp,"%d\n",tdf);
        fclose(fp);
    }

    return 0;
}
