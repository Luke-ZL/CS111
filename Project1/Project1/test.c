#include <stdio.h> 
#include <unistd.h>


int main()
{
	int i = 100;
	write(stdout, "hello = %d", i, 1000);
}