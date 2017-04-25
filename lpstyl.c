/* 
	lpstyl.c
	version 0.9.9

	Copyright (C) 1996-2000 Monroe Williams (monroe@pobox.com)
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:
	1. Redistributions of source code must retain the above copyright
		notice, this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright
		notice, this list of conditions and the following disclaimer in the
		documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
	NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	-----
	
	This is a printer driver for Apple StyleWriter printers.  Here is the 
	known status of printers it supports:
	
	- Color Stylewriter 2500:
		Fully supported in 360x360dpi, B/W and Color.
	- Color Stylewriter 2400:
		Fully supported in 360x360dpi, B/W and Color.
	- Color Stylewriter 2200:
		Supposed to work as of 0.9.9 (I haven't seen it, though.)
	- Color Stylewriter 1500:
		Works in B/W and Color.
	- StyleWriter III, StyleWriter 1200:
		"Works peachy." -- Charles Broderick, bbroder@mit.edu
	- StyleWriter II:
		Reported to work quite well.
	- StyleWriter I:
		Working again as of version 0.9.7.
	- Other stylewriters?:
		Give it a try.  The code will treat any printer it doesn't
		recognize like a SWII, so it just might work.  (That's how
		the SW2400 first worked, anyway.)

	Future releases of this driver will have more documentation, including 
	some explanation of the control codes I have figured out.  Stay tuned.
	
	Any feedback should be directed to monroe@pobox.com.
*/

/* Version history:
	0.9.9:
		- Fixed a problem with identifying the StyleWriter 2400 and 2500.
		- Added changes from Paulo Abreu <paulotex@geocities.com>
			to support the StyleWriter 2200.
		- Initial support for the Apple StyleWriter Ethertalk Adapter
			(Apple part number M4877).  This is a box which connects a
			number of different models of StyleWriter directly to an
			Ethernet network.  This requires netatalk support on the
			machine you're compiling on.  It also required an 
			implementation of a subset of the ADSP protocol for the 
			Appletalk stack, which is in the file adsp.c.  This device
			looks very similar to the "Farallon EtherMac iPrint Adapter SL",
			(it performs the same function and appears to use the same plastics),
			and I'd like to find out if the Farallon device works the same.
	0.9.8:
		- Added changes from Takashi Oe <toe@unlinfo.unl.edu> to
			support the StyleWriter 1500.
		- Made the code's entire concept of paper sizes and margins much
			less confused.  
		- Added A4 paper support.  (Finally.)
		- Added a mechanism for specifying arbitrary paper sizes on 
			the command line.  -W and -H (yes, capital letters) specify
			the width and height of the paper in _pixels_.  One pixel is 
			1/360 of an inch.  In the normal case, these numbers should 
			match the dimensions of the input file.  No validity checking 
			is done on the paper sizes thus received, and I'm sure one could
			make things go rather badly by entering wildly inaccurate numbers.
			Be nice.
		- Improved out-of-paper handling.  The code for all printer
			types should now handle out-of-paper conditions by retrying.
	0.9.7:
		- Finally got hold of a StyleWriter I.  
			It didn't work.  
			It does now.
	0.9.6:
		- Incorporated new information about control codes from 
			Paul Mackerras <paulus@cs.anu.edu.au> to make the 
			StyleWriter 2500 work.
		- Improved out-of-paper handling on 2400/2500.  (Instead of
			retransmitting data, we now use another control code to make
			the printer try again.)  Thanks to Paul Mackerras for coming
			up with the right control code.
		- The driver is now aware of whether a color ink cartridge is
			installed in the 2400/2500.
	0.9.5:
		- Finally, really fixed the finish-page code to wait for the
			right thing.  (Hooray for trial and error...)
		- Figured out how long a microsecond was (duh...) and fixed the
			various retry-loops to sleep for 0.1 second like I originally
			intended.
		- Now deals with the printer running out of paper by retrying
			every 30 seconds until the problem goes away (or someone
			puts a stop to it).
		- No longer reset before every page.  It's unnecessary.  Now we
			reset before the first page of the job, and after printing
			is unpaused with SIGUSR1.
	0.9.4:
		- Now recognizes the SW1200/SW3, courtesy of Charles Broderick 
			<bbroder@mit.edu>.
		- Uses non-blocking I/O to read from the printer.
	0.9.3:
		- Less confusing comments about the default margins.
		- Now checks for what I think are error codes from the printer
			and terminates on errors.
		- Code that gets printer status is now less likely to hang.
		- Disabled the expanded vertical print area on the SW2400 until
			I can figure out why it's causing errors on some pages.
	0.9.2:
		- Generalized the printable area and margins constants so that they can
			be set to different values after the printer is identified.  The 
			SW2400 case in printerSetup now sets larger print area and smaller 
			margins to match what the 2400 can do.  More printers will be added
			as I get their specs.
		- Added a flag -m to disable cropping for top and left margins.
		    Use this flag if you want to see the entire image file.
		    (Files rendered for a full 8.5"x11" page will be offset down and to
		    the right from where they should be on the page.)
	0.9.1:
		- Fixed a problem with waiting for the last part of a page to
			print.  The problem would cause the next page's reset to
			happen too soon, chopping off the last part of the page.
	0.9:
		- Fully functional COLOR on a Color Stylewriter 2400!!!
		- Added signal handlers for common kill signals that eject the
			page and reset the printer.
		- Two new input formats: 'bit' and 'bitcmyk'.
		- Flags, flags, flags.  Type 'lpstyl -?' to see usage.
		- Now sets up the serial port properly (57600 baud, raw mode)
			using termios.
		- Uses a larger buffer size on Color StyleWriter 2400.
	0.2.0d2:
		- Fully functional in B&W on a Color StyleWriter 2400 and a
			SWII
	0.2.0d1:
		- Changed a couple of things to make it work better (but still not
			quite right) on a SWII.
		- First try at using the printer's buffer more efficiently.
	0.2.0d0:
		- First attempt at direct StyleWriter II support, with help from
			Jack Howarth <howarth@bromo.med.uc.edu>.  This includes the
			first attempt at compensating for what looks like a "scanline
			differencing" algorithm the SWII uses, conditional on the 
			returned printer type.
		- Included a modification suggested by Stefan Schmiedl
			<101321.3101@CompuServe.COM> -- reducing the maximum
			amount of data that will be sent at once from 0x10000
			to 0x4000.  (Apparently, some more complex files caused
			the printer's buffer to overflow.)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef unix
	#include <sys/types.h>
	#include <termios.h>
	#include <unistd.h>
	#include <errno.h>
	#include <signal.h>
	#include <fcntl.h>
#else
	/* I sometimes compile this under MacOS for various reasons.  This
		source file gets #included in another one which includes the right
		header files and defines dummy versions of various unix-specific functions.  
	*/
#endif

#ifdef ATALK
	#include <netatalk/at.h> 
	#include <atalk/nbp.h>

	#include "adsp.h"
#endif

/* Just a few prototypes...  Yeah, I was raised on ANSI C.  So sue me. */
void fixPageSizes(void);
int main(int argc, char **argv);
int printStdin(void);
size_t encodescanline(unsigned char *src, size_t srcLen, unsigned char *dst);
void sendEncodedData(unsigned char *buffer, size_t size);
void sendrect(long top, long left, long bottom, long right);

size_t inputRead(void *buffer, size_t size);
int inputGetc(void);
void inputPutback(int c);

void printerSetup(void);
size_t comm_printer_read(void *buffer, size_t size);
size_t comm_printer_write(void *buffer, size_t size);
size_t comm_printer_writestring(char *buffer);
void comm_printer_writeFFFx(char x);
int comm_printer_getc(void);
int comm_printer_getc_block(void);
void ejectAndReset(void);
void finishPage(void);
void waitNonBusy(int canHandlePaperOut);
void printerFlushInput(void);

void print_error(char *s);
void print_info(char *s);
void waitStatus(int stat, int canHandlePaperOut);
int getStatus(int which);


/***** Some relevant constants *****/
/* NOTE: To change these values on a per-printer basis, add a case
	to the "Printer-specific setup" in printerSetup().  Copying
	the SW2400 case is a good place to start.
*/

/* size of the printer's buffer */
long MAX_BUFFER = 0x00004000;

/* Default to letter-size paper, which is 8.5 x 11 inches @ 360dpi */
long PAGE_WIDTH = 3060;
long PAGE_HEIGHT = 3960;

long PRINT_WIDTH;
long PRINT_HEIGHT;
long TOP_MARGIN;	
long BOTTOM_MARGIN;
long LEFT_MARGIN;

/* number of BYTES of each scanline to leave off the left.  (pixels / 8) */
long LEFT_MARGIN_BYTES;
long PRINT_ROWBYTES;

#ifndef nil
	#define nil  ((void*)0)
#endif

/* Do we really care? */
int verbose = 0;

volatile int paused = 0;

char *ProcName;

void handler();
void cleanup();
void usage(void);
int readFileHeader(void);
size_t readFileScanline(char *bufK, char *bufC, char *bufM, char *bufY);
size_t appendEncode(size_t length, char *in, char *last, char *out);

/* Variables printStdin() needs which can also be set by arguments */
long height = -1, width = -1;
enum
{
	FILE_PBMRAW,
	FILE_BIT,
	FILE_BITCMYK
};
int fileType = FILE_PBMRAW;
int fileIsColor = 0;		/* Set to true in readFileHeader if the input
								file contains color information. */
int canPrintColor = 0;		/* set to true in printerSetup() if a color ink
								cartridge is installed. */

int printQuality = 1;		/* 0 = draft (not implemented yet), 1 = normal,
								2 = high. */
size_t rowbytes;

enum
{
	KIND_SW1,
	KIND_SW2,
	KIND_SW3,
	KIND_SW1500,
	KIND_SW2200,
	KIND_SW2400,
	KIND_SW2500,
	KIND_SWUNKNOWN
};
int printerType;
int noMargins = 0;
int doReset = 1;
int bufferHelp = 0;
int atalk_connection = 0;
char *atalk_username = NULL;

#ifdef ATALK
	struct adsp_socket s1, s2;
	struct adsp_endp end1, end2;

	int at_printer_open(char *name);
	void at_printer_kill(void);
	void at_printer_setstatus(char *status);
#endif

long USLEEP_TIME = 100000;	/* 0.1 seconds for usleep() */

void fixPageSizes(void)
{
	/* This routine assumes that the following are set up before it is called:
		(these depend only on the paper size)
			PAGE_WIDTH		- width of the paper
			PAGE_HEIGHT		- height of the paper
		(these depend only on the capabilities of the printer's hardware)
			PRINT_WIDTH 	- width of the printer's imageable area
			LEFT_MARGIN		- printer's left margin
			TOP_MARGIN		- printer's top margin
			BOTTOM_MARGIN	- printer's bottom margin
		
		All values are in pixels.  There are 360 pixels per inch. 
	*/

	/* the height of the printer's imageable area */
	PRINT_HEIGHT = PAGE_HEIGHT - (TOP_MARGIN + BOTTOM_MARGIN + 1);

	/* The number of imageable bytes in each scanline */
	PRINT_ROWBYTES = (PRINT_WIDTH + 7) / 8;

	/* The number of bytes to chop off the left of each scanline to make a left margin */
	LEFT_MARGIN_BYTES = (LEFT_MARGIN + 7) / 8;
}

int main(int argc, char **argv)
{	
	int ch;
	
	ProcName = argv[0];
	
	atalk_username = getlogin();

	/* Enable print pause */
	signal(SIGUSR1, handler);
	
	/* Make sure we eject the page if the driver is killed off. */
	signal(SIGTERM, cleanup);
	signal(SIGHUP, cleanup);
	signal(SIGINT, cleanup);
	
	/* Figure out the options */
	while((ch = getopt(argc, argv, "a:t:f:h:w:H:W:q:p:u:v?mb")) != -1)
	{
		switch(ch)
		{
			case 'f':		/* file to open as printer device */
			{
				int fd;
				
				fd = open(optarg, O_RDWR, 0);
				if(fd == -1)
				{
					perror("open");
					exit(1);
				}
				else
				{
					dup2(fd, 1);
				}
			}
			break;

			case 'a':	/* Printer is on an AppleTalk box. */
			{
#ifdef ATALK
				at_printer_open(optarg);
				atalk_connection = 1;
#else
				fprintf(stderr, "%s: AppleTalk support not compiled in.\n", 
						ProcName);
				exit(1);
#endif
			}
			break;

			case 'u':	/* username for setting status of appletalk printer */
				atalk_username = optarg;
			break;
			
			case 't':		/* file format */
				if(strcmp(optarg, "pbmraw") == 0)
					fileType = FILE_PBMRAW;
				else if(strcmp(optarg, "bit") == 0)
					fileType = FILE_BIT;
				else if(strcmp(optarg, "bitcmyk") == 0)
					fileType = FILE_BITCMYK;
				else
					usage();
			break;
			
			case 'p':
				if(strcmp(optarg, "letter") == 0)
				{
					/* Print on U.S. Letter-size paper */
					PAGE_WIDTH = 3060;    /* 8.5" * 360dpi */
					PAGE_HEIGHT = 3960;   /* 11"  * 360dpi */
				}
				else if(strcmp(optarg, "a4") == 0)
				{
					/* Print on A4 paper */
					PAGE_WIDTH = 2975;    /* about 8.27"  * 360dpi */
					PAGE_HEIGHT = 4210;   /* about 11.69" * 360dpi */
				}
				else
				{
					usage();
				}
			break;
			
			case 'H':		/* paper height (in pixels) */
				PAGE_HEIGHT = atoi(optarg);
			break;

			case 'W':		/* paper width (in pixels) */
				PAGE_WIDTH = atoi(optarg);
			break;
			
			case 'h':		/* input file height (in pixels) */
				height = atoi(optarg);
			break;

			case 'w':		/* input file width (in pixels) */
				width = atoi(optarg);
			break;
			
			case 'm':
				/* don't crop margins */
				noMargins = 1;
			break;

			case 'q':		/* print quality (0, 1, or 2) */
				printQuality = atoi(optarg);
			break;

			case 'v':
				verbose++;
			break;
			
			case 'b':
				bufferHelp = 1;
			break;

			case '?':
			default:
				usage();
			break;
		}
	}
	
	/* Set up some necessary terminal parameters */
	if(!atalk_connection)
	{
		struct termios t;
		
		tcgetattr(1, &t);
		cfmakeraw(&t);
		cfsetispeed(&t, B57600);
		cfsetospeed(&t, B57600);
		if(tcsetattr(1, TCSAFLUSH, &t) == -1)
		{
			perror("tcsetattr");
			exit(1);
		}
		sleep(1);
	}
	
	fprintf(stderr, "%s: printing started, pid = %d.\n", ProcName, getpid());
	
	/* I'd like to do something here so that the driver can be sending data
		to the printer and working on the next chunk at the same time.
		(On my SE/30, it seems to spend about half of its time doing each.)
		Sooner or later, I'll figure out something with pipes or shared
		memory or something.  Not yet.
	*/
	if(bufferHelp)
	{
#if 0
		pipe();
		dup2(fd, 1);
#endif
	}

	/* print some jobs */
	while(printStdin() == 0)
	{
		doReset = 0;
		if(paused)
		{
			fprintf(stderr, "%s: printing paused, you may mess with the printer now.\n", ProcName);
			while(paused)
				pause();
			fprintf(stderr, "%s: printing resumed.\n", ProcName);

			/* Reset the printer before the next page.  Who knows what
				they might have done to the poor thing?
			*/
			doReset = 1;
		}
	}

	fprintf(stderr, "%s: printing finished, exiting.\n", ProcName);

#ifdef ATALK
	if(atalk_connection)
	{
		at_printer_kill();
	}
#endif

	return(0);
}

void usage(void)
{
	fprintf(stderr, "usage: lpstyl [-v] [-m] [-q printQuality] [-t pbmraw | bit | bitcmyk]\n");
	fprintf(stderr, "              [-f printer_device] [-p letter | a4]\n");
	fprintf(stderr, "              [-h inputHeight] [-w inputWidth]\n");
	fprintf(stderr, "              [-H paperHeight] [-W paperWidth]\n");
	exit(1);
}

void handler()
{
	paused = !paused;
	
	if(paused)
		fprintf(stderr, "%s: got SIGUSR1, printing will pause after the next page.\n", ProcName);
	else
		fprintf(stderr, "%s: got SIGUSR1, printing will resume at the next opportunity.\n", ProcName);
}

void cleanup()
{
	fprintf(stderr, "%s: Caught a signal, resetting printer.\n", ProcName);

#if ATALK
	if(atalk_connection)
	{
		at_printer_kill();
	}
	else
#endif
	{
		/* Reset the printer and eject the page. */
		ejectAndReset();
	}

	fprintf(stderr, "%s: printing stopped, exiting.\n", ProcName);

	exit(0);
}

int printStdin(void)
{
	long lastRow, curRow;
	long printwidth, printheight;
	unsigned char *bufK, *lastK;
	unsigned char *lastMark, *dataBuf, *curMark, *tempMark;
	unsigned char *bufC = nil, *bufM = nil, *bufY = nil, *lastC = nil, *lastM = nil, *lastY = nil;
	long result;
	int retry;
	
	result = readFileHeader();
	if(result != 0)
		return(result);
	
	if(verbose)
	{
		fprintf(stderr, "%s: Got a valid header, input image is %dx%d.\n", 
				ProcName, (int)width, (int)height);
	}
	
	/* This may modify margins, etc. */
	printerSetup();

	/* start on the data */
	rowbytes = (width + 7) >> 3;
	printwidth = rowbytes - LEFT_MARGIN_BYTES;
	if(printwidth > PRINT_ROWBYTES)
		printwidth = PRINT_ROWBYTES;
	
	printheight = height;
	if(printheight > PRINT_HEIGHT + TOP_MARGIN)
		printheight = PRINT_HEIGHT + TOP_MARGIN;
	
	/* this buffer is intentionally too big. */
	dataBuf = malloc(MAX_BUFFER * 2);
	bufK = malloc(rowbytes + 8);
	lastK = malloc(rowbytes + 8);
	
	/* Check for malloc errors  */
	if((!bufK) || (!lastK) || (!dataBuf))
	{
		print_error("Couldn't allocate buffers");
		return(-1);
	}
	else
	{
		print_info("Buffers have been allocated.");
	}
	
	if(fileIsColor && canPrintColor)
	{
		bufC = malloc(rowbytes + 8);
		bufM = malloc(rowbytes + 8);
		bufY = malloc(rowbytes + 8);
		lastC = malloc(rowbytes + 8);
		lastM = malloc(rowbytes + 8);
		lastY = malloc(rowbytes + 8);
		if((!bufC) || (!bufM) || (!bufY) || (!lastC) || (!lastM) || (!lastY))
		{
			print_error("Couldn't allocate color buffers");
			return(-1);
		}
		else
		{
			print_info("Color buffers have been allocated.");
		}
	}

	for(curRow = 0;curRow < TOP_MARGIN;curRow++)
	{
		/* read in a scanline and ignore it */
		readFileScanline(nil, nil, nil, nil);
	}
	print_info("Skipped top margin.");
	
	curMark = lastMark = dataBuf;
	lastRow = curRow;
	
	/* Make sure the "previous" line looks blank. */
	memset(lastK, 0, rowbytes + 8);
	if(lastC)
		memset(lastC, 0, rowbytes + 8);
	if(lastM)
		memset(lastM, 0, rowbytes + 8);
	if(lastY)
		memset(lastY, 0, rowbytes + 8);
	
	print_info("Encoding data...");
	for(;;)
	{
		if(curRow < height)
		{
			/* read a scanline */
			result = readFileScanline(bufK, bufC, bufM, bufY);
	
			/* check for input problems */
			if(result != rowbytes)
			{
				print_error("Error reading input file or pipe");
				/* eject the page and reset the printer */
				ejectAndReset();
				return(-1);
			}
		}
		else
		{
			/* fake it */
			memset(bufK, 0, rowbytes);
		}
		curRow++;
		
		/* encode the scanline */
		switch(printerType)
		{
		case KIND_SW1:
			/* encode the scanline into the buffer */
			curMark += appendEncode(printwidth, bufK + LEFT_MARGIN_BYTES, nil, curMark);
		break;

		case KIND_SW1500:
		case KIND_SW2200:
		case KIND_SW2400:
		case KIND_SW2500:
			if(fileIsColor && canPrintColor)
			{
				curMark += appendEncode(printwidth, bufC + LEFT_MARGIN_BYTES, lastC + LEFT_MARGIN_BYTES, curMark);
				curMark += appendEncode(printwidth, bufM + LEFT_MARGIN_BYTES, lastM + LEFT_MARGIN_BYTES, curMark);
				curMark += appendEncode(printwidth, bufY + LEFT_MARGIN_BYTES, lastY + LEFT_MARGIN_BYTES, curMark);
			}
		/* FALLTHROUGH */
		default: 
			curMark += appendEncode(printwidth, bufK + LEFT_MARGIN_BYTES, lastK + LEFT_MARGIN_BYTES, curMark);
		break;
		}

		/* swap the buffers */
		tempMark = lastK;		lastK = bufK;		bufK = tempMark;
		if(fileIsColor && canPrintColor)
		{
			tempMark = lastC;		lastC = bufC;		bufC = tempMark;
			tempMark = lastM;		lastM = bufM;		bufM = tempMark;
			tempMark = lastY;		lastY = bufY;		bufY = tempMark;
		}
		
		/* if this chunk put us over the buffer limit... */
		if((curMark - dataBuf >= MAX_BUFFER) || (curRow >= printheight))
		{
			if(curMark - dataBuf < MAX_BUFFER)
			{
				/* This is the last bit. */
				lastMark = curMark;
				curRow++;
			}
			
			/* wait for a not-busy status */
			waitNonBusy(0);
			
			do
			{
				retry = 0;
				/* send the last batch of scanlines */
				print_info("Ready to send a block of image data...");

				sendrect(lastRow - TOP_MARGIN, 0, 
						(curRow-2) - TOP_MARGIN, PRINT_WIDTH);
				sendEncodedData(dataBuf, (lastMark - dataBuf));
				print_info("data sent.");

				if(lastRow - TOP_MARGIN == 0)
				{
					/* This is the first part of the page. */
					
					/* Wait long enough to make sure the printer knows if it's out of paper... */
					waitNonBusy(1);
					
					if(getStatus('2') == 4)
					{
						switch(printerType)
						{

						default:
							/* the printer is out of paper. */
							print_error("The printer is out of paper -- trying again in 30 seconds...");
							ejectAndReset();
							retry = 1;
							sleep(30);
						break;

						case KIND_SW1500:
						case KIND_SW2200:
						case KIND_SW2400:
						case KIND_SW2500:
							do
							{
								print_error("The printer is out of paper -- trying to continue in 5 seconds...");
								sleep(5);
								comm_printer_writeFFFx('S');
							}
							while(getStatus('2') == 4);
						break;

						}
					}
				}
			} while(retry);
			
			if(lastMark != curMark)
			{
				/* Re-encode the last scanline */
				print_info("Encoding data...");
				curMark = dataBuf;
				/* (no differencing) */
				switch(printerType)
				{
				case KIND_SW1500:
				case KIND_SW2200:
				case KIND_SW2400:
				case KIND_SW2500:
					if(fileIsColor && canPrintColor)
					{
						curMark += appendEncode(printwidth, lastC + LEFT_MARGIN_BYTES, nil, curMark);
						curMark += appendEncode(printwidth, lastM + LEFT_MARGIN_BYTES, nil, curMark);
						curMark += appendEncode(printwidth, lastY + LEFT_MARGIN_BYTES, nil, curMark);
					}	
				/* FALLTHROUGH */
				default:
					curMark += appendEncode(printwidth, lastK + LEFT_MARGIN_BYTES, nil, curMark);
					lastRow = curRow-1;
				break;
				}
			}
			else
			{
				/* we're done with the page */
				print_info("Done with page.");
				break;
			}
		}
		
		lastMark = curMark;
	}
	
	/* This was adjusted to do the last bit of the page -- adjust it back. */
	curRow--;
	
	print_info("Skipping bottom margin.");
	/* skip any extra lines in the input file */
	for(;curRow < height;curRow++)
	{
		/* read in a scanline and ignore it */
		readFileScanline(nil, nil, nil, nil);
	}
	print_info("Skipped bottom margin.");

	/* wait for a not-busy status */
	waitNonBusy(0);

	/* finish printing the page */
	finishPage();
	
	/* clean up buffers */
	free(bufK);
	free(dataBuf);
	if(bufC) free(bufC);
	if(bufM) free(bufM);
	if(bufY) free(bufY);
	if(lastC) free(lastC);
	if(lastM) free(lastM);
	if(lastY) free(lastY);
	
	return(0);
}

int readFileHeader(void)
{
	int c, i;
	char smallBuf[32];

	fileIsColor = 0;

	switch(fileType)
	{
		case FILE_PBMRAW:
			/* read the file header (including height and width) */
			height = -1;
			do
			{
				c = inputGetc();
				switch(c)
				{
					case -1:
						/* end of file -- we're done here.  Note that this is not an error */
						return(1);
						break;
					case '#':
						/* comment line -- skip it */
						while((c = inputGetc()) != 0x0A)
							;
					break;
					case ' ': case '\n': case '\t':
						/* random whitespace... just ignore it. */
					break;
					case 'P':
						/* magic number */
						if((c = inputGetc()) != '4')
						{
							/* bad magic number */
							print_error("Bad magic number in input file");
							return(-1);
						}
						/* there should be one whitespace character */
					break;
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9':
						/* read width */
						smallBuf[0] = c;
						for(i=1;isdigit(c = inputGetc()) && (i < sizeof(smallBuf));i++)
							smallBuf[i] = c;
						if(!isspace(c))
						{
							print_error("Bad input file format");
						}
						smallBuf[i] = 0;
						width = atoi(smallBuf);
						/* read height */
						for(i=0;isdigit(c = inputGetc()) && (i < sizeof(smallBuf));i++)
							smallBuf[i] = c;
						if(!isspace(c))
						{
							print_error("Bad input file format");
						}
						smallBuf[i] = 0;
						height = atoi(smallBuf);
					break;
					default:
						print_error("Bad header format in input file");
						return(-1);
					break;
				}
			}while(height == -1);
			
			/* the header has been taken care of.  The rest of the file is just image data. */	
		break;
		
		case FILE_BITCMYK:
			fileIsColor = 1;
			print_info("Input file is in color.");
		/* FALLTHROUGH */
		
		case FILE_BIT:
			/* These files have no header, but we can make sure we were given
				a height and width...
			*/
			
			if((height == -1) || (width == -1))
			{
				fprintf(stderr, "%s: Width and height must be specified for 'bit' and 'bitcmyk' files.\n", 
						ProcName);
				return(-1);
			}
			
			c = inputGetc();
			if(c == -1)
			{
				/* We're done.  Note that this is not an error. */
				return(1);
			}
			else
			{
				inputPutback(c);
			}
		break;
	}
	return(0);
}


size_t readFileScanline(char *bufK, char *bufC, char *bufM, char *bufY)
{
	static char *inputBuf = nil;
	static size_t inputSize = 0;
	size_t result = 0;
	
	switch(fileType)
	{
		case FILE_PBMRAW:
		case FILE_BIT:
			/* These two are the same, after the header. */
			if(bufK)
			{
				result = inputRead(bufK, rowbytes);
			}
			else
			{
				/* the black buffer is nil -- read into a dummy buffer. */
				if(inputSize < rowbytes)
				{
					/* reallocate the buffer */
					if(inputBuf)
						free(inputBuf);
					inputBuf = malloc(rowbytes);
					inputSize = rowbytes;
				}
				result = inputRead(inputBuf, rowbytes);
			}
		break;
		
		case FILE_BITCMYK:
		{
			size_t filerowbytes = ((width * 4) + 7) >> 3;
			
			if(inputSize < filerowbytes)
			{
				/* reallocate the buffer */
				if(inputBuf)
					free(inputBuf);
				inputBuf = malloc(filerowbytes);
				inputSize = filerowbytes;
			}
			
			if(inputRead(inputBuf, filerowbytes) != filerowbytes)
			{
				result = 0;
				return(result);
			}
			else
			{
				result = rowbytes;
			}
			
			if(!bufK)
			{
				/* skip the hard part. */
				return(result);
			}
			
			/* ASSUMPTION: any extra bits in the file (due to rowbytes) are 0. */
			/* NOTE: I haven't thought through whether this code will work on a
				little-endian system.  Caveat emptor.
			*/

			/* Convert the bits from chunky to planar */
			/* NOTE: This is probably the worst way to do this, but I'm not feeling
				inspired at the moment.  It should work, and I can replace it later
				with something more efficient.
			*/
			if(bufC && bufM && bufY)
			{
				register unsigned long byte, i;
				register unsigned long cmyk = 0;
				register unsigned char *input = (unsigned char*)inputBuf;
				
				/* full color */
				for(i = 0; i < filerowbytes; i++)
				{
					static unsigned long bitsTable[] = {
						0x00000000, 0x00000001, 0x00000100, 0x00000101,
						0x00010000, 0x00010001, 0x00010100, 0x00010101,
						0x01000000, 0x01000001, 0x01000100, 0x01000101,
						0x01010000, 0x01010001, 0x01010100, 0x01010101 
					};
					
					/* get the input */
					byte = (*input++) & 0x000000FF;
					
					/* shift everything */
					cmyk <<= 2;
					
					/* extract the bits from this nibble */
					cmyk |= (bitsTable[(byte >> 4) & 0x0000000F]) << 1;
					cmyk |= bitsTable[byte & 0x0000000F];

					if((i & 0x03) == 0)
					{
						/* write the output */
						*((unsigned char *)bufK++) = cmyk & 0x000000FF;		cmyk >>= 8;
						*((unsigned char *)bufY++) = cmyk & 0x000000FF;		cmyk >>= 8;
						*((unsigned char *)bufM++) = cmyk & 0x000000FF;		cmyk >>= 8;
						*((unsigned char *)bufC++) = cmyk & 0x000000FF;
						cmyk = 0;
					}
				}

				/* Even up the bytes */
				for(;(i & 0x03) != 0;i++)
				{
					/* shift everything */
					cmyk <<= 2;
				}
				
				/* write the final part */
				*((unsigned char *)bufK++) = cmyk & 0x000000FF;		cmyk >>= 8;
				*((unsigned char *)bufY++) = cmyk & 0x000000FF;		cmyk >>= 8;
				*((unsigned char *)bufM++) = cmyk & 0x000000FF;		cmyk >>= 8;
				*((unsigned char *)bufC++) = cmyk & 0x000000FF;
			}
			else
			{
				register int byte, i;
				register char *input = inputBuf;
				register int k = 0;
				
				/* Black channel only */
				for(i = 0; i < filerowbytes; i++)
				{
					/* get the input */
					byte = *input++;
					
					/* shift everything */
					k <<= 2;
					
					if(byte & 0x10){k |= 2;}
					if(byte & 0x01){k |= 1;}

					if((i & 0x03) == 0)
					{
						/* write the output */
						*bufK++ = k;
					}
				}

				/* Even up the bytes */
				for(;(i & 0x03) != 0;i++)
				{
					/* shift everything */
					k <<= 2;
				}
				
				/* write the final byte */
				*bufK++ = k;
			}
		}
		break;
	}
	
	return(result);
}

size_t appendEncode(size_t length, char *in, char *last, char *out)
{
	static char delta[1024];	/* bigger than we'll ever need */
	size_t result = 0;
	
	if(last)
	{
		/* XOR the input with the last scanline */
		unsigned char *src1 = in, *src2 = last, *dst = delta;
		size_t size = length;

		for(;size > 0; size--)
			*dst++ = (*src1++) ^ (*src2++);

		/* encode the difference into the buffer */
		result = encodescanline(delta, length, out);
	}
	else	
	{
		/* encode the scanline into the buffer */
		result = encodescanline(in, length, out);
	}
	
	return(result);
}


void ejectAndReset(void)
{
	/* Eject the page and reset the printer. */
	comm_printer_writeFFFx('I');
}

void finishPage(void)
{
	int c;

	/* Print the last part of the page. */
	switch(printerType)
	{
		default:
			print_info("finishpage: sending \"0x0C\"");
			comm_printer_writestring("\x0C");
			
			do
			{
				c = getStatus('1');
				if(verbose > 2)
				{
					fprintf(stderr, "%s: status(1) = 0x%X. \n", 
						ProcName, c);
				}
				usleep(USLEEP_TIME);
			}while(c == 1);
			
			waitNonBusy(0);
			/* XXX -- This may not be necessary */
			/*
			print_info("finishpage: sending \"El\"");
			comm_printer_writestring("El");
			*/
		break;

		case KIND_SW1:
			print_info("finishpage: sending \"0x0C\"");
			comm_printer_writestring("\x0C");
			waitNonBusy(0);
		break;
	}
}

void waitNonBusy(int canHandlePaperOut)
{
	print_info("About to wait for a non-busy status...");
	switch(printerType)
	{
		case KIND_SW1500:
			if(canPrintColor)
			{
				/* It seems 1500 sends different "wait" status code when color ink
				   cartridge is installed.  <toe@unlinfo.unl.edu> */
				waitStatus(0x87, canHandlePaperOut);
			}
			else
			{
				waitStatus(0x9F, canHandlePaperOut);
			}
		break;

		case KIND_SW2500:
			waitStatus(0x80, canHandlePaperOut);
		break;

		case KIND_SW2200:
			waitStatus(0xA3, canHandlePaperOut);
		break;

		case KIND_SW1:
			waitStatus(0xA0, canHandlePaperOut);
		break;

		default:
			waitStatus(0xF8, canHandlePaperOut);
		break;
	}
#ifdef ATALK
	if(atalk_connection)
	{
		char buf[2];

		buf[0] = 0;
		adsp_write_attn(&end2, 0x0006, buf, 1);
		adsp_read(&end2, buf, 2);

		buf[0] = 0;
		adsp_write_attn(&end2, 0x0006, buf, 1);
		adsp_read(&end2, buf, 2);
	}
#endif

}

#define MAX_RUN 0x3E
#define MAX_BLOCK 0x3E
#define RUN_THRESH 0x01
#define DATA_WHITE 0x00
#define DATA_BLACK 0xFF
#define DATA_OTHER 0x0A
#define MASK_RUNWHT 0x80
#define MASK_RUNBLK 0xC0
#define MASK_RUNDATA 0x00

size_t encodescanline(unsigned char *src, size_t srcLen, unsigned char *dst)
{
	register signed long s, d, i, runStart, runLen;
	unsigned char runChar;
	
	/* SPECIAL CASE: check for a blank line */
	for(i=0;i < srcLen;i++)
		if(src[i] != DATA_WHITE)
			break;
	
	if(i == srcLen)
	{
		*dst = MASK_RUNWHT;
		return(1);
	}
	
	s = d = 0;
	while(s < srcLen)
	{
		runStart = runLen = 0;
		runChar = DATA_OTHER;	/* Just so long as it's not black or white... */
		
		/* Find the first run */
		for(i = s; (i < srcLen); i++)
		{
			if((runChar == DATA_WHITE) || (runChar == DATA_BLACK))
			{
				if(src[i] != runChar)
				{	/* This run is over */
					if(i - runStart >= RUN_THRESH)
					{	/* The run was long enough to count.  Break out. */
						break;
					}
					else
					{	/* The run was too short to count. */
						runChar = DATA_OTHER;
					}
				}
				else	
				{	
					if(i - runStart >= MAX_RUN)
					{	/* This is enough of a run to encode. */
						break;
					}
				}
			}
			else	
			{	/* runChar == DATA_OTHER */
				if((src[i] == DATA_WHITE) || (src[i] == DATA_BLACK))
				{	/* Start a run */
					runChar = src[i];
					runStart = i;
				}
				else if(i - s >= MAX_BLOCK)
				{	/* This block is the maximum length. */
					break;
				}
			}
		}	/* end for */

		if(runChar != DATA_OTHER)
		{
			/* We were in a run when we broke out. */
			runLen = i - runStart;
		}
		else
		{
			runStart = i;
			runLen = 0;
		}
		
		if(runStart != s)
		{
			/* Encode a run of random data */
			dst[d++] = runStart - s;
			for(;s < runStart;)
				dst[d++] = src[s++];
		}
		
		if(runLen > 0)
		{
			/* Encode a run of black or white */
			if(runChar == DATA_BLACK)
				dst[d++] = MASK_RUNBLK + runLen;
			else
			{
				if(s + runLen < srcLen)
					dst[d++] = MASK_RUNWHT + runLen;
				else
					break;	/* Let this be taken up with the padding. */
			}
			s += runLen;
		}
		
		
	}	/* end while */
	
	/* pad out to the width of the page with white */
	for(;s < PRINT_ROWBYTES;)
	{
		runLen = PRINT_ROWBYTES - s;
		if(runLen > MAX_RUN)
			runLen = MAX_RUN;
		dst[d++] = MASK_RUNWHT + runLen;
		s += runLen;
	}
	return(d);
}


void sendEncodedData(unsigned char *buffer, size_t size)
{
	unsigned char begin[4];
	unsigned char save;

	if(verbose)
	{
		fprintf(stderr, "%s: Sending encoded data (0x%x bytes).\n", 
				ProcName, size);
	}	
	begin[0] = 'G';
	begin[1] = size & 0x000000FF;
	begin[2] = size >> 8;
	
	comm_printer_write(begin, 3);

	save = buffer[size];
	buffer[size] = 0;
	comm_printer_write(buffer, size+1);
	buffer[size] = save;
}

void sendrect(long top, long left, long bottom, long right)
{
	unsigned char buffer[9];
	
	if(verbose)
	{
		fprintf(stderr, "%s: Sending rect %ld,%ld,%ld,%ld.\n", 
				ProcName, top, left, bottom, right);
	}	
	/* top = 0, bottom = 0xb5 */
	/* left = 0;
	right = PRINT_WIDTH; */
	
	if(fileIsColor && canPrintColor)
		buffer[0] = 'c';
	else
		buffer[0] = 'R';

	buffer[1] = left & 0x000000FF;
	buffer[2] = left >> 8;
	buffer[3] = top & 0x000000FF;
	buffer[4] = top >> 8;
	buffer[5] = right & 0x000000FF;
	buffer[6] = right >> 8;
	buffer[7] = bottom & 0x000000FF;
	buffer[8] = bottom >> 8;
	
	comm_printer_write(buffer, 9);
}

int inputGetc(void)
{
	unsigned char c;
	size_t length;
	unsigned long result = 0;
	
	length = inputRead(&c, 1);
	result = c;
	if(length == 1)
		return(result);
	else
		return(-1);
}

void printerSetup(void)
{
	char smallBuf[32];
	int c, c1, c2, i;

	if(doReset)
	{
		print_info("Resetting printer...");
		ejectAndReset();
		sleep(2);

#if 1
		do
		{
			usleep(USLEEP_TIME);
			c1 = getStatus('1');
			c2 = getStatus('2');
			c = getStatus('B');
			if(verbose > 2)
			{
				fprintf(stderr, "%s: printer state 1 = 0x%X, 2 = 0x%X, B = 0x%X, \n", 
					ProcName, c1, c2, c);
			}
			
			/* The SW1 shows readiness this way. */
			if((c1 == 1) && (c2 == 0) && (c == 0xA0))
				break;
				
		}while((c1 == 1) || (c1 == -1));
#endif
	
		print_info("Identifying printer...");
		comm_printer_writestring("?");

		usleep(USLEEP_TIME);	/* wait long enough for the printer to respond. */
		for(i=0;((c = comm_printer_getc_block()) != -1) && (i < sizeof(smallBuf)-1); i++)
		{
			if(c == 0x0D)
				break;
			smallBuf[i] = c;
			usleep(USLEEP_TIME);
		}
		smallBuf[i] = 0;
		
		if((c == -1) && (errno != EAGAIN))
		{
			print_error("Something went wrong reading from the printer...");
			fprintf(stderr, "%s: fatal error, exiting.\n", ProcName);
			exit(1);
		}
		
		/* What kind of printer is it? */
		if(i == 0)
		{
			print_error("Error reading from printer.  Is it connected and turned on?");
			fprintf(stderr, "%s: fatal error, exiting.\n", ProcName);
			exit(1);
		}
		else if(!strcmp(smallBuf, "IJ10"))
		{
			printerType = KIND_SW1;
			print_info("Printer is a StyleWriter I.");
		}
		else if(!strcmp(smallBuf, "SW"))
		{
			printerType = KIND_SW2;
			print_info("Printer is a StyleWriter II.");
		}
		 else if(!strcmp(smallBuf, "SW3"))
		 {
			 printerType = KIND_SW3;
			 print_info("Printer is a StyleWriter III (1200).");
		 }
		else if(!strcmp(smallBuf, "CS"))
		{
			printerType = KIND_SW2400;
			print_info("Printer is a Color StyleWriter 1500, 2200, 2400 or 2500.");
		}
		else
		{
			fprintf(stderr, "%s: Unknown printer ID string \"%s\".  Treating it like a StyleWriter II.  (Be amazed if this works.)\n", 
				ProcName, smallBuf);
			printerType = KIND_SW2;
		}

		/* Printer-specific setup */
		canPrintColor = 0;
		switch(printerType)
		{
			case KIND_SW1:
				/* The buffer on the SW1 is smaller. */
				MAX_BUFFER = 0x00002000;
				
				/* Otherwise, it's just like the others. */
			/* FALLTHROUGH */
			default:
				PRINT_WIDTH = 2879;			/* (360 dpi * 8 inches) - 1 */
				LEFT_MARGIN = 90;			/* 0.25 inches * 360 dpi */
				TOP_MARGIN = 90;			/* 0.25 inches * 360 dpi */
				BOTTOM_MARGIN = 90;			/* 0.25 inches * 360 dpi */
			break;
			
			case KIND_SW1500:
			case KIND_SW2200:
			case KIND_SW2400:
			case KIND_SW2500:
				i = getStatus('p');
				if (verbose)
					fprintf(stderr, "%s: status(p) = 0x%x\n", ProcName, i);

				switch(i)
				{
					case 0x01:
						print_info("Looks like the printer is really a 2400.");
						printerType = KIND_SW2400;
					break;
					
					case 0x02:
						print_info("Looks like the printer is really a 2200.");
						printerType = KIND_SW2200;
					break;
					
					case 0x04:
						print_info("Looks like the printer is really a 1500.");
						printerType = KIND_SW1500;
					break;
					
					case 0x05:
						print_info("Looks like the printer is really a 2500.");
						printerType = KIND_SW2500;
					break;
					
					default:
						print_info("Unknown printer subtype!  Treating it like a 2400.");
						printerType = KIND_SW2400;
					break;
				}


				comm_printer_writestring("D");
				i = getStatus('H');
				if (verbose)
					fprintf(stderr, "%s: status(H) = 0x%x\n", ProcName, i);
				if(i & 0x0080)
				{
					print_info("A color ink cartridge is installed.");
					canPrintColor = 1;
				}
				else
				{
					print_info("A black ink cartridge is installed.");
				}

				switch(printQuality)
				{
					default:
						comm_printer_writestring("m0nZAH");
					break;

					case 2:
						comm_printer_writestring("m0sAB");
						i = getStatus('R');
						if (verbose)
							fprintf(stderr, "%s: status(R) = 0x%x\n", ProcName, i);
						break;
						comm_printer_writestring("N");
					break;
				}
				/* This thing has a larger buffer,... */
				MAX_BUFFER = 0x00008000;
				/* ...larger print area... */
				PRINT_WIDTH = 2919;			/* = (360 dpi * 8.1111....") - 1 */
				LEFT_MARGIN = 72;			/* about 0.2" * 360 dpi */
				TOP_MARGIN = 90;			/* 0.25 inches * 360 dpi */
				BOTTOM_MARGIN = 90;			/* 0.25 inches * 360 dpi */

				/* I think this should work, but it seems to be causing 
					errors printing certain files.  I'll disable it for now... 
				*/
				/* TOP_MARGIN = 45;*/  /* about 0.12" * 360 */
				/* BOTTOM_MARGIN = 45;*/  /* about 0.12" * 360 */

			break;
		}

		if(noMargins)
		{
			BOTTOM_MARGIN += TOP_MARGIN;
			TOP_MARGIN = 0;
			LEFT_MARGIN = 0;
		}
		fixPageSizes();

		print_info("The printer is now set up.");
	}

	/* Do as little as needed to start a new page. */
	switch(printerType)
	{
		case KIND_SW1:
		default:
			comm_printer_writestring("nuA");
		break;

		case KIND_SW1500:
		case KIND_SW2200:
		case KIND_SW2400:
		case KIND_SW2500:
			comm_printer_writestring("L");
		break;
	}
}

size_t comm_printer_read(void *buffer, size_t size)
{
	size_t result;
#ifdef ATALK
	if(atalk_connection)
	{
		result = adsp_read_nonblock(&end2, (char*)buffer, size);
	}
	else
#endif
	{
		int flags;

		flags = fcntl(1, F_GETFL, 0);
		fcntl(1, F_SETFL, flags | O_NONBLOCK);

		result = read(1, buffer, size);

		fcntl(1, F_SETFL, flags);
	}

	return(result);
}
size_t comm_printer_write(void *buffer, size_t size)
{
	size_t result;
#ifdef ATALK
	if(atalk_connection)
	{
		result = adsp_write(&end2, (char*)buffer, size);
	}
	else
#endif
	{
		result = write(1, buffer, size);
	}

	return(result);
}

size_t comm_printer_writestring(char *buffer)
{
	return(comm_printer_write(buffer, strlen(buffer)));
}

int comm_printer_getc(void)
{
	unsigned char c[1];
	size_t length;
	unsigned long result = 0;
	
	length = comm_printer_read(c, 1);
	result = c[0];
	if(length == 1)
		return(result);
	else
		return(-1);
}

void printerFlushInput(void)
{
	int flags;
	char dummy[2];
	
	flags = fcntl(1, F_GETFL, 0);
	fcntl(1, F_SETFL, flags | O_NONBLOCK);
	
	while(read(1, dummy, 1) != EAGAIN)
	{
		/* discard these characters */
	}

	fcntl(1, F_SETFL, flags);
}

void comm_printer_writeFFFx(char x)
{
	unsigned char string[4];
	string[0] = string[1] = string[2] = 0xFF;
	string[3] = x;
	comm_printer_write(string, 4);
}

void print_error(char *s)
{
	/* Do something appropriate with error messages... */
	fprintf(stderr, "%s: %s (%d)\n", 
			ProcName, s, errno);
}

void print_info(char *s)
{
	/* Print out a status message */
	if(verbose)
		fprintf(stderr, "%s: %s\n", ProcName, s);
}

int comm_printer_getc_block(void)
{
	int result;
	int i;

	/* wait up to about ten seconds... */
	for(i = 100; i > 0; i--)
	{
		result = comm_printer_getc();
		if(result != -1)
			break;
		else if(errno != EAGAIN)
		{
			/* Something more drastic than no pending input */
			/* MBW -- XXX -- What should happen here? */
			perror("read");
			break;
		}
		usleep(USLEEP_TIME);
	}

	if(result == -1)
	{
		/* Timeout waiting for input from the printer */
		/* MBW -- XXX -- What should happen here? */
	}

	return(result);
}

int getStatus(int which)
{
	comm_printer_writeFFFx(which);
	return(comm_printer_getc_block());
}

void waitStatus(int stat, int canHandlePaperOut)
{
	int c, last = stat;
	int c1 = 0, c2 = 0, last1 = 0, last2 = 0;
	
	last1 = c1 = getStatus('1');
	last2 = c2 = getStatus('2');
	last = c = getStatus('B');
	
	while(c != stat)
	{
		usleep(USLEEP_TIME);
		if(verbose)
		{
			if((c != last) || (c1 != last1) || (c2 != last2) || (verbose > 2))
			{
				fprintf(stderr, "%s: printer state 1 = 0x%X, 2 = 0x%X, B = 0x%X, \n", 
						ProcName, c1, c2, c);
			}
		}

		switch(c2)
		{
			case 4:		/* The printer is out of paper... */

				if(canHandlePaperOut)
				{
					/* The caller will detect and deal with out-of-paper conditions. */
					return;
				}
					
				switch(printerType)
				{
					default:
						/* MBW -- XXX -- Figure out how to deal with this... */
						fprintf(stderr, 
						"%s: The printer is out of paper at a bad time, exiting.\n" 
						, ProcName);
						ejectAndReset();
						exit(0);
					break;

					case KIND_SW1500:
					case KIND_SW2200:
					case KIND_SW2400:
					case KIND_SW2500:
						do
						{
							print_error("The printer is out of paper -- trying to continue in 5 seconds...");
							sleep(5);
							comm_printer_writeFFFx('S');
						}
						while(getStatus('2') == 4);
					break;
				}
			break;

			default:
				/* something went wrong... */
				fprintf(stderr, 
						"%s: Error code from the printer (0x%X), exiting.\n", 
						ProcName, c2);
				ejectAndReset();
				/* Exit code 1 causes lpd to restart and try again.
					I need to think about whether this is the right behavior
					if something is wrong with the printer...
				*/
				exit(0);
			break;

			case 0x80:
			case 0:
				/* everything's fine... */
			break;
		}
	
		last = c;
		last1 = c1;
		last2 = c2;

		c1 = getStatus('1');
		c2 = getStatus('2');
		c = getStatus('B');
	}

	if(verbose)
	{
		fprintf(stderr, "%s: printer state 1 = 0x%X, 2 = 0x%X, B = 0x%X, \n", 
				ProcName, c1, c2, c);
	}
}

static int inputBack = -1;
size_t inputRead(void *buffer, size_t size)
{
	size_t result = 0, len = 0;
	
	if((inputBack != -1) && (size > result))
	{
		((char*)buffer)[result] = inputBack & 0x000000FF;
		result++;
		inputBack = -1;
	}
	
	while(size - result > 0)
	{
		len = read(0, (void*)(((char *)buffer) + result), size - result);
		if(len < 0)
		{
			/* check the error code */
			if(errno != EINTR)
				return(-1);
			else
			{
				print_error("Interrupted system call in input read.");
				usleep(USLEEP_TIME);
			}
		}
		else if(len == 0)
		{
			/* That's all she wrote... */
			return(result);
		}
		else
			result += len;
	}
	return(result);
}

void inputPutback(int c)
{
	inputBack = c;
}

#ifdef ATALK

int at_printer_open(char *name)
{
	struct printRequest
	{
		u_int32_t	port;
		char 		string[66];
	} __attribute__((packed));

	int result;

	if(verbose > 0)
	{
		fprintf(stderr, "%s:opening appletalk connnection to %s\n",
				ProcName, name);
	}

	if(adsp_open_socket(&s1) < 0)
	{
		fprintf(stderr, "%s:failed to open the socket.\n", ProcName);
		exit(1);
	}

	if(adsp_open_socket(&s2) < 0)
	{
		fprintf(stderr, "%s:failed to open the socket.\n", ProcName);
		exit(1);
	}

	/* Get the status of the printer */
	while(1)
	{
		u_int16_t results, results2;
		char scratch[4];

		if(adsp_connect(&s1, &end1, name) < 0)
		{
			fprintf(stderr, "%s:failed to connect.\n", ProcName);
			exit(1);
		}

		{
			struct printRequest req;
			req.port = htons(s2.local_addr.sat_port);
			strcpy(req.string + 1, atalk_username);
			req.string[0] = strlen(req.string + 1);

			adsp_write_attn(&end1, 0x000b, (char*)&req, sizeof(req));
		}

		if(adsp_read(&end1, (char*)&results, 2) != 2)
		{
			fprintf(stderr, "%s:failed to read result code from printer.\n", 
				ProcName);
			exit(1);
		}

		results = ntohs(results);
		if(results == 0)
		{
			break;
		}
		else if(results == 0xFFFF)
		{
			fprintf(stderr, "%s:error opening data connection (%d).\n", 
				ProcName, results);
			exit(1);
		}
		else
		{
			u_int16_t jobcount;
			char username[72], status[259];

			/* The printer is busy. */
			scratch[0] = 0;
			adsp_write_attn(&end1, 0x0011, scratch, 1);

			if(adsp_read(&end1, (char*)&jobcount, 2) != 2)
			{
				fprintf(stderr, "%s:failed to read job count from printer.\n", 
					ProcName);
				exit(1);
			}

			jobcount = ntohs(jobcount);

			scratch[0] = 0;
			adsp_write_attn(&end1, 0x000e, scratch, 1);

			if(adsp_read(&end1, username, 72) != 72)
			{
				fprintf(stderr, "%s:failed to read user name from printer.\n", 
					ProcName);
				exit(1);
			}

			scratch[0] = scratch[1] = 0;
			adsp_write_attn(&end1, 0x0010, scratch, 1);

			if(adsp_read(&end1, status, 259) != 259)
			{
				fprintf(stderr, "%s:failed to read user name from printer.\n", 
					ProcName);
				exit(1);
			}

			username[username[6] + 7] = 0;
			status[status[2] + 3] = 0;
			fprintf(stderr, 
				"%s:printer busy, %d jobs in queue, user '%s', status '%s'\n", 
				ProcName, jobcount, username+7, status+3);
			fprintf(stderr, 
				"%s:will retry in 15 seconds.\n", 
				ProcName);
			
			sleep(15);
		}

		scratch[0] = 0;
		adsp_write_attn(&end1, 0x0012, scratch, 1);
		if(adsp_read(&end1, (char*)&results2, 2) != 2)
		{
			fprintf(stderr, "%s:failed to read result code from printer.\n", 
				ProcName);
			exit(1);
		}

		adsp_disconnect(&end1);

		if(results == 0 && results2 == 0)
			break;
	}

	do
	{
		result = adsp_listen(&s2, &end2);

		if(result < 0)
		{
			fprintf(stderr, "%s:listen failed.\n", ProcName);
			exit(1);
		}
	} while(result == 0);

	if(adsp_accept(&end2) < 0)
	{
		fprintf(stderr, "%s:accept failed.\n", ProcName);
		exit(1);
	}

	adsp_close_socket(&s1);

	at_printer_setstatus("Printing");

	return(0);
}

void at_printer_kill(void)
{
	char scratch[32];

	/* This whole sequence was determined by examining packet dumps.
		If you don't get it just right, after the disconnect the printer 
		tries to open a new connection to the original control socket.
	*/

	adsp_fwd_reset(&end2);

	/* Make sure there isn't pending input. */
	adsp_read_nonblock(&end2, scratch, 32);

	scratch[0] = 0;
	adsp_write(&end2, scratch, 1);

	comm_printer_writeFFFx('I');

	scratch[0] = 0x00;
	adsp_write_attn(&end2, 0x0012, scratch, 1);
	adsp_read(&end2, scratch, 2);

	adsp_disconnect(&end2);
	adsp_close_socket(&s2);
}

void at_printer_setstatus(char *status)
{
	char buf[257];

	buf[0] = 0x06;
	buf[1] = 0x47;
	buf[2] = strlen(status);
	strncpy(buf + 3, status, buf[2]);

	adsp_write_attn(&end2, 0x000a, buf, 257);
	adsp_read(&end2, buf, 2);
}

#endif
