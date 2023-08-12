#include"kernel/types.h"
#include"user.h"
int main(int argc,char *argv[])
{
    if(argc>1){
        fprintf(2,"Too many arguments for this version!\n");
        exit(1);
    }
    printf("%d\n",uptime());
    exit(0);
}