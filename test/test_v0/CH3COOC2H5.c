#include "LA-MAT.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#ifndef __MAT_CMD_H__
#define __MAT_CMD_H__
extern const char* cmd[];
extern const char* error_info[];

enum error_type {MATH_ERROR, SYNTAX_ERROR, INVALID_CMD, STACK_OVERFLOW, MAT_UNDEF};

int cmd_anal(char* scmd);
int calc_expr_anal(char* expr);
int cmd_perf(char* scmd);
#endif

#ifndef __MAT_DATA_H__
#define __MAT_DATA_H__

#define STMAX 10  /* stack max */
typedef struct mat_node {
	char id;
	matrix mat;
	struct mat_node* next;
}mat_node;

extern mat_node* list_head;
extern int stack_pt;
extern matrix stack_buf[STMAX];

mat_node* list_add_node(char id, matrix mat);
int list_del_node(char id);
void list_clear(void);
mat_node* list_search(char id);
int stack_push(matrix mat);
matrix stack_pop(void);
void stack_clear(int rm);
#endif


mat_node* list_head = NULL;
int stack_pt = 0;  /* stack pointer */
matrix stack_buf[STMAX];  /* matrix stack */

/* add a node to the list */
/* if succeed, return the location; or, NULL */
mat_node* list_add_node(char id, matrix mat)
{
	mat_node* p = list_head;
	if (p == NULL) /* the first node */
	{
		list_head = (mat_node*)malloc(sizeof(mat_node));
		if (list_head == NULL)
			return NULL;
		list_head->id = id;
		list_head->mat = mat;
		list_head->next = NULL;
		return list_head;
	}
	for (; p->next != NULL; p = p->next)
		;
	p->next = (mat_node*)malloc(sizeof(mat_node));
	if ((p = p->next) == NULL)
		return NULL;
	p->id = id;
	p->mat = mat;
	p->next = NULL;
	return p;
}

/* delete a node */
/* succeed: 0; or: 1 */
int list_del_node(char id)
{
	mat_node* p = list_head;
	mat_node* q = NULL;
	if (p == NULL)
		return 1;
	if (p->id == id)  /* the head was matched */
	{
			list_head = p->next;
			mat_delete(&(p->mat));
			free(p);
			return 0;
	}
	for (; p->next != NULL; p = p->next)
	{
		if (p->next->id == id)
		{
			mat_delete(&(p->next->mat));
			q = p->next->next;
			free(p->next);
			p->next = q;
			return 0;
		}
	}
	return 1;  /* not found */
}

/* delete all nodes */
void list_clear()
{
	while (list_head != NULL)
		list_del_node(list_head->id);
}

/* search a node and return its location */
mat_node* list_search(char id)
{
	mat_node* p = list_head;
	if (p == NULL)
		return NULL;
	for (; p != NULL; p = p->next)
	{
		if (p->id == id)
			return p;
	}
	return NULL;  /* not found */
}

/* push a matrix into the stack */
/* succeed: 0; overflow: 1 */
int stack_push(matrix mat)
{
	matrix mtp = MAT_NULL;
	if (stack_pt >= STMAX)
		return 1;  /* stack overflow */
	mtp = mat_create(mat.row, mat.col);
	mat_copy(&mtp, &mat); /* make a cpoy */
	stack_buf[stack_pt++] = mtp;
	return 0;
}

/* pop a matrix */
matrix stack_pop()
{
	if (stack_pt <= 0)
		return MAT_NULL;
	else
		return stack_buf[--stack_pt];
}

/* rm=0: clear stack and release all matrixes; rm=1: remain 1 matrix */
void stack_clear(int rm)
{
	while (stack_pt > rm)
		mat_delete(stack_buf+(--stack_pt));
}

/* analyse command */
const char* cmd[] = {"exit","clear","del","det","inv","elim"};
const char* error_info[] = {
	"**Error: math error\n",
	"**Error: syntax error\n",
	"**Error: invalid command\n",
	"**Error: stack overflow\n",
	"**Error: matrix undefined\n"
};
/* analyse command:
-4: math error;
-3: syntax error;
-2: invalid cmd;
-1: a=...;
0+: match cmd[];
'm': expression;
'n': expr, but stack overflow*/
int cmd_anal(char* scmd)
{
	unsigned int i, j;
	unsigned int ct = 0;
	/*int rt;  return value */
	/*char id;*/
	matrix mat = MAT_NULL;
	//mat_node* mat_sea = NULL; /* search result */
	while (isspace(*scmd))
		scmd++;
	switch (calc_expr_anal(scmd))
	{
	case 0:
		return 'm';
		break;
	case 1:
		return 'n';
		break;
	case 2:
		return -4;
		break;
	case 3:
		break;
	}
	if (*scmd == '[')
		return '[';
	if (!isalpha(*scmd))
		return -2;  /* invalid command */
	/*if (*(scmd + 1) == '\0')
		return (int)(*scmd);  ASCII */
	for (i = 0; i == 0 || scmd[i - 1] != '\0'; i++)
	{
		if (ct < 2)
		{
			if (!isspace(scmd[i]))
				ct++;
			if (scmd[i] == '=')
			{
				if (ct == 2 && isalpha(*scmd)) /* after '=' */
				{
					switch (cmd_anal(scmd + i + 1))
					{
					case 'm':
						mat = mat_create(stack_buf->row, stack_buf->col);
						mat_copy(&mat, stack_buf);
						stack_clear(0);
						list_del_node(*scmd);
						list_add_node(*scmd, mat);
						return -1;
						break;
					case 'n':
						return 'n';
						break;
					case '[':
						mat = string_mat(scmd + i + 1);
						if (mat.dat == NULL)
							return -3; /* syntax error */
						list_del_node(*scmd);
						list_add_node(*scmd, mat);
						return -1;
						break;
					case 3: /* det */
						while (isspace(scmd[i + 1]))
							i++;
						while (isalpha(scmd[i + 1]))
							i++;
						switch (cmd_anal(scmd + i + 1))
						{
						case 'm':
							mat = mat_create(1, 1);
							mat.dat[0][0] = mat_det(stack_buf[0]);
							stack_clear(0);
							list_del_node(*scmd);
							list_add_node(*scmd, mat);
							return -1;
							break;
						case 'n':
							return 'n';
							break;
						default:
							return -3;
							break;
						}
						break;
					case 4: /* inv */
						while (isspace(scmd[i + 1]))
							i++;
						while (isalpha(scmd[i + 1]))
							i++;
						switch (cmd_anal(scmd + i + 1))
						{
						case 'm':
							mat = mat_create(stack_buf->row, stack_buf->col);
							mat_copy(&mat, stack_buf);
							stack_clear(0);
							if ((mat = (mat_inv(mat))).dat != NULL)
							{
								list_del_node(*scmd);
								list_add_node(*scmd, mat);
								return -1;
							}
							else
								return -4;
							break;
						case 'n':
							return 'n';
							break;
						default:
							return -3;
							break;
						}
						break;
					default:
						return -3;
						break;
					}
				}
				else
					return -2; /* invalid command */
			}
		}
		else
		{
			if (isspace(scmd[i]) || scmd[i] == '\0')
			{
				for (j = 0; j < sizeof(cmd) / sizeof(const char*); j++)
				{
					if (strncmp(scmd, cmd[j], i) == 0)
						break;
				}
				switch (j)
				{
				case 0:case 1:
					return j;
					break;
				case 2:case 3:case 4:case 5:
					if (scmd[i] == '\0')
						return -3;
					return j;
					break;
				default:
					return -2;
					break;
				}
			}
		}
	}
	return -2;
}

/* analyse calc expression */
/* 0: succeed; 1: stack overflow; 2: math error; 3: invalid expr */
int calc_expr_anal(char* expr)
{
	mat_node* mat_sea = NULL;  /* search result */
	matrix mat1 = MAT_NULL, mat2 = MAT_NULL;
	matrix mat3 = MAT_NULL;
	stack_clear(0);
	while (isspace(*expr))
		expr++;
	for (; *expr != '\0'; expr++)
	{
		if (isspace(*expr))
			continue;
		if (isalpha(*expr))  /* matrix */
		{
			if ((mat_sea = list_search(*expr)) == NULL)
			{
				stack_clear(0); /* clear stack */
				return 3;
			}
			if (stack_push(mat_sea->mat) == 1)
			{
				stack_clear(0);
				return 1; /* stack overflow */
			}
			continue;
		}
		switch (*expr)
		{
		case '+':
			if (stack_pt < 2)
			{
				stack_clear(0); /* clear stack */
				return 3;
			}
			mat1 = stack_pop();
			mat2 = stack_pop();
			mat3 = mat_plus(mat1, mat2);
			mat_delete(&mat1);
			mat_delete(&mat2);
			if (mat3.dat == NULL) /* math error */
			{
				stack_clear(0);
				return 2;
			}
			stack_push(mat3);
			mat_delete(&mat3);
			break;
		case '-':
			if (stack_pt < 2)
			{
				stack_clear(0); /* clear stack */
				return 3;
			}
			mat1 = stack_pop();
			mat2 = stack_pop();
			mat3 = mat_minu(mat2, mat1);
			mat_delete(&mat1);
			mat_delete(&mat2);
			if (mat3.dat == NULL) /* math error */
			{
				stack_clear(0);
				return 2;
			}
			stack_push(mat3);
			mat_delete(&mat3);
			break;
		case '*':
			if (stack_pt < 2)
			{
				stack_clear(0); /* clear stack */
				return 3;
			}
			mat1 = stack_pop();
			mat2 = stack_pop();
			mat3 = mat_mult(mat2, mat1);
			if (mat3.dat == NULL) /* math error */
			{
				if (mat2.row == 1 && mat2.col == 1) /* times */
				{
					mat3 = mat_times(mat1, mat2.dat[0][0]);
				}
				else
				{
					stack_clear(0);
					return 2;
				}
			}
			stack_push(mat3);
			mat_delete(&mat1);
			mat_delete(&mat2);
			mat_delete(&mat3);
			break;
		case '\'': /* trans */
			if (stack_pt < 1)
			{
				stack_clear(0);
				return 3;
			}
			mat1 = stack_pop();
			mat3 = mat_tran(mat1);
			stack_push(mat3);
			mat_delete(&mat1);
			mat_delete(&mat3);
			break;
		default:
			stack_clear(0);
			return 3;
			break;
		}
	}
	if (stack_pt == 1)
		return 0;
	else if (stack_pt > 1)
		stack_clear(0);
	return 3;
}

/* perform a command */
/* 0: success; 1: exit; 2: error */
int cmd_perf(char* scmd)
{
	int cmd_anal_rt = cmd_anal(scmd);
	matrix mtp = MAT_NULL;
	mat_node* mat_node_temp = NULL;
	char* expr = scmd;
	struct RN rtp;
	unsigned int utp;  /* tp: temp */
	switch (cmd_anal_rt)
	{
	case -4:
		printf(error_info[MATH_ERROR]);
		return 2;
		break;
	case -3:
		printf(error_info[SYNTAX_ERROR]);
		return 2;
		break;
	case -2:
		printf(error_info[INVALID_CMD]);
		return 2;
		break;
	case 'n':
		printf(error_info[STACK_OVERFLOW]);
		return 2;
		break;
	case 0: /* exit */
		return 1;
		break;
	case 1: /* clear */
		list_clear();
		return 0;
		break;
	case -1:
		while (isspace(*scmd))
			scmd++;
		mtp = list_search(*scmd)->mat;
		printf("%c =\n", *scmd);
		mat_print(mtp);
		return 0;
		break;
	case 'm':
		printf("ans =\n");
		mat_print(stack_buf[0]);
		stack_clear(0);
		return 0;
		break;
	case 2:case 3:case 4:case 5:
		while (isspace(*expr++))
			;
		while (isalpha(*expr++))
			;
		while (isspace(*expr))
			expr++;
		if (cmd_anal_rt == 3 || cmd_anal_rt == 4)
			switch (calc_expr_anal(expr))
			{
			case 1:
				printf(error_info[STACK_OVERFLOW]);
				return 2;
				break;
			case 2:
				printf(error_info[MATH_ERROR]);
				return 2;
				break;
			case 3:
				printf(error_info[INVALID_CMD]);
				return 2;
				break;
			}
		break;
	}
	switch (cmd_anal_rt)
	{
	case 2: /* del */
		if (list_del_node(*expr) != 0)
		{
			printf(error_info[MAT_UNDEF]);
			return 2;
		}
		return 0;
		break;
	case 3: /* det */
		rtp = mat_det(stack_buf[0]);
		stack_clear(0);
		printf("ans =\n    ");
		DisplayRN(rtp);
		printf("\n");
		return 0;
		break;
	case 4: /* inv */
		mtp = mat_inv(stack_buf[0]);
		stack_clear(0);
		printf("ans =\n");
		mat_print(mtp);
		mat_delete(&mtp);
		return 0;
		break;
	case 5: /* elim */
		if ((mat_node_temp = list_search(*expr)) == NULL)
		{
			printf(error_info[MAT_UNDEF]);
			return 2;
		}
		mtp = mat_node_temp->mat;
		utp = mat_gauss_elim(&mtp, 'r');
		printf("ans =\n");
		mat_print(mtp);
		printf("rank(%c) = %u\n", *expr, utp);
		return 0;
		break;
	}
	return -1;
}


int main(int argc, char *argv[])
{
#define INMAX 1001  /* input max length */
	char s[INMAX];
	unsigned int cmd_count = 0;
	unsigned int in_count = 0;
	int cylim_l, cylim_h;
	unsigned int get_line(char* str, unsigned int len_max);

	if (argc >= 3 && (cylim_l = atoi(argv[1])) >= 0 &&
		(cylim_h = atoi(argv[2])) >= cylim_l)
		sys_set_cylim(cylim_l, cylim_h);
	else
		/* Default location */
		sys_set_cylim(0, 13);

	while (++cmd_count)
	{
		printf("CH3COOC2H5:%u> ", cmd_count);
		in_count = get_line(s, INMAX - 1);
		if (in_count >= INMAX - 1)
		{
			if (getchar() != '\n')
			{
				while (getchar() != '\n')
					;
				printf("**Error: input too long\n");
				continue;
			}
		}
		if (cmd_perf(s) == 1)
			return 0;
	}
	return 0;
}

unsigned int get_line(char* str, unsigned int len_max)
{
	unsigned int i = 0;
	while ((*str++ = getchar()) != '\n' && ++i < len_max)
		;
	*(str - 1) == '\n' ? (*(str - 1) = '\0') : (*str = '\0');
	return i;
}
