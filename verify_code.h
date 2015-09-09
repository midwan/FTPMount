/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

#if defined(VERIFY) || defined(MINI_VERIFY)
unsigned char verify_buffer[200];

static int make_int(int pos, int n)
{
	if (!n) return pos;

	pos = make_int(pos, n / 10);
	verify_buffer[pos] = (n % 10) + '0';

	return pos + 1;
}

static int make_int_f(int pos, int n)
{
	if (n == 0) {
		verify_buffer[pos++] = '0';
		return pos;
	}

	if (n < 0) {
		verify_buffer[pos++] = '-';
		return make_int(pos, -n);
	}
	else {
		return make_int(pos, n);
	}
}

static int make_string(int pos, char *s)
{
	strcpy((char*)&verify_buffer[pos], s);
	return (int)(pos + strlen(s));
}

static int make_char(int pos, char c)
{
	verify_buffer[pos] = c;

	return pos + 1;
}

void SAVEDS verify_alert(char *file, int line, char *a, char *b)
{
	int i;

	Forbid();

	i = 0;

	i = make_char(i, 0);
	i = make_char(i, 10);
	i = make_char(i, 15);
	i = make_string(i, file);
	i = make_string(i, " line ");
	i = make_int(i, line);
	i = make_string(i, ": ");
	i = make_string(i, a);
	i = make_string(i, " is not a ");
	i = make_string(i, b);
	i = make_char(i, 0);
	i = make_char(i, 0);

//	sprintf(verify_buffer, "%c%c%cVerify alert in %s line %d: %s is not a %s%c%c", 0, 10, 15, file, line, a, b, 0, 0);
	DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 40);
	Permit();
}

void SAVEDS truth_alert(char *file, int line, char *a)
{
	int i;

	Forbid();

	i = 0;

	i = make_char(i, 0);
	i = make_char(i, 10);
	i = make_char(i, 15);
	i = make_string(i, file);
	i = make_string(i, " line ");
	i = make_int(i, line);
	i = make_string(i, ": ");
	i = make_string(i, a);
	i = make_string(i, " is not TRUE");
	i = make_char(i, 0);
	i = make_char(i, 0);

//	sprintf(verify_buffer, "%c%c%c%s line %d: %s is not TRUE%c%c", 0, 10, 15, file, line, a, 0, 0);
	DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 40);
	Permit();
}

void SAVEDS false_alert(char *file, int line, char *a)
{
	int i;

	Forbid();

	i = 0;

	i = make_char(i, 0);
	i = make_char(i, 10);
	i = make_char(i, 15);
	i = make_string(i, file);
	i = make_string(i, " line ");
	i = make_int(i, line);
	i = make_string(i, ": ");
	i = make_string(i, a);
	i = make_string(i, " is not FALSE");
	i = make_char(i, 0);
	i = make_char(i, 0);

//	sprintf(verify_buffer, "%c%c%c%s line %d: %s is not FALSE%c%c", 0, 10, 15, file, line, a, 0, 0);
	DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 40);
	Permit();
}

void SAVEDS int_alert(char *file, int line, char *a, int b)
{
	int i;

	Forbid();
	i = 0;

	i = make_char(i, 0);
	i = make_char(i, 10);
	i = make_char(i, 15);
	i = make_string(i, file);
	i = make_string(i, " line ");
	i = make_int(i, line);
	i = make_string(i, ": ");
	i = make_string(i, a);
	i = make_char(i, '=');
	i = make_int_f(i, b);
	i = make_char(i, 0);
	i = make_char(i, 0);

//	sprintf(verify_buffer, "%c%c%c%s line %d: %s=%d%c%c", 0, 10, 15, file, line, a, b, 0, 0);
	DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 40);
	Permit();
}

void SAVEDS string_alert(char *file, int line, char *a, char *b)
{
	int i;

	Forbid();

	i = 0;

	i = make_char(i, 0);
	i = make_char(i, 10);
	i = make_char(i, 15);
	i = make_string(i, file);
	i = make_string(i, " line ");
	i = make_int(i, line);
	i = make_string(i, ": ");
	i = make_string(i, a);
	i = make_char(i, '=');
	i = make_string(i, b);
	i = make_char(i, 0);
	i = make_char(i, 0);

//	sprintf(verify_buffer, "%c%c%c%s line %d: %s=%s%c%c", 0, 10, 15, file, line, a, b, 0, 0);
	DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 40);
	Permit();
}

/* struct List memlist; */

void SAVEDS track_init(void)
{
	struct Task *me;

	me = FindTask(0);
	truth(me != 0);
	truth(me->tc_UserData == 0);
	me->tc_UserData = 0;
}

void * SAVEDS track_malloc(int size, unsigned long type, unsigned long flags, char *file, int line, char *a, char *b)
{
	struct bink *block;
	int i;

	if (size <= 0 || size > 100000) {  /* the positive is arbitrary */
		Forbid();

		i = 0;

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 15);
		i = make_string(i, "Illegal malloc size of ");
		i = make_int_f(i, size);
		i = make_string(i, " in ");
		i = make_string(i, file);
		i = make_string(i, " line ");
		i = make_int(i, line);
		i = make_string(i, ": allocating ");
		i = make_string(i, a);
		i = make_string(i, " to be ");
		i = make_string(i, b);
		i = make_char(i, 0);
		i = make_char(i, 0);

//		sprintf(verify_buffer, "%c%c%cIllegal malloc size of %d in %s line %d: allocating %s to be %s%c%c", 0, 10, 15, size, file, line, a, b, 0, 0);
		DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 40);
		Permit();
		return 0;
	}

	block = (struct bink *)AllocMem(size + sizeof(struct bink), flags);
	if (!block) {
		Forbid();

		i = 0;

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 15);
		i = make_string(i, "Malloc failure in ");
		i = make_int_f(i, size);
		i = make_string(i, " in ");
		i = make_string(i, file);
		i = make_string(i, " line ");
		i = make_int(i, line);
		i = make_string(i, ": allocating ");
		i = make_string(i, a);
		i = make_string(i, " to be ");
		i = make_string(i, b);
		i = make_char(i, 0);
		i = make_char(i, 0);

//		sprintf(verify_buffer, "%c%c%cMalloc failure in %s line %d: allocating %s to be %s%c%c", 0, 10, 15, file, line, a, b, 0, 0);
		DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 40);
		Permit();
		return 0;
	}

	block->prev = (struct bink **)&(FindTask(0)->tc_UserData);
	block->next = *(block->prev);
	*(block->prev) = block;
	if (block->next) block->next->prev = &(block->next);

	block->type = type;
	block->size = size;
	block->file = file;
	block->line = line;
	block->typename = b;

	return (void *)((unsigned char *)block + sizeof(struct bink));
}

void SAVEDS track_free(void *block, unsigned long type, char *file, int line, char *a, char *b)
{
	int i;
	struct bink *rblock;
	rblock = (struct bink *)((unsigned char *)block - sizeof(struct bink));

	if (rblock->type != type) {
		Forbid();

		i = 0;

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 15);
		i = make_string(i, "Illegal FREE in ");
		i = make_string(i, file);
		i = make_string(i, " line ");
		i = make_int(i, line);
		i = make_char(i, ':');
		i = make_char(i, 0);
		i = make_char(i, 1);

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 25);
		i = make_string(i, "freeing ");
		i = make_string(i, a);
		i = make_string(i, " which should be ");
		i = make_string(i, b);
		i = make_char(i, 0);
		i = make_char(i, 1);

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 35);
		i = make_string(i, "Allocated in ");
		i = make_string(i, rblock->file);
		i = make_string(i, " line ");
		i = make_int(i, rblock->line);
		i = make_char(i, 0);
		i = make_char(i, 0);

/*		sprintf(verify_buffer, "%c%c%cIllegal FREE in %s line %d:%c%c"
					 "%c%c%cfreeing %s which should be %s%c%c"
					 "%c%c%cAllocated in %s line %d%c%c",
					  0, 10, 15, file, line, 0, 1,
					  0, 10, 25, a, b, 0, 1,
					  0, 10, 35, rblock->file, rblock->line, 0, 0); */
		DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 50);
		Permit();
		return;
	}

	*(rblock->prev) = rblock->next;
	if (rblock->next) rblock->next->prev = rblock->prev;

	rblock->type = 0xffffffff;
	FreeMem(rblock, rblock->size + sizeof(struct bink));
}

void SAVEDS track_disown(void *block, unsigned long type, char *file, int line, char *a, char *b)
{
	int i;
	struct bink *rblock;
	rblock = (struct bink *)((unsigned char *)block - sizeof(struct bink));

	if (rblock->type != type) {
		Forbid();

		i = 0;

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 15);
		i = make_string(i, "Illegal DISOWN in ");
		i = make_string(i, file);
		i = make_string(i, " line ");
		i = make_int(i, line);
		i = make_char(i, ':');
		i = make_char(i, 0);
		i = make_char(i, 1);

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 25);
		i = make_string(i, "disowning ");
		i = make_string(i, a);
		i = make_string(i, " which should be ");
		i = make_string(i, b);
		i = make_char(i, 0);
		i = make_char(i, 1);

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 35);
		i = make_string(i, "Allocated in ");
		i = make_string(i, rblock->file);
		i = make_string(i, " line ");
		i = make_int(i, rblock->line);
		i = make_char(i, 0);
		i = make_char(i, 0);

		DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 50);
		Permit();
		return;
	}

	*(rblock->prev) = rblock->next;
	if (rblock->next) rblock->next->prev = rblock->prev;
}

void SAVEDS track_adopt(void *block, unsigned long type, char *file, int line, char *a, char *b)
{
	int i;
	struct bink *rblock;
	rblock = (struct bink *)((unsigned char *)block - sizeof(struct bink));

	if (rblock->type != type) {
		Forbid();

		i = 0;

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 15);
		i = make_string(i, "Illegal ADOPT in ");
		i = make_string(i, file);
		i = make_string(i, " line ");
		i = make_int(i, line);
		i = make_char(i, ':');
		i = make_char(i, 0);
		i = make_char(i, 1);

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 25);
		i = make_string(i, "adopting ");
		i = make_string(i, a);
		i = make_string(i, " which should be ");
		i = make_string(i, b);
		i = make_char(i, 0);
		i = make_char(i, 1);

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 35);
		i = make_string(i, "Allocated in ");
		i = make_string(i, rblock->file);
		i = make_string(i, " line ");
		i = make_int(i, rblock->line);
		i = make_char(i, 0);
		i = make_char(i, 0);

		DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 50);
		Permit();
		return;
	}

	rblock->prev = (struct bink **)&(FindTask(0)->tc_UserData);
	rblock->next = *(rblock->prev);
	*(rblock->prev) = rblock;
	if (rblock->next) rblock->next->prev = &(rblock->next);
}

void SAVEDS track_check(void)
{
	struct bink *a;
	int i;

	for (a = (struct bink *)FindTask(0)->tc_UserData; a; a = a->next) {
		Forbid();

		i = 0;

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 15);
		i = make_char(i, '(');
		i = make_string(i, FindTask(0)->tc_Node.ln_Name);
		i = make_string(i, ") Unfreed block of type ");
		i = make_string(i, a->typename);
		i = make_char(i, 0);
		i = make_char(i, 1);

		i = make_char(i, 0);
		i = make_char(i, 10);
		i = make_char(i, 25);
		i = make_string(i, "Allocated in ");
		i = make_string(i, a->file);
		i = make_string(i, " line ");
		i = make_int(i, a->line);
		i = make_char(i, 0);
		i = make_char(i, 0);

/*		sprintf(verify_buffer, "%c%c%c(%s) Unfreed block of type %s%c%c"
					 "%c%c%cAllocated in %s line %d%c%c",
					  0, 10, 15, FindTask(0)->tc_Node.ln_Name, a->typename, 0, 1,
					  0, 10, 25, a->file, a->line, 0, 0); */
		DisplayAlert(RECOVERY_ALERT, (CONST_STRPTR)verify_buffer, 45);
		a->type = ~0;
		Permit();
/*		FreeMem(a, a->size); */
	}

	FindTask(0)->tc_UserData = 0;
}
#endif
