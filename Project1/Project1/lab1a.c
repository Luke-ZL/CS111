//#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>  //atoi
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h> //execvp
#include <errno.h>
#include <sys/types.h> 

int fd_valid(int* arr, int fd, int size)
{
	int i = 0;
	if (fd >= size) return 0;
	if (arr[fd] == -1) return 0;
	return 1;
}

int main(int argc, char* argv[])
{
	int fd_array_size = 100;
	int rdonly_flag = 0;
	int wronly_flag = 0;
	int* fd_array = malloc(sizeof(int) * fd_array_size);
	int verbose_flag = 0;
	struct option options[] =
	{
		{ "rdonly", required_argument, &rdonly_flag, 1 },
		{ "wronly", required_argument, &wronly_flag, 1 },
		{ "command", required_argument, NULL , 3 },
		{ "verbose", no_argument, &verbose_flag, 1 },
		{ 0, 0, 0, 0 }
	};

	int c, i, index, WRONG_FD_FLAG;
	int IOE_D[3];
	int arg_count;
	char** cmd;
	int exit_stat = 0;
	int file_count = 0;
	int file_count_success = 0;

	while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {

		switch (c) {
		case 3:
			arg_count = 0;
			WRONG_FD_FLAG = 0;
			// store the filedescriptors
			index = optind - 1;
			for (i = 0; i < 3; i++)
			{
				IOE_D[i] = atoi(argv[index]);
				if (!fd_valid(fd_array, IOE_D[i], file_count)) {
					fprintf(stderr, "unspecified file descriptor\n");
					exit_stat = 1;
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
			}

			pid_t childPID = fork();
			if (childPID > 0);
			else if (childPID == 0) {
				int j;
				for (j = 0; j < 3; j++)
				{
					close(j);
					dup2(fd_array[IOE_D[j]] + 3, j); //https://stackoverflow.com/questions/35447474/in-c-are-file-descriptors-that-the-child-closes-also-closed-in-the-parent, close() in child process does not affect the parent
					close(fd_array[IOE_D[j]] + 3);
				}
				if (execvp(cmd[0], cmd) == -1) {
					fprintf(stderr, "execvp error: %s\n", strerror(errno));
					exit_stat = 1;
				}
			}
			else {
				fprintf(stderr, "Cannot create child process: %s\n", strerror(errno));
				exit_stat = 1;
			}

			break;

		case '?':
			fprintf(stderr, "Please use spcified options\n");
			exit_stat = 1;
			break;

		default:
			if (rdonly_flag || wronly_flag) {



				int WR_flag = (rdonly_flag) ? 0 : 1;
				if (verbose_flag) {
					if (rdonly_flag)
						printf("--rdonly %s\n", optarg);
					else printf("--wronly %s\n", optarg);
				}

				int fd;

				if ((fd = open(optarg, WR_flag)) == -1) {
					fprintf(stderr, "unable to open the specified file: %s\n", strerror(errno));
					exit_stat = 1;
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

				rdonly_flag = 0;
				wronly_flag = 0;
			}


		}
	}

	exit(exit_stat);
}
