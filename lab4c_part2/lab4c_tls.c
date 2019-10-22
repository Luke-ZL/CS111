#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <mraa.h>
#include <poll.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

int button_pressed = 0;
int paused = 0;
int log_flag = 0;
FILE* fd;
int cmd_size = 0;
int period = 1;
struct timeval tv;
struct tm *now;
char scale_opt = 'F';
mraa_aio_context sensor;
//mraa_gpio_context button;
const int B = 4275;
const int R0 = 100000; //taken from http://wiki.seeedstudio.com/Grove-Temperature_Sensor_V1.2/
char buffer[1000];
char cmd[100];
int exitcode = 0;

int port;
char* id;
struct hostent* server;
char* host;
struct sockaddr_in address;
int sockfd;
SSL *ssl;
SSL_CTX* context;

void button_func() {
	button_pressed = 1;
}

float get_temp() {
	int reading = mraa_aio_read(sensor);
	float R = 1023.0 / ((float) reading) - 1.0;
	R = R0 * R;
	float temperature = 1.0 / (log(R / R0) / B + 1 / 298.15) - 273.15; //taken from http://wiki.seeedstudio.com/Grove-Temperature_Sensor_V1.2/

	if (scale_opt == 'F')
		return (temperature * 9 / 5 + 32);
	else
		return temperature;
}

void print_sample() {
	float reading = get_temp();
	now = localtime(&(tv.tv_sec));
	sprintf(buffer, "%02d:%02d:%02d %.1f\n", now->tm_hour, now->tm_min,
			now->tm_sec, reading);
	//fputs(buffer, stdout);
	SSL_write(ssl, buffer, strlen(buffer));
	if (log_flag)
		fputs(buffer, fd);
}

void process_cmd() {
	if (strncmp(cmd, "SCALE=", 6) == 0) {
		if (cmd[6] == 'F')
			scale_opt = 'F';
		else if (cmd[6] == 'C')
			scale_opt = 'C';
		else {
			fprintf(stderr, "Please use specified options.\n");
			exitcode = 1;
		}
	} else if (strncmp(cmd, "PERIOD=", 7) == 0) {
		char* temp_cmd = cmd + 7;
		period = atoi(temp_cmd);
	} else if (strcmp(cmd, "STOP") == 0) {
		paused = 1;
	} else if (strcmp(cmd, "START") == 0) {
		paused = 0;
	} else if (strcmp(cmd, "OFF") == 0) {
		button_pressed = 1;
	} else if (!(strncmp(cmd, "LOG ", 4) == 0)) {
		fprintf(stderr, "Please use specified options.\n");
		exitcode = 1;
	}
	if (log_flag) {
		cmd[cmd_size] = '\n';
		cmd[cmd_size + 1] = (char) 0;
		fputs(cmd, fd);
	}
}

void process_buffer(int size) {

	for (int i = 0; i < size; i++) {
		if (buffer[i] == '\n') {
			cmd[cmd_size] = (char) 0;
			process_cmd();
			cmd_size = 0;
		} else {
			cmd[cmd_size] = buffer[i];
			cmd_size++;
		}
	}
}

void socket_func() {
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Socket function call error.\n");
		exit(1);
	}
	if ((server = gethostbyname(host)) == NULL) {
		fprintf(stderr, "gethostbyname function call error.\n");
		exit(1);
	}
	address.sin_family = AF_INET;
	memcpy((char *) &address.sin_addr.s_addr, (char *) server->h_addr_list[0],
			server->h_length);
	address.sin_port = htons(port);
	if (connect(sockfd, (struct sockaddr*) &address, sizeof(address)) < 0) {
		fprintf(stderr, "Connect function call error.\n");
		exit(1);
	}

	SSL_set_fd(ssl, sockfd);
	if (SSL_connect(ssl) != 1) {
		fprintf(stderr, "SSL_set_fd function error.\n");
		exit(1);
	}

	sprintf(buffer, "ID=%s\n", id);
	SSL_write(ssl, buffer, strlen(buffer));
	if (log_flag)
		fputs(buffer, fd);
}

void ssl_func() {
	SSL_load_error_strings();
	if (SSL_library_init() < 0) {
		fprintf(stderr, "SLL init error.\n");
		exit(1);
	}
	OpenSSL_add_all_algorithms();

	if ((context = SSL_CTX_new(TLSv1_client_method())) == NULL) {
		fprintf(stderr, "SSL_CTX_new function call error.\n");
		exit(1);
	}

	ssl = SSL_new(context);
}

int main(int argc, char* argv[]) {
	//parse arg
	struct option options[] = { { "period", required_argument, NULL, 'p' }, {
			"scale", required_argument, NULL, 's' }, { "log",
	required_argument, NULL, 'l' }, { "id",
	required_argument, NULL, 'i' }, { "host",
	required_argument, NULL, 'h' }, { 0, 0, 0, 0 } };
	int c;
	while (optind < argc) {
		if ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
			switch (c) {
			case 'p':
				period = (int) atoi(optarg);
				break;
			case 's':
				if (strlen(optarg) != 1) {
					fprintf(stderr, "Argument for SCALE= must be C or F\n");
					exit(1);
				} else if ((optarg[0] != 'C') && (optarg[0] != 'F')) {
					fprintf(stderr, "Argument for SCALE= must be C or F\n");
					exit(1);
				} else
					scale_opt = optarg[0];
				break;
			case 'l':
				log_flag = 1;
				if ((fd = fopen(optarg, "wb")) == NULL) { //need to specify mode 0666 for creation
					fprintf(stderr, "unable to create the specified file %s\n",
							optarg);
				}
				break;
			case 'i':
				id = optarg;
				break;
			case 'h':
				host = optarg;
				break;
			default:
				fprintf(stderr, "Please use specified options.\n");
				exit(1);
			}
		} else {
			port = atoi(argv[optind]);
			optind++;
		}
	}
	ssl_func();
	socket_func();

	//init
	sensor = mraa_aio_init(1);
	//button = mraa_gpio_init(60);
	//mraa_gpio_dir(button, MRAA_GPIO_IN);
	//mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &button_func, NULL);
	struct pollfd poll_stdin = { sockfd, POLLIN, 0 };
	time_t next_cycle_time = 0;

	//body
	while (!button_pressed) {
		if (gettimeofday(&tv, 0)) {
			fprintf(stderr, "gettimeofday function failed.\n");
			exit(1);
		}

		if ((!paused) && (tv.tv_sec >= next_cycle_time)) {
			print_sample();
			next_cycle_time = tv.tv_sec + period;
		}

		int ret1, ret2;
		if ((ret1 = poll(&poll_stdin, 1, 0)) < 0) {
			fprintf(stderr, "poll function call error.\n");
			exit(1);
		} else if (ret1 > 0) {
			if (poll_stdin.revents & POLLIN) {
				if ((ret2 = SSL_read(ssl, buffer, 1000)) < 0) {
					fprintf(stderr, "read function call error.\n");
					exit(1);
				}
			}
			process_buffer(ret2);
		}
	}
	now = localtime(&(tv.tv_sec));
	sprintf(buffer, "%02d:%02d:%02d SHUTDOWN\n", now->tm_hour, now->tm_min,
			now->tm_sec);
	SSL_write(ssl, buffer, strlen(buffer));
	//fputs(buffer, stdout);
	if (log_flag)
		fputs(buffer, fd);

	mraa_aio_close(sensor);
	//mraa_gpio_close(button);
	exit(exitcode);
}
