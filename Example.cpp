#include <stdio.h>
#include <stdlib.h>
#include<math.h>
volatile int total = 1000; 
void test(){
    volatile int count = 0;
    volatile int taken_count = 0;
    for (int i = 0; i < total; i++) {
        
        if(i&1){

        }
        else int c=0;

    }
    return;
}

int main() {
    printf("Running test loop with %d iterations...\\n", total);

    for(int i=0;i<100;i++){
        test();
        // for(int j=0;j<100;j++){
        //     int c=-1;
        //     int a=rand()%100,b=rand()%100;
        //     if((a+b)&1){
        //         c=1;
        //     }
        //     else c=0;
        // }
    }

    // printf("Test complete. count=%d, taken=%d\\n", count, taken_count);
    return 0;
}