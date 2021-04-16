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
#include <time.h>
#define NANO 1000000000LL
long long diff;
int busy=0;
struct timespec begin, end;
//manager
char task_q[100][512];
int q_size = 0;
int num_dirs=1, num_lines=0, num_files=0;
int my_pid;
int pid[8];
int task, result;
int num_process=2;
void handler(int sig){
	clock_gettime(CLOCK_MONOTONIC, &end);
	diff = NANO * (end.tv_sec - begin.tv_sec) + (end.tv_nsec - begin.tv_nsec);
	diff /= 1000000;
	printf("TIME : %lldms",diff);
	printf("\nDIRECTORIES : %d\n",num_dirs);
	printf("FOUND LINES : %d\n", num_lines);
	printf("FOUND FILES : %d\n", num_files);
	
	close(result);
	close(task);
	for(int i=0;i<num_process;i++){
		kill(pid[i],SIGKILL);
	}
	kill(my_pid, SIGKILL);
}

void push(char task_q[100][512], int* q_size, char* str) {
	if (*(q_size) >= 100) return;
	strcpy(task_q[*q_size], str);
	*(q_size) += 1;
}
void pop(char task_q[100][512], int* q_size) {
	if (*(q_size) == 0) return ;
	if (*(q_size) == 1) strcpy(task_q[0],"");
	else {for (int i = 0; i <= *(q_size)-2;i++) {
		strcpy(task_q[i], task_q[i + 1]);
	}}
	*(q_size) -= 1;
}
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
		if ((c == EOF || c == '\n') && (len > 0))
			break ; 
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
	
    sprintf(dest, "%s",s);
    return ;
}
int main(int argc, char* argv[]){
	int a_path = 0;
	int case_sen = 0;
	int arg_index=1;
	char org_path[512];
	char dirpath[512];
	char keyword[30][128];
	my_pid= getpid();
	clock_gettime(CLOCK_MONOTONIC, &begin);
	while(1){
		if(argv[arg_index][0]=='-'){
			if(strcmp(argv[arg_index],"-p")==0){
				if(atoi(argv[arg_index+1])<1 || atoi(argv[arg_index+1])>8){
					printf("ERROR : The <-p> option should be number in 1~8\n");
					return 0;
				}
				num_process = atoi(argv[arg_index+1]);
				arg_index+=2;
			}
			else if(strcmp(argv[arg_index],"-a")==0){
				a_path=1;
				arg_index++;
			}
			else if(strcmp(argv[arg_index],"-c")==0){
				case_sen=1;
				arg_index++;
			}
			else{
				printf("ERROR : OPTION INPUT ERROR\n");
				return 0;
			}
		}
		else break;
	}
	strcpy(dirpath, argv[1+arg_index-1]);
	strcat(dirpath,"/");
	int num_keyword=argc-(2+arg_index-1);
	
	for(int i=0;i<num_keyword;i++){
		strcpy(keyword[i],argv[i+2+arg_index-1]);
	}
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
	task = open("task", O_WRONLY | O_SYNC);
	result = open("result", O_RDONLY | O_SYNC);
	char a[2];
	sprintf(a,"%d",num_process);
	write_pro(task, a);
	sprintf(a, "%d", case_sen);
	write_pro(task, a);
	for(int i=0; i<num_keyword;i++){
		write_pro(task, keyword[i]);
	}
	
	write_pro(task, "^STARTFLAG");
	
	push(task_q, &q_size, dirpath);
	signal(SIGINT, handler);
	int i=0;
	while(1){
		if(q_size>0) {
			write_pro(task, task_q[0]);
			busy++;
			pop(task_q, &q_size);
		}
		char temp[512];
		read_pro(result, temp);
		if(q_size==0 && strlen(temp)<2) break;
		char *find =NULL;
		find = strstr(temp, "^FILEFLAG");
		if(find!=NULL) {
			num_files++;
		}
		find=NULL;
		find = strstr(temp, "^PIDFLAG");
		if(find!=NULL) {
			char *ptr = strtok(temp, " ");
			pid[i]=atoi(ptr);
			i++;
		}
		find=NULL;
		find = strstr(temp, "^TASKFLAG");
		if(find!=NULL) {
			busy--;
		}
		find=NULL;
		find = strstr(temp, "^DIRFLAG");
		if(find!=NULL) {
			num_dirs++;
			char *ptr = strtok(temp, " ");
			push(task_q, &q_size, ptr);
		}
		find=NULL;
		find = strstr(temp, "^LINEFLAG");
		if(find!=NULL){
			num_lines++;
			char *ptr = strtok(temp, " ");
			ptr = strtok(NULL,"");
			if(a_path==1) printf("%s\n",ptr);
			else if(a_path==0) printf("%s\n",ptr+strlen(dirpath));
		}
		
		if(q_size==0 && busy==0) break;
	}
	
	clock_gettime(CLOCK_MONOTONIC, &end);
	diff = NANO * (end.tv_sec - begin.tv_sec) + (end.tv_nsec - begin.tv_nsec);
	diff /= 1000000;
	
	printf("TIME : %lldms",diff);
	printf("\nDIRECTORIES : %d\n",num_dirs);
	printf("FOUND LINES : %d\n", num_lines);
	printf("FOUND FILES : %d\n", num_files);
	
	
	close(task);
	close(result);
	for(int i=0;i<num_process;i++){
		kill(pid[i],SIGKILL);
	}
	return 0;
}
