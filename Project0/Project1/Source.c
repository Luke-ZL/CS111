#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void handler(int sig)
{
	fprintf(stderr, "Segmentation error caught with sig num %d\n", sig);
	exit(4);
}

int main(int argc, char* argv[])
{
	struct option options[] =
	{
		{ "input", required_argument, NULL, 'i' },
		{ "output", required_argument, NULL, 'o' },
		{ "segfault", no_argument, NULL, 's' },
		{ "catch", no_argument, NULL, 'c' },
		{ "dump-core", no_argument, NULL, 'd' },
		{0, 0, 0, 0}
	};

	int c;
	char *p = NULL;
	int filedesc, filedesc2;
	while ((c = getopt_long(argc, argv, "ioscd", options, NULL)) != -1) {
		switch (c) {
		case 'i':
		case 'o':
		case 's':
		case 'c':
		case 'd': break;
		default:
			fprintf(stderr, "Please use spcified options\n");
			exit(3);
		}
	}
	optind = 1;
	while ((c = getopt_long(argc, argv, "ioscd", options, NULL)) != -1) {
		switch(c) {
			case 'i':
				if ((filedesc = open(optarg, O_RDONLY)) == -1) {
					fprintf(stderr, "unable to open the specified input file: %s\n", strerror(errno));
					exit(1);
				}
				else {
					dup2(filedesc, 0);
					close(filedesc);
				}
				break;
			case 'o':
				if ((filedesc2 = creat(optarg, 0666)) == -1) {
					fprintf(stderr, "error with output file: %s\n", strerror(errno));
					exit(2);
				}
				else {
					dup2(filedesc2, 1);
					close(filedesc2);
				}
				break;
			case 's':
				*p = 'L';
				break;
			case 'c':
				signal(SIGSEGV, handler);
				break;
			case 'd':
				signal(SIGSEGV, SIG_DFL);
				break;
			default:
				fprintf(stderr, "Please use spcified options\n");
				exit(3);
		}
	}

	char ch;
	int r;

	while ((r = read(0, &ch, sizeof(char))) > 0) {
		if (r < 0) {
			fprintf(stderr, "read error: %s\n", strerror(errno));
			exit(1);
		}
		if (write(1, &ch, sizeof(char)) < 0) {
			fprintf(stderr, "write error: %s\n", strerror(errno));
			exit(2);
		}
	}
	exit(0);
	close(0);
	close(1);
}
