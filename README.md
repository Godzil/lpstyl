"lpstyl version 0.9.9"

This is an Apple StyleWriter driver for un*x.  It was developed on
NetBSD-mac68k, and has been reported to work on NetBSD-i386,
linux-pmac, mklinux-ppc, and linux-x86.  

Supported printers include:

StyleWriter I
StyleWriter II
StyleWriter III (aka StyleWriter 1200)
Color StyleWriter 1500
Color StyleWriter 2200 
Color StyleWriter 2400 
Color StyleWriter 2500 

If you have some breed of StyleWriter besides the ones mentioned
here, feel free to try it.  The driver should realize that it
doesn't know what sort of printer you have and try to treat it like
a StyleWriter II or 2400, depending on values the printer returns.
Try it and see, and please let me know what happens.

Random notes about this version:

Since the StyleWriter has no built-in fonts, this driver only prints
a couple of types of raster image files at 360 dpi (the standard
printer resolution of StyleWriters).  It is expected that gs will
be used to create these files from PostScript files, but if you
have some other way of creating them, feel free.  The printcap
entry and shell scripts in this kit will allow you to "lpr" a
postscript or ascii text file and have it printed directly (without
ever creating the raster file on disk) by piping output from gs to
the driver.

If the printer runs out of paper, the driver will usually wait
indefinitely for the problem to go away, retrying every 30 seconds.
It will also send messges to the log file to this effect.  On the
2400/2500, the retry will be every 5 seconds, and it won't reset
the printer or resend any data.  I say "usually" because if the
out-of-paper condition is detected at the wrong time, the driver
doesn't quite know how to deal with it.  In these cases, it will
exit with an non-zero status.

If you want to turn the printer off during a long print job, the
driver needs to be informed so that the printer's buffered data
isn't lost.  Sending the driver process a SIGUSR1 will tell it to
suspend printing after the current page finishes.  The driver will
print a line to the lpd log file telling you when it is OK to turn
off the printer.  While printing is suspended, the printer can be
turned off, disconnected, etc.  (You can even connect a different
type of StyleWriter -- when you restart printing, the driver will
identify the printer again.)  Sending another SIGUSR1 will resume
printing with the next page.

The driver program itself is a pure filter -- it depends on lpd to
connect its stdin and stdout to an image file and the tty the
printer is attached to, respectively.  The printcap entry and sample
scripts assume that the printer is attached to /dev/stylewriter,
which whould be a symbolic link to whatever serial device the
printer is connected to.  (In netbsd-mac68k, the printer port is
/dev/tty01.)

The driver program accepts the following flags:

-f <device node>
    Tells the driver to open a device directly instead of using
	stdout to talk to the printer.

-t <file format>
	Tells the driver which file format to expect for input.  Currently
	this can be pbmraw, bit, or bitcmyk.  Using "bitcmyk" implies that
	the driver will attempt to print in color.

-h <height>
-w <width>
	Tells the driver the height and width of the input file.  This will
	be overridden by the embedded dimension data in pbmraw files.
	These parameters are required for bit and bitcmyk files, since they
	have no embedded height and width.

-m
    Disables the normal cropping that is done to take the top and left
	margins into account.  This probably shouldn't be used for properly 
	typeset pages (it will cause them to be shifted down and to the right 
	instead of being properly placed on the page), but it can be useful
	for files that don't take page margins into account, like some of the
	samples that come with ghostscript.

-v
    Increases the amount of debugging information the driver sends
	to stderr.  Multiple v's give more output.  Currently more than
	two doesn't do anything different, but feel free to add as many
	as you like if it makes you happy.  The stderr of the driver is
	normally directed to /var/log/lpd-errs (or whatever file is
	specified in the printcap entry) by lpd.  Watching this file 
	with 'tail -f' can be informative.

-p letter
-p a4
	Sets the paper size the driver expects.  Default is "letter".

-H <height>
-W <width>
	Another way of setting the paper size.  Height and width are in
	printer pixels.  One printer pixel is 1/360 of an inch.  No sanity
	checking is done on the arguments, so be nice.

-?
	Prints a usage message.

If the driver is compiled with AppleTalk support, the following options are
also available:

-a <nbpname>
	Specify the name of the AppleTalk printer you want to use.  This is the
	switch that tells the driver to use AppleTalk instead of serial
	communication.  nbpname can be specified in standard AppleTalk wildcard
	format (properly quoted to avoid shell expansion), but a name which 
	matches multiple devices may produce undesired results.

-u <username>
	Specify the username that will be shown to other users while the printer
	is busy.  This can be any string, but I recommend a proper 'user@host'
	to avoid confusion.  The sample AppleTalk driver scripts show how to 
	do this with the standard parameters that lpd passes to driver programs.

MANIFEST

This kit includes the following files:

README:
	this file
README.atalk:
	notes about using the driver with AppleTalk
README.troubleshooting:
	common questions and answers
README.protocol:
	information about the control protocol StyleWriters use
lpstyl.c: 
    the source for the driver program
adsp.c:
adsp.h:
	an implementation of a subset of the adsp protocol
printcap: 
    a sample printcap file that uses the driver
printcap.a4: 
    a sample printcap file for use with A4 paper
stylps:
stylps-color:
stylascii:
    Shell scripts (used by the printcap example) that plug together 
	GhostScript, enscript, and the printer driver in various ways.  
	Henceforth known as "the driver scripts".
stylps-atalk:
stylps-color-atalk:
stylascii-atalk:
	Similar to the above, but using AppleTalk for communication.
direct_stylpbm:
direct_stylps:
    Shell scripts that take pbmraw and postscript files as standard input 
	and use the lpstyl driver (and ghostscript, for direct_stylps) to 
	send them to a StyleWriter connected to /dev/stylewriter.  Note that 
	you must have read/write access to the tty for this to work.  These 
	are included mostly as examples and for testing purposes.  It's much
	more pleasant to use lpd, once you get it set up.
styl.ppd:
	Experimental ppd file for use with systems that expect one
	(such as netatalk/papd or the LaserWriter 8.5 driver using 'lpd' 
	protocol).  This is the first time I've tried writing one of these 
	things, so corrections are welcome.
control_codes:
	Information about the control protocol the driver uses, written after
	the fact.  If this file and the source code conflict, believe the source.

INSTALLATION

To make this thing work, do the following:

- compile lpstyl.c to create the executable lpstyl
- make sure that the path to gs is correct in stylps (it assumes
    /usr/local/gnu/bin/gs)
- make lpstyl and the driver scripts world-executable and put them 
    somewhere public (I suggest /usr/local/sbin)
- make sure that the path to lpstyl is correct in the driver scripts (they 
    assume /usr/local/sbin/lpstyl, etc.)
- make /dev/stylewriter a symbolic link to the serial port your printer is 
    connected to.  (On netbsd-mac68k, the printer port is /dev/tty01.)
- install the printcap entries into /etc/printcap (making sure that all 
    paths are correct there, too)
- make sure the spool directory specified in the printcap entry exists
- kick lpd so that it re-reads /etc/printcap

That should do it.  Find a postscript file and 'lpr -Pstylps file.ps'  
(Or, if you don't have a postscript file handy, print your favorite 
man page with 
'groff -mandoc /usr/local/share/man/man1/manpage.1 |lpr -Pstylps'.)

CONTACTS

I can be reached via email at monroe@pobox.com.  The latest version
of this package will reside at <http://www.pobox.com/~monroe/styl/>.

If you use this driver successfully, or want to use it but can't
get it to work, feel free to send me email.  If you make improvements
to or have questions about this package, please do send them my
way.

-- Monroe Williams  a.k.a.  monroe@pobox.com --
Copyright 1996-2000 Monroe Williams, all rights reserved.
