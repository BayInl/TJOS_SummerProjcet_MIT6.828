#include"kernel/types.h"
#include"user.h"

void primes()
{
	int prime;
	int p[2];
	if(read(0,&prime,sizeof(int))==0){
		exit(0);
	}
	printf("prime %d\n",prime);

	pipe(p);
	if(fork()==0){
		close(0);
		dup(p[0]);
		close(p[0]);
		close(p[1]);
		primes();
	}
	else{
		close(p[0]);
		int number;
		while(read(0,&number,sizeof(int))==4){
			if(number%prime!=0){
				write(p[1],&number,sizeof(int));
			}
		}
		close(p[1]);
		wait(0);
	}
	exit(0);
}
int main(int argc,char* argv[])
{
	int p[2];
	pipe(p);
	close(0);
	if(fork()==0){
		dup(p[0]);
		close(p[0]);
		close(p[1]);
		primes();
	
	}
	else {
		close(p[0]);
		for(int i=2;i<36;++i){
			write(p[1],&i,sizeof(int));
		}
		close(p[1]);
		wait(0);
	}
	exit(0);
}
