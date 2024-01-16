/*
 * This program is used to test reliable data transmission with GNTP.
 * Usage: nrecv -l[LEN] [-h] [-c] [-ch] [-p[PLOC]]
 * [LEN] is the number of bytes to be received;
 * [PLOC] is the bottom of printing area;
 * `-h` means that print data in hex format;
 * `-c` means that print data in char format;
 * `-ch` means that enable Chinese display.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int print_location = 13;
int recv_len;

enum Format {NONE, HEX, CHAR, ZHCN};

enum Format print_format = NONE;

#define LINEMAX_CHAR 48
#define LINEMAX_HEX 16

uint16_t fletcher16(const uint8_t *data, int len);

int main(int argc, char *argv[])
{
	char *rbuf;
	int rt;
	int i, j;

	while (--argc)
	{
		++argv;
		if (strcmp("-c", *argv) == 0)
			print_format = CHAR;
		if (strcmp("-h", *argv) == 0)
			print_format = HEX;
		else if (strcmp("-ch", *argv) == 0)
			print_format = ZHCN;
		else if (strncmp("-l", *argv, 2) == 0)
			recv_len = atoi(*argv + 2);
		else if (strncmp("-p", *argv, 2) == 0)
			print_location = atoi(*argv + 2);
	}
	if (print_location < 0 || recv_len <= 0)
	{
		printf("Usage: nrecv -l[LEN] [-h] [-c] [-ch] [-p[PLOC]]\n");
		return 1;
	}
	sys_set_cylim(1, print_location);
	if ((rbuf = malloc(recv_len + 16)) == NULL)
	{
		printf("**Failed to allocate memory!\n");
		return 2;
	}
	printf("Waiting for %d bytes data...(测试)", recv_len);
	while ((rt = sys_net_recv_stream((void*)rbuf, recv_len)) > 0)
	{
		sys_move_cursor(0, 0);
		printf("%d bytes of %u B file are received using GNTP:\n", rt, *(uint32_t*)rbuf);
		if (print_format == ZHCN)
		{
			int sc;
			int temp;
			for (i = 4; i < recv_len; i += sc + 1)
			{
				for (sc = 0; rbuf[i + sc] != '\n' && rbuf[i + sc] != '\r' && rbuf[i + sc] != '\0'; ++sc)
					;
				temp = rbuf[i + sc];
				rbuf[i + sc] = '\0';
				printf("%s\n", rbuf + i);
				/* Restore the byte as it will influence Fletcher! */
				rbuf[i + sc] = temp;
			}
		}
		else if (print_format == CHAR)
		{
			for (i = j = 0; i < recv_len; ++i)
			{
				int c = (unsigned char)rbuf[i];
				if (c >= 32 && c <= 126)
				{
					printf("%c", c);
					++j;
				}
				else if (c == '\n' || c == '\r' || j >= LINEMAX_CHAR)
				{
					printf("\n");
					j = 0;
				}
				else
				{
					printf(".");
					++j;
				}
			}
		}
		else if (print_format == HEX)
		{
			for (i = 0; i < (recv_len + LINEMAX_HEX - 1) / LINEMAX_HEX; ++i)
			{
				for (j = 0; j < LINEMAX_HEX && (i * LINEMAX_HEX + j < recv_len); ++j)
					printf("%02x ", (int)rbuf[i * LINEMAX_HEX + j]);
				printf("\n");
			}
		}
		printf("### The Fletcher sum of the file is %u.\n",
			fletcher16((uint8_t *)rbuf + 4, *(int32_t*)rbuf - 4));
	}
	printf("**Failed to receive %d bytes\n", recv_len);
	return 3;
}

uint16_t fletcher16(const uint8_t *data, int len)
{
	uint16_t sum1 = 0U, sum2 = 0U;
	int i;

	for (i = 0; i < len; ++i)
	{
		sum1 = (sum1 + data[i]) % 0xffU;
		sum2 = (sum2 + sum1) % 0xffU;
	}
	return (sum2 << 8) | sum1;
}