#include"kernel/types.h"
#include"kernel/param.h"
#include"user.h"
int main(int argc,char *argv[])
{
    if(argc<2){
        fprintf(2,"Too few arguments!\n");
        exit(1);
    }

    if(argc+1>MAXARG){
        fprintf(2,"Too many arguments!\n");
        exit(1);
    }
    char* arguments[MAXARG]={0};
    char buf[512]={0};

    for(int i=1;i<argc;++i){
        arguments[i-1]=argv[i];
    }
    char *p=buf;    
    while(read(0,p,1)==1){
        if(*p=='\n'){
            *p='\0';
            arguments[argc-1]=buf;
            p=buf;
            if(fork()==0){
                exec(argv[1],arguments);

                fprintf(2,"exec %s failed",argv[1]);// Normally the process will not execute this position.
                exit(0);
            }
            else{
                wait(0);
                
            }
        }
        else{
            p++;
        }
    }
    
    exit(0);
}