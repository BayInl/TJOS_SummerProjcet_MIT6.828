#include"kernel/types.h"
#include"user.h"
int main(int argc,char *argv[]){
	int p1[2];
	int p2[2];
	char ch[1];

	if(pipe(p1)==-1||pipe(p2)==-1){
		fprintf(2,"pingpong: create pipe failed!\n");
		exit(1);
	}

	int cpid=fork();
	if(cpid==0){
		close(p1[1]);//close the write end of first pipe
		read(p1[0],ch,1);//read date
		int t=getpid();
		printf("%d: received ping\n",t);
		close(p1[0]);

		close(p2[0]);
		write(p2[1],"1",1);
		close(p2[1]);
	}
	else{
		//is parent process
		close(p1[0]);//close read end of pipe first
		write(p1[1],"1",1);//write date to the first pipe
		close(p1[1]);

		close(p2[1]);
		read(p2[0],ch,1);
		int t=getpid();
		printf("%d: received pong\n",t);
		close(p2[0]);

	}
	exit(0);
}
