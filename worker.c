#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#define _GNU_SOURCE
//worker
char *strcasestr(const char *haystack, const char *needle);
int task, result;
int case_sen = 0;
int read_bytes (int fd, void * a, int len)
{
	char * s = (char *) a ;
	
	int i ;
	for (i = 0 ; i < len ; ) {
		int b ;
		b = read(fd, s + i, len - i) ;
		if (b == 0)
			break ;
		i += b ;
	}
	return i ; 
}
int write_bytes (int fd, void * a, int len)
{
	char * s = (char *) a ;

	int i = 0 ; 
	while (i < len) {
		int b ;
		b = write(fd, s + i, len - i) ;
		if (b == 0)
			break ;
		i += b ;
	}
	return i ;	
}
void write_pro(int target, char *str){
	char s[512] ;
	size_t len = 0 ;
	while (len < 128) {
		char c ;
		c = str[len] ; 
		if ((c == EOF || c == '\n') && (len > 0)) break ; 
		s[len] = c ;
		len++;
	}		

	if (write_bytes(target, &len, sizeof(len)) != sizeof(len)) return ;
	if (write_bytes(target, s, len) != len) return ;

	return ;
}
void read_pro(int src, char dest[512]){
    char s[512] ;
    size_t len, bs ;
    flock(src, LOCK_EX) ;
    if (read_bytes(src, &len, sizeof(len)) != sizeof(len)) {
        flock(src, LOCK_UN) ;
        return ;
    }
    bs = read_bytes(src, s, len) ;
    flock(src, LOCK_UN) ;
    if (bs != len) return ;
    strncpy(dest, s, len+1);
    return ;
}
void find(char keyword[30][128],int size, char* path) {
	int linecount = 0;
	FILE* pFile = NULL;
	pFile = fopen(path, "r");
	if (pFile != NULL) {
		char result_temp[512];
		int findcheck = 0;
		char strtemp[512];
		int linecount = 0;
		while (!feof(pFile))
		{
			char flag[512];
			linecount++;
			fgets(strtemp, sizeof(strtemp), pFile);
			
			int ok=1;
			for(int i=0;i<size;i++){
				char* find=NULL;
				if(case_sen) find = strcasestr(strtemp, keyword[i]);
				else 
				find = strstr(strtemp, keyword[i]);
				if(find==NULL){
					ok=0;
					break;
				}
			}
			if(ok) {
				sprintf(result_temp, "^LINEFLAG %s:%d:%s ", path, linecount, strtemp);
            	write_pro(result, result_temp);
			}
		}
		fclose(pFile);
	}
	return ;
}
int main(){
	int status;
    pid_t pid[8];
	printf("%d\n",getpid());
    int num_keyword=0;
    int num_process;
    char keyword[30][128];
    int protocol=-1; // 0:numpro / 1:casesen / 2:keyword / 3:start
    if (mkfifo("task", 0666)) {
		if (errno != EEXIST) {
			perror("FAIL TO CALL FIFO(TASK)\n");
			exit(1);
		}
	}
	if (mkfifo("result", 0666)) {
		if (errno != EEXIST) {
			perror("FAIL TO CALL FIFO(TASK)\n");
			exit(1);
		}
	}
    task = open("task", O_RDONLY | O_SYNC);
	result = open("result", O_WRONLY | O_SYNC);
	char str_temp[512];
	read_pro(task, str_temp);
	num_process = atoi(str_temp);
    read_pro(task, str_temp);
	case_sen = atoi(str_temp);

	int go=0;
	while(1){
		read_pro(task,str_temp);
		if(strcmp(str_temp, "^STARTFLAG")==0) go=1;
		if(go==0){
			strcpy(keyword[num_keyword], str_temp);
			num_keyword++;
		}
		else break;
	}
	for(int i=0;i<num_process;i++){
		pid[i]=fork();
		char pid_temp[10];
		sprintf(pid_temp,"%d ^PIDFLAG",getpid());
		write_pro(result, pid_temp);
		while(1){
		if(pid[i]==0){
			char temp[512];
			read_pro(task, temp);
			struct stat buf;
			DIR *dir_info;
			dir_info = opendir(temp);
			struct dirent* dir_entry;

			while ((dir_entry = readdir(dir_info)) != NULL) {
				if(dir_entry->d_name[0]=='.') continue;
				char path[512];
				sprintf(path, "%s%s", temp, dir_entry->d_name);
				lstat(path, &buf);
				if (S_ISDIR(buf.st_mode)) {
					sprintf(path, "%s/ ^DIRFLAG", path);
					write_pro(result, path);
				}
				else if (S_ISREG(buf.st_mode)) {
					char flag[512];
					sprintf(flag, "^FILEFLAG");
					write_pro(result, flag);
					find(keyword, num_keyword, path);
				}
			}
			char task_flag[10];
			sprintf(task_flag,"^TASKFLAG");
			write_pro(result, task_flag);
		}
		}
		exit(i+1);
		if(pid[i]>0) {wait(&status);}
	}
    close(task);
    close(result);
	return 0;
}

