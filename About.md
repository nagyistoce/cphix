**What is CPhix?**

Command line tool for automatic analysis and enhancement of photos. It analyses and modifies saturation, sharpness and brightness

**Why is CPhix different?**

It **analyses each image**. It **calculates saturation, sharpness and current distribution of brightness**. And based on results it applies changes. Most of similar programs applies user-defined changes to all images, so you have to group similar images together and tell application what to do. Here, you dont say almost anything (still there is few switches) and program will "normalize" images for you.


**Intended use**

When you have huge number of images and needs to process them without spending time on it. Typically vacation photos and so on.

The tool can also **downscale**, **label** and **auto-rotate** (based on exif information).

**Supported formats**

Input images are opened by [Cimg (C Imaging Library) ](http://cimg.sourceforge.net/) so number of supported formats are huge.

As for output - you can choose from jpg (quality 92 % - this is default, and 8-bit png). Adding more formats is trivial, just let me know.

**Logic of processing:**

Well all is just a mathematics. The cphix attemtps to calculate (average/overall) level of brightness, contrast, saturation and sharpness and compare the value with target range and apply modifications. For better understanding here is an example:


```
$cphix2.bin /path/to/i*g
[many images processed here]
==> Image  17/17: img_4742.jpg    ( 2048 x 1536)
    (rotating, original orientation: 8)
  Brightness gamma: 1.04, change: 0.68->0.68 (target: 0.45-0.68)
  Contrast gamma  : 1.00, change: 0.43->0.43 (target: 0.30-0.45)
  Sharpness boost : 1.35, change: 0.09->0.12 (target: 0.12-0.30)
  Saturation gamma: 0.66, change: 0.04->0.18 (target: 0.18-0.40)
==SUMMARY (per change - count of images):
   Type of modif.       | Decreased | Increased |Not changed|
   Brightness           |         4 |         2 |         11|
   Contrast             |         6 |         5 |          6|
   Saturation           |         0 |        17 |          0|
   Sharpness            |         0 |         7 |         10|
```

Look at "change" part of lines, there you can see old value and new value and how they fits into target range.
At the end there is and summary of processed images.

**Switches:**

Well, use 'cphix.bin -h' to see switches as avaiable by now....

**Final files naming**

By default the cphix attaches "final`_`" to the filename, but you can use own names or pattern of names. There are two wildcards: '`*`' - will be replaced by original filename (without extension) and '#' will be replaced by counter number of image (0001, 0002 and so on).

So following --newname 'Vacation\_2013`_`#' defines output names like:  Vacation\_2013\_0001.jpg, Vacation\_2013\_0002.jpg and so on... (since v. 1.6)


**Limitations:**

Well, this is not finished yet and there are couple of features that are to be applied yet. F.e. (but there are many more):

  1. the algorithm is not designed and/or adjusted to work with human skine...
  1. it does not do white-point balancing
  1. algorithm does not cope well with images where bis areas are "empty" - either gray or flat (even brightness).
  1. it cannot do miracles (like recovering lost blue on sky)

**Color-space theory**

Well I spent quite lot of time investigating and learning about colorspaces. I noticed that colorspace you work in matters. Especially it matters for brightness and saturation. As I need to put everything into numbers, everything must be proportional. F.e. saturation 0.1 must be roughly double (visually) of saturation of 0.05 and so on. I gave up on using CIELUV and similar colorspaces as I was not willing to re-code all the formulas from the scratch. I liked also [Munsell color system](http://en.wikipedia.org/wiki/Munsell_color_system), but either there are no formulas or they are secret or so...

At the end I ended with this rough color-space that can be depicted on this
[image](https://cphix.googlecode.com/files/ColorSpace-v1.3.png). Here you can see f.e. that not colors are equal in regard to highes achievable saturation and highest saturation depends on brightness. The colorspace should be depicted as 3D object, but I used random colors instead and put them into 2D.


**Dependencies**

Well, the main dependency is [Cimg (C Imaging Library) ](http://cimg.sourceforge.net/). If you are using linux, chances are that is separate package cimg-dev with header files. You need this one as well.

The other one is [easyefix (EXIF parsing library)](https://code.google.com/p/easyexif/). However this one is packed in the tarbal.

**Download of sources**

The source can be downloaded from download section here, or if you have subversion installed, you can download it with command:

```
svn checkout http://cphix.googlecode.com/svn/tags/1.6 .
```

It will put files into current directory.


**Installation&compilation on Linux**

  1. To install it you need to have basic compilation environment installed (usually this is a meta-package in packaging system) and the other package needed is Cimg (including cimg-dev package if there is such package in repositories). (Note for linux newbies - all needed should be available in repositories unless you are using very obscure distribution)
  1. Download and extract the tarball from Downloads section here, extract it and go to cphix directory
  1. run 'make'
  1. the binary is named cphix2.bin, this is standalone binary so you can copy it elsewhere

In case you are an **Arch Linux** user, there is a [PKGBUILD](https://aur.archlinux.org/packages/cphix/)

**Installation&compilation on Windows**

There is installation package for windows as well in Downloads section

**Examples**

For examples skip to this wiki page with [Examples](Examples.md).

**Participation**

I would welcome a feedback. For example - photos that gave strange results and so on.
But I would be glad for any even small issues you had with this tool.

**Supported platforms**

It is intended for linux, but in theory it should work also on other unix-like systems and windows as well.

Contact: tiborb95 at gmail dot com





<br><br><br><br><br><br><br><p align='right'>
<a href='https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=GBERSY833N4AA'><img src='https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif' border='0'></img></a>