#include"kernel/types.h"
#include"user.h"
#include"kernel/stat.h"
#include"kernel/fs.h"
int match(char* , char *);
char*
fmtname(char *path)
{
	static char buf[DIRSIZ+1];
	char *p;
	for(p=path+strlen(path);p>=path&&*p!='/';--p){
		;}
	p++;
	if(strlen(p)>=DIRSIZ){
		return p;
	}
	memmove(buf,p,strlen(p));
	memset(buf+strlen(p),'\0',DIRSIZ-strlen(p));

	return buf;
}
void find(char*path,char*name){
	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;

	if((fd=open(path,0))<0){
		fprintf(2,"find: cannot open %s\n",path);
		return;
	}

	if(fstat(fd,&st)<0){
		fprintf(2,"find: cannot stat %s\n",path);
		close(fd);
		return;
	}

	switch(st.type){
		case T_FILE:
			if(match(name,fmtname(path))){
			//if(strcmp(name,fmtname(path))==0){
				printf("%s\n",path);
			}
			break;

		case T_DIR:
			if(strlen(path)+1+DIRSIZ+1>sizeof(buf)){
				fprintf(2,"find: path too long\n");
				break;
			}
			strcpy(buf,path);
			p=buf+strlen(buf);
			*p++='/';

			while(read(fd,&de,sizeof(de))==sizeof(de)){
				if(de.inum==0)
					continue;
				if(strcmp(de.name,".")==0||strcmp(de.name,"..")==0)
					continue;
				memmove(p,de.name,DIRSIZ);
				p[DIRSIZ]=0;
				find(buf,name);
			}
			break;
		default:
			break;
	}
	close(fd);
	return;

}
int main(int argc,char* argv[])
{
	if(argc<3){
		fprintf(2,"Too few parameters!\n");
		exit(1);
		// Actually, it needs to work with fewer than 3 arguments, such as find a, which lists all items in the
		// a directory.

	}
	find(argv[1],argv[2]);
	exit(0);
}



// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9.

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}
