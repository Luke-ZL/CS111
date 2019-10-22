//#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>  //atoi
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h> //execvp
#include <errno.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <sys/resource.h>

int print_usage(char * argument, struct rusage start_point)
{
	int get_status = 1;
	struct rusage usage;
	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		fprintf(stderr, "Fail to use getrusage()\n");
		fflush(stderr);
		get_status = 0;
	}
	else {
		double user_time = (double)((usage.ru_utime.tv_sec - start_point.ru_utime.tv_sec) + (usage.ru_utime.tv_usec - start_point.ru_utime.tv_usec) * 0.000001);
		double sys_time = (double)((usage.ru_stime.tv_sec - start_point.ru_utime.tv_sec) + (usage.ru_stime.tv_usec - start_point.ru_stime.tv_usec) * 0.000001);
		printf("%s: usertime = %f, systime = %f\n", argument, user_time, sys_time);
		fflush(stdout);
	}
	return get_status;
}

int fd_valid(int* arr, int fd, int size)
{
	int i = 0;
	if (fd >= size) return 0;
	if (arr[fd] == -1) return 0;
	return 1;
}

void handler(int sig)
{
	fprintf(stderr, "%d caught", sig);
	fflush(stderr);
	exit(sig);
}

int main(int argc, char* argv[])
{



	int fd_array_size = 100;
	//int pipe_read_end_array_size = 0;
	int rdonly_flag = 0;
	int wronly_flag = 0;
	int rdwr_flag = 0;
	int file_flag = 0;
	int process_count = 0;
	int* fd_array = malloc(sizeof(int) * fd_array_size);
	int verbose_flag = 0;
	int profile_flag = 0;
	struct option options[] =
	{
		{ "rdonly", required_argument, &rdonly_flag, 1 },
		{ "wronly", required_argument, &wronly_flag, 1 },
		{ "rdwr", required_argument, &rdwr_flag, 1 },
		{ "command", required_argument, NULL , 1 },
		{ "verbose", no_argument, &verbose_flag, 1 },
		{ "append", no_argument, NULL, 2 },
		{ "cloexec", no_argument, NULL, 3 },
		{ "creat", no_argument, NULL, 4 },
		{ "directory", no_argument, NULL, 5 },
		{ "dsync", no_argument, NULL, 6 },
		{ "excl", no_argument, NULL, 7 },
		{ "nofollow", no_argument, NULL, 8 },
		{ "nonblock", no_argument, NULL, 9 },
		{ "rsync", no_argument, NULL, 10 },
		{ "sync", no_argument, NULL, 11 },
		{ "trunc", no_argument, NULL, 12 },
		{ "pipe", no_argument, NULL, 13 },
		{ "wait", no_argument, NULL, 14 },
	    { "close", required_argument, NULL, 15 },
		{ "abort", no_argument, NULL, 16 },
		{ "catch", required_argument, NULL, 17 },
		{ "ignore", required_argument, NULL, 18 },
		{ "default", required_argument, NULL, 19 },
		{ "pause", no_argument, NULL, 20 },
		{ "profile", no_argument, NULL, 21 },
		{ 0, 0, 0, 0 }
	};

	int process_size = 1000;
	int c, i, index, WRONG_FD_FLAG, j;
	int IOE_D[3];
	int arg_count;
	char** cmd;
	int exit_stat = 0;
	int signal_stat = 0;
	int file_count = 0;
	int file_count_success = 0;
	//pid_t* process_array = malloc(sizeof(pid_t) * process_size);
	//char*** command_array = (char ***)malloc(sizeof(char**) * process_size);
	//int* arg_count_array = malloc(sizeof(int) * process_size);
	pid_t process_array[1000];
	int arg_count_array[1000];
	int command_array[1000];
	//int* pipe_read_end_array = malloc(sizeof(int));
	int who = RUSAGE_SELF;
	struct rusage usage;
	int ret;

	while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {

		switch (c) {
		case 1:
			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					exit_stat = 1;
				}
			}
			arg_count = 0;
			WRONG_FD_FLAG = 0;
			// store the filedescriptors
			index = optind - 1;
			for (i = 0; i < 3; i++)
			{
				IOE_D[i] = atoi(argv[index]);
				if (!fd_valid(fd_array, IOE_D[i], file_count)) {
					fprintf(stderr, "unspecified file descriptor\n");
					fflush(stderr);
					if (exit_stat == 0) exit_stat = 1;
					WRONG_FD_FLAG = 1;

				}
				index++;
			}


			while (1)
			{
				if (index >= argc) {
					optind = index;
					break;
				}
				if ((argv[index][0] == '-') && (argv[index][1] == '-')) {
					optind = index;
					break;
				}
				else {
					arg_count++;
					index++;
				}
			}
			
			if (arg_count == 0) {
				fprintf(stderr, "please specify the command\n");
				fflush(stderr);
				if (exit_stat == 0) exit_stat = 1;
				break;
			}
			if (WRONG_FD_FLAG) break; //check wrong fd, if wrong just break without executing

			index -= arg_count;
			cmd = (char **)malloc(arg_count + 1);

			for (i = 0; i < arg_count; i++)
			{
				cmd[i] = argv[index + i];
			}

			cmd[arg_count] = (char *)NULL;

			if (verbose_flag) {
				printf("--command %d %d %d", IOE_D[0], IOE_D[1], IOE_D[2]);
				for (i = 0; i < arg_count; i++)
				{
					printf(" %s", cmd[i]);
				}
				printf("\n");
				fflush(stdout);
			}

			pid_t childPID = fork();


			if (childPID > 0){
				process_array[process_count] = childPID;
				//process_array = realloc(process_array, sizeof(pid_t) * (process_count + 1));
				command_array[process_count] = index;
				//command_array = realloc(command_array, sizeof(char**) * (process_count + 1));
				arg_count_array[process_count] = arg_count;
				//arg_count_array = realloc(arg_count_array, sizeof(int) * (process_count + 1));
				process_count++;

				if (profile_flag) {
					if (!print_usage("--command", usage)) {
						if (exit_stat == 0) exit_stat = 1;
					}
				}

			}
			else if (childPID == 0) {
				for (j = 0; j < 3; j++)
				{
					close(j);
					dup2(fd_array[IOE_D[j]] + 3, j); //https://stackoverflow.com/questions/35447474/in-c-are-file-descriptors-that-the-child-closes-also-closed-in-the-parent, close() in child process does not affect the parent
					//close(fd_array[IOE_D[j]] + 3);
				}
				for (j = 0; j < file_count_success; j++)
					close(j + 3);
				if (execvp(cmd[0], cmd) == -1) {
					fprintf(stderr, "execvp error: %s\n", strerror(errno));
					fflush(stderr);
					if (exit_stat == 0) exit_stat = 1;
				}
			}
			else {
				fprintf(stderr, "Cannot create child process: %s\n", strerror(errno));
				fflush(stderr);
				if (exit_stat == 0) exit_stat = 1;
			}


			break;

		case 2:
			if (verbose_flag) printf("--appned\n");
			fflush(stdout);
			file_flag = file_flag | O_APPEND;
			break;
		case 3:
			if (verbose_flag) printf("--cloexec\n");
			fflush(stdout);
			file_flag = file_flag | O_CLOEXEC;
			break;
		case 4:
			if (verbose_flag) printf("--creat\n");
			fflush(stdout);
			file_flag = file_flag | O_CREAT;
			break;
		case 5:
			if (verbose_flag) printf("--directory\n");
			fflush(stdout);
			file_flag = file_flag | O_DIRECTORY;
			break;
		case 6:
			if (verbose_flag) printf("--dsync\n");
			fflush(stdout);
			file_flag = file_flag | O_DSYNC;
			break;
		case 7:
			if (verbose_flag) printf("--excl\n");
			fflush(stdout);
			file_flag = file_flag | O_EXCL;
			break;
		case 8:
			if (verbose_flag) printf("--nofollow\n");
			fflush(stdout);
			file_flag = file_flag | O_NOFOLLOW;
			break;
		case 9:
			if (verbose_flag) printf("--nonblock\n");
			fflush(stdout);
			file_flag = file_flag | O_NONBLOCK;
			break;
		case 10:
			if (verbose_flag) printf("--rsync\n");
			fflush(stdout);
			file_flag = file_flag | O_RSYNC;
			break;
		case 11:
			if (verbose_flag) printf("--sync\n");
			fflush(stdout);
			file_flag = file_flag | O_SYNC;
			break;
		case 12:
			if (verbose_flag) printf("--trunc\n");
			fflush(stdout);
			file_flag = file_flag | O_TRUNC;
			break;

		case 13:

			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					exit_stat = 1;
				}
			}

			if (verbose_flag) printf("--pipe\n");
			fflush(stdout);

			int pipe_flags[2];
			
			file_count += 2;

			if (file_count >= fd_array_size) {
				fd_array_size *= 2;
				fd_array = realloc(fd_array, sizeof(int) * fd_array_size);
			}

			if (pipe(pipe_flags) < 0) {
				fprintf(stderr, "Pipe open error: %s\n", strerror(errno));
				fflush(stderr);
				fd_array[file_count - 2] = -1;
				fd_array[file_count - 1] = -1;
			}
			else {
				fd_array[file_count - 2] = file_count_success;
				fd_array[file_count - 1] = file_count_success + 1;
				//pipe_read_end_array[pipe_read_end_array_size] = file_count_success;
				file_count_success += 2;
				//pipe_read_end_array_size++;
				//pipe_read_end_array = realloc(pipe_read_end_array, sizeof(int)*(pipe_read_end_array_size + 1));
			}
			if (profile_flag) {
				if (!print_usage("--pipe", usage)) {
					if (exit_stat == 0) exit_stat = 1;
				}
			}
			break;

		case 14:
			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					exit_stat = 1;
				}
			}
			if (verbose_flag) printf("--wait\n");
			//printf("process_count = %d\n", process_count);
			fflush(stdout);

			int closed_count = 0;
		

			while (closed_count < process_count)
			{
				int pid_stat;
				pid_t closed_ID= waitpid(-1, &pid_stat, 0);
				//printf("closed_ID = %d\n", closed_ID);
				//printf("pid_stat = %d\n", pid_stat);
				//fflush(stdout);

				for (i = 0; i < process_count; i++) {
					if (closed_ID == process_array[i]) break; //i serves as the index
				}

				//printf("i=%d\n",i);
				//printf("aca=%d\n",arg_count_array[i]);
				//fflush(stdout);

				if (WIFSIGNALED(pid_stat)) {         //terminate by a signal
					int signal_number = WTERMSIG(pid_stat);
					printf("signal %d", signal_number);
					for (j = 0; j < arg_count_array[i]; j++)
					{
						printf(" %s", argv[command_array[i]+j]);
					}
					printf("\n");
					fflush(stdout);
					if (signal_stat < signal_number) signal_stat = signal_number;
				}
				else if (WIFEXITED(pid_stat)){
					int exit_number = WEXITSTATUS(pid_stat);
					printf("exit %d", exit_number);
					for (j = 0; j < arg_count_array[i]; j++)
					{
						printf(" %s", argv[command_array[i] + j]);
					}
					printf("\n");
					fflush(stdout);
					if (exit_stat < exit_number) exit_stat = exit_number;
				}

				closed_count++;
			}


			process_count = 0; //reset

			if (profile_flag) {
				if (!print_usage("--wait", usage)) {
					if (exit_stat == 0) exit_stat = 1;
				}
			

				if (getrusage(RUSAGE_CHILDREN, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					if(exit_stat == 0) exit_stat = 1;
				}
				else {
					double user_time = (double)(usage.ru_utime.tv_sec + usage.ru_utime.tv_usec * 0.000001);
					double sys_time = (double)(usage.ru_stime.tv_sec + usage.ru_stime.tv_usec * 0.000001);
					printf("Profile for all child processes: usertime = %f, systime = %f\n", user_time, sys_time);
					fflush(stdout);
				}

			}

			break;

		case 15:
			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					exit_stat = 1;
				}
			}

			if (verbose_flag) printf("--close %s\n", optarg);
			fflush(stdout);
			int close_fd = atoi(optarg);
			close(fd_array[close_fd] + 3);
			fd_array[close_fd] = -1;
			//for (i = 0; i < pipe_read_end_array_size; i++) {
			//	if (close_fd == pipe_read_end_array[i]) {
			//		close(fd_array[close_fd + 1] + 3);
			//		fd_array[close_fd + 1] = -1;
		//		}
		//	}

			if (profile_flag) {
				if (!print_usage("--close", usage)) {
					if (exit_stat == 0) exit_stat = 1;
				}
			}

			break;

		case 16:
			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					exit_stat = 1;
				}
			}
			if (verbose_flag) printf("--abort\n");
			fflush(stdout);
			char *p = NULL;
			*p = 'L';

			if (profile_flag) {
				if (!print_usage("--abort", usage)) {
					if (exit_stat == 0) exit_stat = 1;
				}
			}

			break;

		case 17:

			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					exit_stat = 1;
				}
			}


			if (verbose_flag) printf("--catch %s\n", optarg);
			fflush(stdout);
			signal(atoi(optarg), handler);
			break;

			if (profile_flag) {
				if (!print_usage("--catch", usage)) {
					if (exit_stat == 0) exit_stat = 1;
				}
			}


		case 18:

			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					exit_stat = 1;
				}
			}


			if (verbose_flag) printf("--ignore %s\n", optarg);
			fflush(stdout);
			signal(atoi(optarg), SIG_IGN);

			if (profile_flag) {
				if (!print_usage("--ignore", usage)) {
					if (exit_stat == 0) exit_stat = 1;
				}
			}

			break;

		case 19:

			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					exit_stat = 1;
				}
			}


			if (verbose_flag) printf("--default %s\n", optarg);
			fflush(stdout);
			signal(atoi(optarg), SIG_DFL);

			if (profile_flag) {
				if (!print_usage("--default", usage)) {
					if (exit_stat == 0) exit_stat = 1;
				}
			}

			break;

		case 20:

			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					exit_stat = 1;
				}
			}

			if(verbose_flag) printf("--pause\n");
			fflush(stdout);
			pause();


			if (profile_flag) {
				if (!print_usage("--pause", usage)) {
					if (exit_stat == 0) exit_stat = 1;
				}
			}

			break;

		case 21:
			if (verbose_flag) printf("--profile\n");
			fflush(stdout);
			profile_flag = 1;
			break;

		case '?':
			fprintf(stderr, "Please use spcified options\n");
			fflush(stderr);
			if (exit_stat == 0) exit_stat = 1;
			break;

		default:

			if (profile_flag) {
				if (getrusage(who, &usage) < 0) {
					fprintf(stderr, "Fail to use getrusage()\n");
					fflush(stderr);
					printf("HI\n");
					fflush(stdout);
					exit_stat = 1;
				}
			}


			if (rdonly_flag || wronly_flag || rdwr_flag) {
				int WR_flag;
				if (rdonly_flag) WR_flag = 0;
				else if (wronly_flag) WR_flag = 1;
				else WR_flag = 2;

				if (verbose_flag) {
					if (rdonly_flag)
						printf("--rdonly %s\n", optarg);
					else if (wronly_flag) printf("--wronly %s\n", optarg);
					else printf("--rdwr %s\n", optarg);
					fflush(stdout);
				}

				int fd;

				if ((fd = open(optarg, WR_flag | file_flag, 0666)) == -1) {   //need to specify mode 0666 for creation
					fprintf(stderr, "unable to open the specified file %s: %s\n", optarg, strerror(errno));
					fflush(stderr);
					if (exit_stat == 0) exit_stat = 1;
					fd_array[file_count] = -1;
				}
				else {

					fd_array[file_count] = file_count_success;
					file_count_success++;
				}

				file_count++;

				if (file_count >= fd_array_size) {
					fd_array_size *= 2;
					fd_array = realloc(fd_array, sizeof(int) * fd_array_size);
				}

			
				file_flag = 0;
			}

			if (profile_flag) {
				if (rdonly_flag) {
					if (!print_usage("--rdonly", usage)) {
						if (exit_stat == 0) exit_stat = 1;
					}
				}
				else if (wronly_flag) {
					if (!print_usage("--wronly", usage)) {
						if (exit_stat == 0) exit_stat = 1;
					}
				}
				else if (rdwr_flag) {
					if (!print_usage("--rdwr", usage)) {
						if (exit_stat == 0) exit_stat = 1;
					}
				}
			}
			rdonly_flag = 0;
			wronly_flag = 0;
			rdwr_flag = 0;
		}
	}
	/*
	//for bench only
	if (profile_flag) {
		struct rusage usage_bench;
		if (getrusage(who, &usage_bench) < 0) {
			fprintf(stderr, "Fail to use getrusage()\n");
			fflush(stderr);
			if (exit_stat == 0) exit_stat = 1;
		}
		else {
			double user_time = (double)(usage_bench.ru_utime.tv_sec + usage_bench.ru_utime.tv_usec * 0.000001);
			double sys_time = (double)(usage_bench.ru_stime.tv_sec + usage_bench.ru_stime.tv_usec * 0.000001);
			printf("Profile for simpsh: usertime = %f, systime = %f\n", user_time, sys_time);
			fflush(stdout);
		}

	}
	*/
	if (signal_stat) {
		signal(signal_stat, SIG_DFL);
		raise(signal_stat);
     	}
	else exit(exit_stat);
}
