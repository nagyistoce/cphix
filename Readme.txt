Cphix is CLI tool to normalize images intended primarily for linux.
 
Current version 1.6.4
HOME: http://code.google.com/p/cphix/
(see wiki section there also)


The Cphix2 analyses brightness (average and overall contrast), saturation and sharpness and modifies them.


 =====      Usage     =======
At least one image must be provided, wildcards can be used:
 $cphix.bin i*g myphoto.JPG
 (name of binary can differ)
As by now, "final_" prefix is appended to the filename. Original images are not touched (of course).


 =====   CLI switches =======
 GENERAL CONTROLS
--half                Process only half of image
--skip                Skip image if final image exists.
--png                 Save as 8-bit png, default is jpg
--newname $PATTERN    Naming pattern for final images, % will be replaced
                       by old filename, # by images counter, so with pattern
                       'vacation_#_%', the name IMG_0586.jpg will be changed
                       to 'vacation_0001_IMG_0586.jpg' 
--mpx $DECIMAL        Final size of image in MPx
--version; -v         Prints out version (and proceeds with processing)
--help; -h            Prints help and quits
  DISABLING AN MODIFICATION
--nosat               Do not modify saturation
--nobr                Do not modify brightness (&contrast)
--nosharp             No sharpening (USM)
--norgbalign          Disable RGB alignment
  INSERTING A TEXT
--title $YOURTEXT     Insert text into left bottom corner.
                      (Use '' to encapsulate a text with blanks)
--textsize $FLOAT     Relative size of text (default: 1).
--topacity $FLOAT     Text (label) opacity (0-1,default: 1).
--txpos $FLOAT        Text x (horiz.) position (0-1,default: 1).
--typos $FLOAT        Text y (vert.) position (0-1,default: 1).
                      (Coordinates starts on upper left corner. Relative
                       positions refer to bottom right corner of text label).
  MODIFICATION OF TARGETS
--minbr $DECIMAL      (Brightness and contrast are coupled
--maxbr $DECIMAL       together, so exteme value in any of them
--mincontr $DECIMAL    can lead to undesired results)
--maxcontr $DECIMAL   
--minsat $DECIMAL     Minimal saturation
--minsharp1 $DECIMAL  Minimal sharpness (blur radius 0.1)
--minsharp2 $DECIMAL  Minimal sharpness (blur radius 0.02)



*INSTALLATION:*

LINUX:
(The platfom this is developed on)
Extract and enter cphix directory and run
make
There is no 'make install', you can do whatever you want with compiled binary file and put it somewhere in your path or leave it where it is.

OTHER PLATFORMS:
For windows binaries check on homepage's download: https://code.google.com/p/cphix/downloads/list


License of Cphix: GPLv3

*CONTACT:*
tiborb95 at gmail dot com

Your feedback is WELCOMED !


Updated: 04 Oct 2013
