/*
 * cat: read a file and print it.
 * Usage: cat [file_name] -p[PLOC] -n[MAX] -l[STL] -x
 * [PLOC] is the bottom of printing area;
 * [MAX] is the max number to read;
 * [STL] is the start location;
 * -x means that display the content in hex.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define BUF_SIZE 128
#define LINEMAX_HEX 16

const char *fname;
int ploc = 13;
char flag_hex = 0;
int start_loc;
long r_max = 256, r_ymr = 0;

char buf[BUF_SIZE];

int main(int argc, char *argv[])
{
	int rt, fd, len, i, j;

	while (--argc)
	{
		++argv;
		if (strncmp(*argv, "-p", 2) == 0)
			ploc = atoi(*argv + 2);
		else if (strncmp(*argv, "-n", 2) == 0)
			r_max = atol(*argv + 2);
		else if (strcmp(*argv, "-x") == 0)
			flag_hex = 1;
		else if (strncmp(*argv, "-l", 2) == 0)
			start_loc = atoi(*argv + 2);
		else
			fname = *argv;
	}
	sys_move_cursor(0, 0);
	sys_set_cylim(0, ploc);
	if (fname == NULL)
	{
		printf("Usage: cat [file_name] -p[PLOC] -n[MAX] -l[STL] -x\n"
 			"[PLOC] is the bottom of printing area;\n"
 			"[MAX] is the max number to read;\n"
 			"-x means that display the content in hex.\n");
		return 2;
	}
	fd = sys_open(fname, O_RDONLY);
	if (fd < 0)
	{
		printf("cat: Failed to open file %s: %d", fname, fd);
		return 2;
	}
	rt = sys_lseek(fd, start_loc, SEEK_SET);
	if (rt < 0)
	{
		printf("cat: Failed to move pointer to %d: %d", start_loc, rt);
		return 2;
	}
	while (r_ymr < r_max)
	{
		len = sys_read(fd, buf, BUF_SIZE);
		if (len > 0)
		{
			if (len > r_max - r_ymr)
				len = r_max - r_ymr;
			r_ymr += len;
			if (flag_hex == 0)
				sys_screen_write(buf, len);
			else
			{
				for (i = 0; i < (len + LINEMAX_HEX - 1) / LINEMAX_HEX; ++i)
				{
					for (j = 0; j < LINEMAX_HEX && (i * LINEMAX_HEX + j < len); ++j)
						printf("%02x ", (int)buf[i * LINEMAX_HEX + j]);
					printf("\n");
				}
			}
		}
		else if (len < 0)
		{
			printf("### Failed to read: %d", len);
			return 2;
		}
		if (len < BUF_SIZE)
			/* EOF is reached */
			break;
	}
	return 0;
}
