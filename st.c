// vim: syntax=c noet sw=4 sts=0 ts=4 fenc=utf-8
/* st - the simplest module music tracker using PC Speacker for Linux(R).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 *
 * Contributer(s):
 * 		eXerigumo Clanjor <cjxgm@126.com>
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <pthread.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kd.h>


// constants
#define VERSION			"0.13"
#define MAX_PATTERN		0x100
#define MAX_ROW			0x100
/* terminal */
#define COLOR_NORMAL				"\e[0m"
#define COLOR_LOGO					"\e[33m"
#define COLOR_LINE					"\e[0;37;44m"
#define COLOR_LINENO				"\e[0;33;40m"
#define COLOR_PATTERN				"\e[0;37;40m"
#define COLOR_CURRENT_LINE			"\e[1;37;46m"
#define COLOR_CURRENT_LINENO		"\e[1;33;40m"
#define COLOR_CURRENT_PATTERN		"\e[1;37;45m"
#define COLOR_NOTE					"\e[31m"
#define COLOR_FX					"\e[35m"
#define COLOR_FX_INVALID			"\e[30m"
/* pc speacker */
#define CLOCK_TICK_RATE 1193180


// typedefs
typedef   signed int   s32;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;
typedef struct Pattern
{
	u32 nrow;
	u8  notes [MAX_ROW];
	u8  fxs   [MAX_ROW];
	u8  fxvals[MAX_ROW];
}
Pattern;


// global variables
static const char header[] = "sttm0.1x";
static u32     tpr;			// time per row
static u8      mode;		// 0 => note, 1 => fx, 2 => fxv1, 3 => fxv2
static u8      coct;		// current octave
static u8      playing;
static s32     cpat;		// current pattern
static s32     crow;		// current row
static u32     npat;
static Pattern pats[MAX_PATTERN];
/* terminal */
static int term_height;
/* pc speacker */
static int fd_beep;
/* thread */
static pthread_t th_play;


// function declarations
/* utils */
static int  key_to_note(char key);
static void note_to_str(int note, char str[4]);
static float note_to_freq(int note);
/* terminal */
static void term_clear();	// clear screen and reset cursor to upper-left
static void term_reset_cursor();// reset cursor to upper-left
static void term_cbne();	//  enable cbreak, set no echo
static void term_nocbne();	// disable creabk, set    echo
/* ui */
static int  process_key(int key);
static void render();
static void print_line(int i);
static void print_pattern(int i);
static void print_row(int i);
/* pc speacker */
static void beep_init();
static void beep_free();
static void beep(float freq);
/* commands */
static void cmd_create();
static void cmd_play();
static void cmd_stop();
static void cmd_save();
static void cmd_open();
static void cmd_term(int offset);
static void cmd_move_row(int offset);
static void cmd_move_pat(int offset);
static void cmd_row(int offset);
static void cmd_pat(int offset);
/* thread */
static void thread_play();


/**********************************************************************
 *
 * main
 *
 */

int main()
{
	beep_init();

	term_height = 10;
	term_cbne();
	term_clear();

	// init
	cmd_create();

	// command loop
	do render(); while (process_key(getchar()));

	// reset terminal and go back to system
	term_nocbne();
	printf(COLOR_NORMAL "\n");
	beep_free();
	return 0;
}


/**********************************************************************
 *
 * utils
 *
 */

static int key_to_note(char key)
{
	const char table[13] = "qQwWerRtTyYui";
	int i;
	for (i=0; i<13; i++)
		if (key == table[i])
			break;
	if (i == 13) return -1;
	if (i == 12) return 7*12+1;
	return (coct-1) * 12 + i + 1;
}


static void note_to_str(int note, char str[4])
{
	const char ntable[13] = "CCDDEFFGGAAB";
	const char stable[13] = "-#-#--#-#-#-";
	str[3] = 0;
	if (note) {
		note--;
		if (note == 7*12)
			str[0] = str[1] = str[2] = '=';
		else {
			int oct = note / 12;
			int no  = note % 12;
			str[0] = ntable[no];
			str[1] = stable[no];
			str[2] = oct + '1';
		}
	}
	else str[0] = str[1] = str[2] = '.';
}


static float note_to_freq(int note)
{
	if (!note) return -1;
	note--;
	if (note == 7*12) return 0;
	int oct = note / 12;
	int no  = note % 12;
	return 440 * powf(2, (oct-3)+(no-9)/12.0);
}

/**********************************************************************
 *
 * terminal
 *
 */

static void term_clear()
{
	printf("\e[H\e[J");
}


static void term_reset_cursor()
{
	printf("\e[H");
}


static void term_cbne()
{
	system("stty cbreak -echo");
}


static void term_nocbne()
{
	system("stty -cbreak echo");
}


/**********************************************************************
 *
 * ui
 *
 */

static int process_key(int key)
{
	if (playing) {
		cmd_stop();
		return 1;
	}

	int note;
	switch (key) {
		case '\e':	return 0;
		case '\t':	mode = (mode+1) % 4;				break;
		case '\n':	cmd_play();							break;
		case 'c':	cmd_create();						break;
		case 'o':	cmd_open();							break;
		case 's':	cmd_save();							break;
		case ']':	cmd_term(+1);						break;
		case '[':	cmd_term(-1);						break;
		case 'j':	cmd_move_row(+1);					break;
		case 'k':	cmd_move_row(-1);					break;
		case 'l':	cmd_move_pat(+1);					break;
		case 'h':	cmd_move_pat(-1);					break;
		case '=':	cmd_row(+1);						break;
		case '-':	cmd_row(-1);						break;
		case '+':	cmd_pat(+1);						break;
		case '_':	cmd_pat(-1);						break;
		default:
			if (mode == 0 && (note = key_to_note(key)) != -1) {
				pats[cpat].notes[crow] = note;
				beep(note_to_freq(note));
				usleep(1000*100);
				beep(0);
				cmd_move_row(+1);
			}
			else if (mode == 0 && key == ' ') {
				pats[cpat].notes[crow] = 0;
				cmd_move_row(+1);
			}
			else if (mode != 0 && key == ' ') {
				pats[cpat].fxs[crow] = 0;
				pats[cpat].fxvals[crow] = 0;
			}
			else if (mode == 0 && key >= '1' && key <= '7')
				coct = key - '0';
			else if (mode == 1 && isdigit(key))
				pats[cpat].fxs[crow] = key - '0' + 1;
			else if (mode == 2 && isxdigit(key)) {
				if (key >= 'a') key -= 'a' - 10;
				else if (key >= 'A') key -= 'A' - 10;
				else key -= '0';
				u8 * f = &pats[cpat].fxvals[crow];
				*f = ((*f) & 0x0F) | (key << 4);
			}
			else if (mode == 3 && isxdigit(key)) {
				if (key >= 'a') key -= 'a' - 10;
				else if (key >= 'A') key -= 'A' - 10;
				else key -= '0';
				u8 * f = &pats[cpat].fxvals[crow];
				*f = ((*f) & 0xF0) | key;
			}
			else	printf("\a");						break;
	}
	return 1;
}


static void render()
{
	term_reset_cursor();

	// header
	printf(COLOR_LOGO "Speaker Tracker " VERSION "\n");
	printf(COLOR_LINE "%2.2X ", cpat);

	if (mode == 0) printf(COLOR_CURRENT_PATTERN "   " COLOR_NORMAL "    ");
	else if (mode == 1) printf(COLOR_NORMAL "    "
			COLOR_CURRENT_PATTERN " " COLOR_NORMAL "  ");
	else if (mode == 2) printf(COLOR_NORMAL "     "
			COLOR_CURRENT_PATTERN " " COLOR_NORMAL " ");
	else printf(COLOR_NORMAL "      "
			COLOR_CURRENT_PATTERN " " COLOR_NORMAL);

	printf(COLOR_LINENO "%d "
			COLOR_CURRENT_PATTERN "%2.2X"
			COLOR_NORMAL "\n", coct, npat);

	// render each line
	int i;
	for (i=0; i<term_height; i++)
		print_line(i);
}


static void print_line(int i)
{
	int hh = term_height >> 1;
	print_row(i-hh+crow);
	print_pattern(i);
	printf(COLOR_NORMAL "\n");
}


static void print_pattern(int i)
{
	int hh = term_height >> 1;
	if (cpat > hh) i += cpat-hh;
	if (i < npat) {
		if (i == cpat) printf(COLOR_CURRENT_PATTERN);
		else printf(COLOR_PATTERN);
		printf(" [%2.2X]", pats[i].nrow);
	}
	else printf(COLOR_NORMAL "     ");
}


static void print_row(int i)
{
	if (i < 0 || i >= pats[cpat].nrow)
		printf(COLOR_LINENO "   " COLOR_LINE "       ");
	else {
		if (i == crow) printf(COLOR_CURRENT_LINENO);
		else printf(COLOR_LINENO);
		printf("%2.2X ", i);
		if (i == crow) printf(COLOR_CURRENT_LINE);
		else printf(COLOR_LINE);
		char note[4];
		note_to_str(pats[cpat].notes[i], note);
		printf(COLOR_NOTE "%s ", note);
		if (pats[cpat].fxs[i]) 
			printf(COLOR_FX "%1.1X%2.2X",
					pats[cpat].fxs[i]-1, pats[cpat].fxvals[i]);
		else printf(COLOR_FX_INVALID "...");
	}
}

/**********************************************************************
 *
 * pc speacker
 *
 */

static void beep_init()
{
	if ((fd_beep = open("/dev/console", O_WRONLY)) == -1) {
		fprintf(stderr, "Could not open /dev/console for writing.\n");
		printf("\a");
		exit(1);
	}
}


static void beep_free()
{
    ioctl(fd_beep, KIOCSOUND, 0);
}


static void beep(float freq)
{
	if (ioctl(fd_beep, KIOCSOUND, (int)(CLOCK_TICK_RATE / freq)) < 0) {
		fprintf(stderr, "Could not beep.\n");
		printf("\a");
		exit(2);
	}
}


/**********************************************************************
 *
 * commands
 *
 */

static void cmd_create()
{
	memset(pats, 0, sizeof(pats));
	npat = 1;
	pats[0].nrow = 0x40;
	cpat = 0;
	crow = 0;
	coct = 4;
	tpr  = 200;
	mode = 0;
}


static void cmd_play()
{
	playing = 1;
	pthread_create(&th_play, NULL, (void *)&thread_play, NULL);
}


static void cmd_stop()
{
	playing = 0;
}


static void cmd_save()
{
	// .stm; Speakertracker Tight Module
	FILE * fp = fopen("music.stm", "w");
	if (!fp) {
//		fprintf(stderr, "Could not save.");
		printf("\a");
		return;
	}

	fwrite(header, sizeof(header)-1, 1, fp);
	fwrite(&npat, sizeof(npat), 1, fp);
	int i, j;
	for (i=0; i<npat; i++) {
		fwrite(&pats[i].nrow, sizeof(pats[0].nrow), 1, fp);
		for (j=0; j<pats[i].nrow; j++) {
			fwrite(&pats[i].notes [j], sizeof(pats[0].notes [0]), 1, fp);
			fwrite(&pats[i].fxs   [j], sizeof(pats[0].fxs   [0]), 1, fp);
			fwrite(&pats[i].fxvals[j], sizeof(pats[0].fxvals[0]), 1, fp);
		}
	}
	fclose(fp);
}


static void cmd_open()
{
	// .stm; Speakertracker Tight Module
	FILE * fp = fopen("music.stm", "r");
	if (!fp) {
//		fprintf(stderr, "Could not open.");
		printf("\a");
		return;
	}

	char str[sizeof(header)-1];
	fread(str, sizeof(header)-1, 1, fp);
	if (strncmp(header, str, sizeof(header)-1)) {
//		fprintf(stderr, "File type mismatch.");
		printf("\a");
		return;
	}

	cmd_create();

	fread(&npat, sizeof(npat), 1, fp);
	int i, j;
	for (i=0; i<npat; i++) {
		fread(&pats[i].nrow, sizeof(pats[0].nrow), 1, fp);
		for (j=0; j<pats[i].nrow; j++) {
			fread(&pats[i].notes [j], sizeof(pats[0].notes [0]), 1, fp);
			fread(&pats[i].fxs   [j], sizeof(pats[0].fxs   [0]), 1, fp);
			fread(&pats[i].fxvals[j], sizeof(pats[0].fxvals[0]), 1, fp);
		}
	}
	fclose(fp);
}


static void cmd_term(int offset)
{
	term_height += offset;
	if (term_height < 5) term_height = 5;
	if (offset < 0) term_clear();
}


static void cmd_move_row(int offset)
{
	crow += offset;

	if (crow >= (s32)pats[cpat].nrow) {
		crow = 0;
		cpat++;
	}
	if (cpat >= (s32)npat) cpat = 0;

	if (crow < 0) {
		cpat--;
		if (cpat < 0) cpat = npat - 1;
		crow = pats[cpat].nrow - 1;
	}
}


static void cmd_move_pat(int offset)
{
	cpat += offset;
	crow = 0;
	if (cpat >= (s32)npat) cpat = 0;
	else if (cpat < 0) cpat = npat - 1;
}


static void cmd_row(int offset)
{
	pats[cpat].nrow += offset;
	if (pats[cpat].nrow < 1) pats[cpat].nrow = 1;
	else if (pats[cpat].nrow > 0x100) pats[cpat].nrow = 0x100;
}


static void cmd_pat(int offset)
{
	npat += offset;
	if (npat < 1) npat = 1;
	else if (npat > 0x100) npat = 0x100;
	if (offset > 0) pats[npat-1].nrow = 0x40;
}


/**********************************************************************
 *
 * thread
 *
 */

static void thread_play()
{
	while (playing) {
		u8 note  = pats[cpat].notes [crow];
		u8 fx    = pats[cpat].fxs   [crow];
		u8 fxval = pats[cpat].fxvals[crow];
		if (fx) {
			switch (fx-1) {
				case 0:	// set tpr
					tpr = fxval*2;
					break;
			}
		}
		float freq;
		if (note) {
			freq = note_to_freq(note);
			beep(freq);
		}
		int i, j;
		if (fx) {
			fx--;
			switch (fx) {
				case 1:	// down
					i = fxval;
					while (i--) {
						usleep(1000 * tpr / fxval);
						freq *= 0.9;
						beep(freq);
					}
					break;
				case 2:	// up
					i = fxval;
					while (i--) {
						usleep(1000 * tpr / fxval);
						freq *= 1.1;
						beep(freq);
					}
					break;
				case 3:	// vib
					i = fxval;
					j = 0;
					while (i--) {
						usleep(1000 * tpr / fxval);
						beep(freq * j);
						j = !j;
					}
					break;
				default:
					usleep(1000 * tpr);
			}
		}
		else usleep(1000 * tpr);
		cmd_move_row(+1);
		render();
	}
	beep(0);
}

