Cphix is CLI tool to normalize images intended primarily for linux.
 
Current version 1.6
HOME: http://code.google.com/p/cphix/
(see wiki section there also)


The Cphix2 analyses brightness (average and overall contrast), saturation and sharpness and modifies them.


 =====      Usage     =======
At least one image must be provided, wildcards can be used:
 $cphix.bin i*g myphoto.JPG
 (name of binary can differ)
As by now, "final_" prefix is appended to the filename. Original images are not touched (of course).

 =====   CLI switches =======
--half                Process only half of image
--nosat               Do not modify saturation
--nobr                Do not modify brightness (&contrast)
--nosharp             No sharpening (USM)
--version; -v         Prints out version (and proceeds with processing)
--title $YOURTEXT     Insert text into left bottom corner.
                      (Use '' to encapsulate a text with blanks)
--textsize $FLOAT     Relative size of text (default: 1).
--skip                Skip image if final image exists.
--mpx $DECIMAL        Final size of image in MPx
--minsat $DECIMAL     Modifies bottom saturation target
--minsharp $DECIMAL   Modifies bottom sharpness target
--png                 Save as 8-bit png, default is jpg
--newname $PATTERN    Naming pattern for final images, * will be replaced
                       by old filename, # by images counter, so with pattern
                       'vacation_#_*', the name IMG_0586.jpg will be changed
                       to 'vacation_0001_IMG_0586.jpg' 
--help; -h            Prints help and quits



*INSTALLATION:*

LINUX:
(The platfom this is developed on)
Extract and enter cphix directory and run
make
There is no 'make install', you can do whatever you want with compiled binary file and put it somewhere in your path or leave it where it is.

OTHER PLATFORMS:
Sorry, perhaps some volunteer could help with compiling windows binary


License of Cphix: GPLv3

*CONTACT:*
tiborb95 at gmail dot com

Your feedback is WELCOMED !


Updated: 20 Sep 2013
