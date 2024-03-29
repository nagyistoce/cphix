// Cphix2 - tool to normalize images - modifies saturation, brightness, sharpness 
// and distribution of brightness
// look for 'const char *version=' to find out version :)
// 
// It is command line tool working in batch mode.
// Intended platform: Linux (though it should work elsewhere)
// To compile run:
// ./configure.sh
// make
// No 'make install' though, just put the output binary (cphix2.bin) to appropriate directory.
// Needed dependencies: gcc (obviously) and Cimg (C Image Library)
// strongly reccomended: libjpeg-dev (cimg's native support for jpeg is not reliable)
// alse review the output of configure.sh script to lear about other optional dependencies
//
//
// Contact: tiborb95 at gmail dot com
// 

#include <iostream>
using namespace std;
#include <vector>

//#define cimg_use_jpeg
//#define cimg_use_png
//#define cimg_use_tiff
#include <CImg.h>
using namespace cimg_library;
#include <stdio.h> 
#include <stdlib.h>  
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <math.h>
#include "easyexif/exif.h"
#include <errno.h>

#define RWEIGHT 0.3
#define GWEIGHT 0.5
#define BWEIGHT 0.2
#define POINTSPERLINE 15
#define FALSE 0
#define TRUE 1
#define BRIGHT 0
#define SHARP 1
#define SATUR 2
#define IMAGE 100
#define MASK 101
#define JPG 50
#define PNG 51
#define PARSINGSUCCESS 61
#define PARSINGFAILED 62
#define RTARGET 0 //0.01
#define GTARGET 0 //0.01
#define BTARGET 0 //-0.04


static 	vector<string> images; 
static int img_count=0;
static float *r,*g,*b, *br, *sat, *mask1, *tmp1;   //,*r_blur,*g_blur,*b_blur; sat taking from raw values now
static float *r_orig,*g_orig,*b_orig, *br_orig; // to save orignal values
static float sample_br[POINTSPERLINE*POINTSPERLINE];
static float sample_sat[POINTSPERLINE*POINTSPERLINE];
static float sample_sharp1[POINTSPERLINE*POINTSPERLINE];
static int allocated=0;
static int br_stat[3]={0,0,0};
static int contr_stat[3]={0,0,0};
static int sharp_stat1[3]={0,0,0};
static int sharp_stat2[3]={0,0,0};
static int sat_stat[3]={0,0,0};
static int rgbalign_stat=0;
void help ();
static string filename; 
const char *version="1.7.4";
//const float remaptable[7]={1.3,0.7,1.1,0.8,1.4,0.9,1.3};
//const float remaptable[7]={1.3,0.7,1.4,0.8,1.4,0.9,1.3}; // v 1.6
const float remaptable[7]={1.0,0.8,0.9,0.7,1.0,0.7,1.0}; // v 1.7

static float minofmaxr=1,minofmaxg=1,minofmaxb=1,maxofminr=-1,maxofming=-1,maxofminb=-1,rmiddle=0,gmiddle=0,bmiddle=0;
//minofmaxr=1;minofmaxg=1;minofmaxb=1;maxofminr=-1;maxofming=-1;maxofminb=-1;

void get_brightness(float value, float br_gamma, float contr_gamma, float *new_br, float *new_contr, float overallbr=0.5);
void get_ordered(float *array,int length);
float get_waverage(float *array,float gamma=1, float startpos=0);
float get_average(float *array,int count);
float calculate_brightness(float r,float g, float b);
void resaturate(int basepos, float saturation_gamma, float *newr,float *newg,float *newb,float *applied_sat);
int test_file(const char* file);
float my_pow(float base,float exp);
int parse_float(const char* argument,float downlimit, float uplimit, float *result, const char argument2,const char *text);

typedef struct
{
	float minbr;
	float maxbr;
	float mincontr;
	float maxcontr;
	bool halfmode;
	float minsat;
	float maxsat;
	float minsharp1;
	float maxsharp1;
	float minsharp2;
	float maxsharp2;
	bool nobr;
	bool nosat;
	bool savemask;
	bool nosharp;
	float mpx_resize;
	int output;
	bool rgbalign;
	float xpos;
	float ypos;
	float topacity;
	bool bw;
	float textsize ;
	bool insertingtext;
	bool skip;
	bool patterndefined;
	int rotation;
	int source_x_size;
	int source_y_size;
	int result_x_size;
	int result_y_size;
	int source_size;
	char *title;
	char *pattern;
	bool savedata;
	float rdiff;   // fro rgb alignment
	float gdiff;
	float bdiff;
	bool savelabel;
	float toffset; // ratio to core label size
} MyMainVals;
static MyMainVals  maindata; //={,0,JPG,TRUE,0.95,0.92,1,FALSE,1,FALSE,FALSE};

void set_initial(){
maindata.minbr=0.45  ;
maindata.maxbr=0.73  ;
maindata.mincontr=0.25  ;
maindata.maxcontr=0.5  ;
maindata.halfmode=FALSE  ;
maindata.minsat=0.2  ;
maindata.maxsat=0.4  ;
maindata.minsharp1=0.35  ;
maindata.maxsharp1=1  ;
maindata.minsharp2=0.30  ;
maindata.maxsharp2=1  ;
maindata.nobr=FALSE  ;
maindata.nosat=FALSE  ;
maindata.savemask=FALSE  ;
maindata.nosharp=FALSE  ;
maindata.mpx_resize=0  ;
maindata.output=JPG  ;
maindata.rgbalign=TRUE  ;
maindata.xpos=0.95  ;
maindata.ypos=0.92  ;
maindata.topacity=1  ;
maindata.bw=FALSE  ;
maindata.textsize=1;
maindata.skip=FALSE  ;
maindata.patterndefined=FALSE  ;
maindata.savedata=FALSE;
maindata.savelabel=FALSE;
maindata.toffset=1.0;
maindata.insertingtext=FALSE;}


void arg_processing ( int argc, char** argv){
	//creating OVERLAY ARRAY to mark processed CLI arguments
	bool *shadow_argv;
	int filetestresult;
	float tmpfloat;
	shadow_argv= new bool[argc]; // creating bool array that identifies parsed arguments
	for (int n=1;n<argc;n=n+1){ // setting defaul false values
		shadow_argv[n] = false;}
	
	//PROCESSING CLI ARGUMENTS
	for (int n=1;n<argc;n=n+1) { 
		if (shadow_argv[n]) {continue;} //if identified as parsed, breaking
		if (strcmp (argv[n],"--half") == 0 ) { 
			cout << " * Doing only half image \n";
			maindata.halfmode=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--nobr") == 0 ) { 
			cout << " * No brightness modifications to be done \n";
			maindata.nobr=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--nosat") == 0 ) { 
			cout << " * No saturation modifications to be done \n";
			maindata.nosat=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--nosharp") == 0 ) { 
			cout << " * No sharpening to be done \n";
			maindata.nosharp=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--png") == 0 ) { 
			cout << " * Saving as 8-bit png \n";
			maindata.output=PNG;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--bw") == 0 ) { 
			cout << " * B/W output\n";
			maindata.bw=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--norgbalign") == 0 ) { 
			cout << " * No RGB align\n";
			maindata.rgbalign=FALSE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--savedata") == 0 ) { 
			cout << " * Saving data\n";
			maindata.savedata=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--savelabel") == 0 ) { 
			cout << " * Saving label\n";
			maindata.savelabel=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--skip") == 0 ) { 
			cout << " * Skipping if final image exists \n";
			maindata.skip=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--version") == 0 || strcmp (argv[n],"-v") == 0 ) { 
			printf (" * Version: %s\n",version);
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--savemask") == 0  || strcmp (argv[n],"--savemasks") == 0) { 
			cout << " * Saving blurred masks \n";
			maindata.savemask=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--title") == 0 || strcmp (argv[n],"--text") == 0 ) { 
			if (shadow_argv[n+1]==false && n+1<argc) {
				maindata.title=argv[n+1];
				printf (" * Text to be inserted: %s \n",maindata.title);
				maindata.insertingtext=TRUE;
				shadow_argv[n]=true; shadow_argv[n+1]=true; }
			else {
				cout << " --title needs a string. \n";
				shadow_argv[n]=true; }}
		if (strcmp (argv[n],"--newname") == 0 || strcmp (argv[n],"--outfile") == 0 ) { 
			if (shadow_argv[n+1]==false && n+1<argc) {
				//strncmp(argv[n+1],maindata.pattern,19);
				maindata.pattern=argv[n+1];
				printf (" * Pattern used for new filenames: %s \n",maindata.pattern);
				maindata.patterndefined=TRUE;
				shadow_argv[n]=true; shadow_argv[n+1]=true; }
			else {
				cout << " --newname needs a string. \n";
				shadow_argv[n]=true; }}
		if (strcmp (argv[n],"--textsize") == 0 || strcmp (argv[n],"--tsize") == 0) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				maindata.textsize=atof(argv[n+1]);
				if (maindata.textsize==0)
					printf (" ! Check the argument for --textsize  \n");
				else if (maindata.textsize<0.5 || maindata.textsize>5)
					printf (" ! --textsize got value out of allowed range (0,5-5), ignoring... \n");
				else 
					printf (" * Text label will be rescaled: %.2fx \n",maindata.textsize);
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --textsize needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}	

				
		if (strcmp (argv[n],"--txpos") == 0 ) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				parse_float(argv[n+1],0,1,&maindata.xpos,*argv[n],"Label x position");
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --txpos needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}	
		if (strcmp (argv[n],"--typos") == 0 ) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				parse_float(argv[n+1],0,1,&maindata.ypos,*argv[n],"Label y position");
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --typos needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}				
			


		if (strcmp (argv[n],"--minbr") == 0 ) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				parse_float(argv[n+1],0,1,&maindata.minbr,*argv[n],"Minimal brightness");
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --minbr needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}				
		if (strcmp (argv[n],"--maxbr") == 0 ) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				parse_float(argv[n+1],0,1,&maindata.maxbr,*argv[n],"Maximal brightness");
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --maxbr needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}				
		
		//contr				
		if (strcmp (argv[n],"--mincontr") == 0 ) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				parse_float(argv[n+1],0,1,&maindata.mincontr,*argv[n],"Minimal contrast");
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --mincontr needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}				
		if (strcmp (argv[n],"--maxcontr") == 0 ) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				parse_float(argv[n+1],0,1,&maindata.maxcontr,*argv[n],"Maximal contrast");
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --maxcontr needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}				
			
		// --mpx		
		if (strcmp (argv[n],"--mpx") == 0 || strcmp (argv[n],"--MPx") == 0) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				tmpfloat=atof(argv[n+1]);
				//maindata.mpx_resize=atof(argv[n+1]);
				if (tmpfloat==0)
					printf (" ! Check the argument for --mpx  \n");
				else if (tmpfloat<0.01 || tmpfloat>15){
					printf (" ! --mpx got value out of allowed range (0,01-15), no resize... \n");
					maindata.mpx_resize=0;}	
				else {
					maindata.mpx_resize=tmpfloat;
					printf (" * Image(s) will be resized to: %.2f MPx \n",maindata.mpx_resize);}
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --mpx needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}				
		// --minsat
		if (strcmp (argv[n],"--minsat") == 0 ) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				tmpfloat=atof(argv[n+1]);
				if (tmpfloat==0)
					printf (" ! Check the argument for --minsat  \n");
				else if (tmpfloat<0.01 || tmpfloat>0.5)
					printf (" ! --minsat got value out of allowed range (0,01-0.5), using default... \n");
				else {
					maindata.minsat=tmpfloat;
					printf (" * Minimal saturation set to: %.2f \n",maindata.minsat);}
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --mpx needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }
				if (maindata.maxsat<maindata.minsat+0.05)maindata.maxsat=maindata.minsat+0.05; }
		//text opacity		
		if (strcmp (argv[n],"--topacity") == 0 ) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				tmpfloat=atof(argv[n+1]);
				if (tmpfloat==0)
					printf (" ! Check the argument for --topacity  \n");
				else if (tmpfloat<0.001 || tmpfloat>1)
					printf (" ! --topacity got value out of allowed range (0.001-1), using default... \n");
				else {
					maindata.topacity=tmpfloat;
					printf (" * Title opacity set to: %.2f \n",maindata.topacity);}
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --topacity needs an parameter (float) \n";
				shadow_argv[n]=true; }
			}
		//text shadow
		if (strcmp (argv[n],"--tshadow") == 0 || strcmp (argv[n],"--tshad") == 0) {	
			if (shadow_argv[n+1]==false && n+1<argc) {
				parse_float(argv[n+1],0.1,10,&maindata.toffset,*argv[n],"Text shadow (relative size)");
				shadow_argv[n]=true;shadow_argv[n+1]=true; 
				}
			else {
				cout << " --tshadow needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}	
					
		// --minsharp1
		if (strcmp (argv[n],"--minsharp1") == 0 ) {	
			if (shadow_argv[n+1]==false && n+1<argc) {
				parse_float(argv[n+1],0,1,&maindata.minsharp1,*argv[n],"Minimal sharpness (radius 0.1)");
				shadow_argv[n]=true;shadow_argv[n+1]=true; 
				if (maindata.maxsharp1<maindata.minsharp1+0.05) maindata.maxsharp1=maindata.minsharp1+0.05;}
			else {
				cout << " --minsharp1 needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}	
		// --minsharp2
		if (strcmp (argv[n],"--minsharp2") == 0 ) {	
			if (shadow_argv[n+1]==false && n+1<argc) {
				parse_float(argv[n+1],0,1,&maindata.minsharp2,*argv[n],"Minimal sharpness (radius 0.01)");
				shadow_argv[n]=true;shadow_argv[n+1]=true; 
				if (maindata.maxsharp2<maindata.minsharp2+0.05) maindata.maxsharp2=maindata.minsharp2+0.05;}
			else {
				cout << " --minsharp2 needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }}			
		
					
				
		if (strcmp (argv[n],"--help") == 0 || strcmp (argv[n],"-h") == 0) {
	   	    help ();exit (1); }
		}
	   	    
	   	    
   	//checking remaining argumets to make sure they are files  
    string qarg;
   	for (int n=1;n<argc;n=n+1) {		//iterating over (all) arguments
   		if (shadow_argv[n]) {continue;} //skip if agrument was recognized before

		filetestresult=test_file(argv[n]);
		
		if(filetestresult==0) {
			images.push_back(argv[n]); 		//source image exists
   			img_count=img_count+1;}
   		else if (filetestresult==13) {
			printf (" ! File %s is not readable...\n",argv[n]);}
		else if (filetestresult==2) {
			printf (" ! Unrecognized argument: %-15s Use \"-h\" for info about switches.\n",argv[n]);}
   		else  {
			printf (" ! Error %1d when trying to open file %s...\n",filetestresult,argv[n]);}		
		}
   	
   	}

void help (){
	cout << "\n =====  What is Cphix ====== \n";
	cout << "Cphix is command line tool to enhance photos (images in generall), it measures saturation, sharpness and brightness and changes them based on predefined target values.\n";
	printf ("Current version: %s\n",version);
	cout << "HOME: http://code.google.com/p/cphix/\n";

	cout << "\n =====      Usage     =======\n";
	cout << "At least one image must be provided, wildcards can be used:\n";
	cout << " $cphix.bin i*g myphoto.JPG\n";
	cout << " (name of binary can differ)\n";
	cout << "By default \"final_\" prefix is appended to the filename. Original images are not touched (of course).\n";	
	cout << "\n =====   CLI switches =======\n";
	printf("  GENERAL CONTROLS\n");	
	printf("%-20s  %s\n", "--half", "Process only half of image");
	printf("%-20s  %s\n", "--skip", "Skip image if final image exists.");
	printf("%-20s  %s\n", "--png", "Save as 8-bit png, default is jpg");
	printf("%-20s  %s\n", "--newname $PATTERN", "Naming pattern for final images, % will be replaced");
	printf("%-20s  %s\n", " "				  , " by old filename, # by images counter, so with pattern");
	printf("%-20s  %s\n", " "				  , " 'vacation_#_%', the name IMG_0586.jpg will be changed");
	printf("%-20s  %s\n", " "				  , " to 'vacation_0001_IMG_0586.jpg' ");
	printf("%-20s  %s\n", "--mpx $DECIMAL", "Final size of image in MPx");
	printf("%-20s  %s\n", "--version; -v", "Prints out version (and proceeds with processing)");
	printf("%-20s  %s\n", "--help; -h", "Prints help and quits");
	
	printf("  DISABLING AN MODIFICATION\n");		
	printf("%-20s  %s\n", "--nosat", "Do not modify saturation");
	printf("%-20s  %s\n", "--nobr", "Do not modify brightness (&contrast)");
	printf("%-20s  %s\n", "--nosharp", "No sharpening (USM)");
	printf("%-20s  %s\n", "--norgbalign", "Disable RGB alignment");		

	printf("  INSERTING A TEXT\n");
	printf("%-20s  %s\n", "--title $YOURTEXT", "Insert text into left bottom corner.");
	printf("%-20s  %s\n", " ","(Use '' to encapsulate a text with blanks)");
	printf("%-20s  %s\n", "--textsize $FLOAT", "Relative size of text (default: 1).");
	printf("%-20s  %s\n", "--topacity $FLOAT", "Text (label) opacity (0-1,default: 1).");
	printf("%-20s  %s\n", "--txpos $FLOAT", "Text x (horiz.) position (0-1,default: 1).");	
	printf("%-20s  %s\n", "--typos $FLOAT", "Text y (vert.) position (0-1,default: 1).");	
	printf("%-20s  %s\n", " ", "(Coordinates starts on upper left corner. Relative");	
	printf("%-20s  %s\n", " ", " positions refer to bottom right corner of text label).");		
	printf("%-20s  %s\n", "--tshadow $FLOAT", "Thickness of shadow (0.1-10,1=default)");	

	
	printf("  MODIFICATION OF TARGETS\n");
	printf("%-20s  %s\n", "--minbr $DECIMAL", "(Brightness and contrast are coupled");
	printf("%-20s  %s\n", "--maxbr $DECIMAL", " together, so exteme value in any of them");
	printf("%-20s  %s\n", "--mincontr $DECIMAL", " can lead to undesired results)");
	printf("%-20s  %s\n", "--maxcontr $DECIMAL", "");	
	printf("%-20s  %s\n", "--minsat $DECIMAL", "Minimal saturation");
	printf("%-20s  %s\n", "--minsharp1 $DECIMAL", "Minimal sharpness (blur radius 0.1)");
	printf("%-20s  %s\n", "--minsharp2 $DECIMAL", "Minimal sharpness (blur radius 0.01)");

	printf("\n");
	}

inline int get_basepos(int x, int y, int width){
	return y*width + x;}



int parse_float(const char* argument,float downlimit, float uplimit, float *result,const char argument2,const char *text){
	float tmp;
	char * e;

	tmp=(float)strtod(argument, &e);   //  atof(&argument);
	if (*e != 0) {
		printf (" ! Check the argument for %s  \n",text);
		return PARSINGFAILED;}
	else if (tmp<downlimit || tmp>uplimit){
		//cout << 
		printf (" ! %s got value out of allowed range (%.2f-%.2f), using default... \n",text,downlimit,uplimit);
		return PARSINGFAILED;}
	else {
		*result=tmp;
		printf (" * %s set to: %.2f \n",text,*result);
		return PARSINGSUCCESS;}
	}
	
	
	
	


void take_samples(int action,float uptresh=10){
	int x,y,xpoint,ypoint,basepos,halfstepx,halfstepy,basepos_sample;
	const bool debug=FALSE;
	
	halfstepx=maindata.source_x_size/POINTSPERLINE/2.0;
	halfstepy=maindata.source_y_size/POINTSPERLINE/2.0;
	for (xpoint=0;xpoint<POINTSPERLINE;xpoint+=1) {for (ypoint=0;ypoint<POINTSPERLINE;ypoint+=1) {	
		x=halfstepx+xpoint*maindata.source_x_size/POINTSPERLINE;
		y=halfstepy+ypoint*maindata.source_y_size/POINTSPERLINE;
		//printf (" Testing pixel %4d x %4d\n",x,y);
		basepos_sample=get_basepos(xpoint,ypoint,POINTSPERLINE);
		basepos=get_basepos(x,y,maindata.source_x_size);
		if (action==BRIGHT)
			{sample_br[basepos_sample]=br[basepos];
			sample_sat[basepos_sample]=sat[basepos];
			}
		if (action==SHARP){
			sample_sharp1[basepos_sample]=abs(br[basepos]-mask1[basepos])/mask1[basepos];
			if (sample_sharp1[basepos_sample]>uptresh) {
				if (debug) printf(" Cropping value: %.3f to %.3f\n",sample_sharp1[basepos_sample],uptresh);
				sample_sharp1[basepos_sample]=uptresh;
				}
			}
		if (action==SATUR)
			{
			sample_sat[basepos_sample]=sat[basepos];
			}
		}}
	
}


void blur(const float *input, float *output,const float relradius,const int x_dim,const int y_dim,int callingline){
	int steps[10]={0,0,0,0,0,0,0,0,0,0};
	int cummulative, expected;
	float sum;
	int count,radius,i,j,x,y,basepos,basepos_local;
	const bool debug=FALSE;
	const bool verbose=FALSE;
	
	if (verbose==TRUE) printf( " Starting blur(), calling from line: %4d\n",callingline);
	
	if(relradius>=1.0) radius=(int)relradius; // the value is to be takes as absolute in pixels
	else {
		radius=((x_dim+y_dim) * relradius);
		if (radius>500) radius=500;
		if (radius<1) radius=1;}
	if (verbose==TRUE) printf ( "   Calculated absolute radius for blur: %2d (received: %.2f)\n",radius,relradius);
	
	steps[0]=1;cummulative=1;
	for (i=1;i<10;i+=1) {
		expected=steps[i-1]*1.8+0.3;
		if (expected> (radius-cummulative)) expected=radius-cummulative;
		steps[i]=expected;
		cummulative+=expected;
		if (verbose==TRUE) printf( "   subradius for step: %1d: %3d\n",i,steps[i]);
	}
	
	//blurring itself
	//the proceeding is we blur from tmp1 to mask1
	for (j=0;j<x_dim*y_dim;j+=1) output[j]=input[j];
	
	for (i=9;i>=0;i-=1){
		if (verbose==TRUE) printf ("  step: %2d: radius: %2d\n",i,steps[i]);
		if (steps[i]==0) continue;
		//shifting data
		for (j=0;j<x_dim*y_dim;j+=1) tmp1[j]=output[j];		
		
		//blurring the pixel
		for (x=0;x<x_dim;x+=1) { for (y=0;y<y_dim;y+=1) {

			basepos=get_basepos(x,y,x_dim);
			if (debug && x%1000==0 && y%1000==0) printf ("    pixel: %3d x %3d, iteration: %2d, step radius: %2d, original value: %.3f\n",
			x,y,i,steps[i],tmp1[basepos]);
			sum=tmp1[basepos];count=1;
			if (x-steps[i]>=0) {
				basepos_local=get_basepos(x-steps[i],y,x_dim);
				sum+=tmp1[basepos_local];
				count+=1;}
			if (x+steps[i]<x_dim) {
				basepos_local=get_basepos(x+steps[i],y,x_dim);
				sum+=tmp1[basepos_local];
				count+=1;}
			if (y-steps[i]>=0) {
				basepos_local=get_basepos(x,y-steps[i],x_dim);
				sum+=tmp1[basepos_local];
				count+=1;}
			if (y+steps[i]<y_dim) {
				basepos_local=get_basepos(x,y+steps[i],x_dim);
				sum+=tmp1[basepos_local];
				//if (debug) printf(" adding pixel: %3d x %3d, value: %.3f\n",y+steps[i]
				count+=1;}
			output[basepos]=sum/count;
			if (debug&& x%1000==0 && y%1000==0) printf("    result: %3f / %1d = %.3f\n", sum,count, output[basepos]);
			}} //end of blur step
	}// end of all iterations
	
}


void blur_wrong(const float *input, float *output,const float relradius,const int x_dim,const int y_dim,int callingline){
	int steps[10]={0,0,0,0,0,0,0,0,0,0};
	int cummulative, expected;
	//float sum;
	int radius,i,j,x,y,basepos_local;
	const bool debug=FALSE;
	const bool verbose=FALSE;
	int *counts;
	int basepos_diff;
	counts = new int[x_dim*y_dim];
	
	if (verbose) printf( " Starting blur(), calling from: %4d\n",callingline);
	
	if(relradius>=1.0) radius=(int)relradius; // the value is to be takes as absolute in pixels
	else {
		radius=((x_dim+y_dim) * relradius);
		if (radius>500) radius=500;
		if (radius<1) radius=1;}
	if (verbose==TRUE) printf ( "   Calculated absolute radius for blur: %2d (received: %.2f)\n",radius,relradius);
	
	steps[0]=1;cummulative=1;
	for (i=1;i<10;i+=1) {
		expected=steps[i-1]*1.8+0.3;
		if (expected> (radius-cummulative)) expected=radius-cummulative;
		steps[i]=expected;
		cummulative+=expected;
		if (verbose==TRUE) printf( "   calculated radius for step: %1d: %3d\n",i,steps[i]);
	}
	
	//blurring itself
	//copying and populating counts
	for (j=0;j<x_dim*y_dim;j+=1) {
		output[j]=input[j];
		counts[j]=0;}
	
	for (i=9;i>=0;i-=1){
		if (verbose==TRUE) printf ("  blurring step: %2d: radius: %2d\n",i,steps[i]);
		if (steps[i]==0) continue;
		
		//copying data from output to tmp1
		for (j=0;j<x_dim*y_dim;j+=1){
			tmp1[j]=output[j];
			counts[j]=1;}
		
		
		//passing values west->east
		basepos_diff=get_basepos(steps[i],0,x_dim)-get_basepos(0,0,x_dim);
		for (x=0;x<x_dim-steps[i];x+=1) { for (y=0;y<y_dim;y+=1) {
			basepos_local=get_basepos(x,y,x_dim);
			output[basepos_local+basepos_diff]+=tmp1[basepos_local];
			counts[basepos_local+basepos_diff]+=1;}}
		//passing values east->west
		basepos_diff=get_basepos(0,0,x_dim)-get_basepos(steps[i],0,x_dim);
		for (x=steps[i];x<x_dim;x+=1) { for (y=0;y<y_dim;y+=1) {
			basepos_local=get_basepos(x,y,x_dim);
			output[basepos_local+basepos_diff]+=tmp1[basepos_local];
			counts[basepos_local+basepos_diff]+=1;}}
		//passing values north->south
		basepos_diff=get_basepos(0,steps[i],x_dim)-get_basepos(0,0,x_dim);
		for (x=0;x<x_dim;x+=1) { for (y=0;y<y_dim-steps[i];y+=1) {
			basepos_local=get_basepos(x,y,x_dim);
			output[basepos_local+basepos_diff]+=tmp1[basepos_local];
			counts[basepos_local+basepos_diff]+=1;}}
		//passing values sout->north
		basepos_diff=get_basepos(0,0,x_dim)-get_basepos(0,steps[i],x_dim);
		for (x=0;x<x_dim;x+=1) { for (y=steps[i];y<y_dim;y+=1) {
			basepos_local=get_basepos(x,y,x_dim);
			output[basepos_local+basepos_diff]+=tmp1[basepos_local];
			counts[basepos_local+basepos_diff]+=1;}}			
		
		//calculating new values in output
		for (j=0;j<x_dim*y_dim;j+=1){
			output[j]=output[j]/counts[j];
			if (debug && isnan(output[j])){ printf (" nan found\n");
											exit(1);}
			}
		if (debug) printf ("   new value for pixel 1   : %.3f (count: %1d)\n",output[0],counts[0]);
		if (debug) printf ("   new value for last pixel: %.3f (count: %1d)\n",output[x_dim*y_dim-1],counts[x_dim*y_dim-1]);		
	}// end of all iterations
	if (verbose) printf( " deleting counts\n");	
	delete[] counts;
	if (verbose) printf( " Ending blur()\n");
}		
		
		
		
		
		////blurring the pixel
		//for (x=0;x<x_dim;x+=1) { for (y=0;y<y_dim;y+=1) {

			//basepos=get_basepos(x,y,x_dim);
			//if (debug && x%1000==0 && y%1000==0) printf ("    pixel: %3d x %3d, iteration: %2d, step radius: %2d, original value: %.3f\n",
			//x,y,i,steps[i],tmp1[basepos]);
			//sum=tmp1[basepos];count=1;
			//if (x-steps[i]>=0) {
				//basepos_local=get_basepos(x-steps[i],y,x_dim);
				//sum+=tmp1[basepos_local];
				//count+=1;}
			//if (x+steps[i]<x_dim) {
				//basepos_local=get_basepos(x+steps[i],y,x_dim);
				//sum+=tmp1[basepos_local];
				//count+=1;}
			//if (y-steps[i]>=0) {
				//basepos_local=get_basepos(x,y-steps[i],x_dim);
				//sum+=tmp1[basepos_local];
				//count+=1;}
			//if (y+steps[i]<y_dim) {
				//basepos_local=get_basepos(x,y+steps[i],x_dim);
				//sum+=tmp1[basepos_local];
				////if (debug) printf(" adding pixel: %3d x %3d, value: %.3f\n",y+steps[i]
				//count+=1;}
			//output[basepos]=sum/count;
			//if (debug&& x%1000==0 && y%1000==0) printf("    result: %3f / %1d = %.3f\n", sum,count, output[basepos]);
			//}} //end of blur step

	
//}

void change_brightness(float *array, float br_gamma, float contr_gamma){
	float new_br,new_contr;
	int x,y,basepos;
	
	for (x=0;x<maindata.source_x_size;x+=1) {
		for (y=0;y<maindata.source_y_size;y+=1) {	
			basepos=get_basepos(x,y,maindata.source_x_size);		
			get_brightness(array[basepos],br_gamma,contr_gamma,&new_br,&new_contr);
			array[basepos]=new_br;
		}}
	
	}

//void apply_gamma(float *array, float gamma){
	//int x,y,basepos;
	
	//for (x=0;x<maindata.source_x_size;x+=1) {
		//for (y=0;y<maindata.source_y_size;y+=1) {
			//basepos=get_basepos(x,y,maindata.source_x_size);
			//array[basepos]=pow(array[basepos],gamma);}
	//}
	//}

float linear_calibrate(float *array,const float minlimit,const float maxlimit,const float pre_gamma,const char *title,float tresh, float *newvalue,const float valuetresh=0){
	//int i,good,outofrange;
	float avg,boost;
	float oldvalue;

	
	//testing if values are big enough for meaningfull processing
	//printf ("Effect of valuetresh: %.3f vs. %.3f\n",get_waverage(&array[0],pre_gamma,valuetresh),get_waverage(&array[0],pre_gamma)	);
	avg=get_waverage(&array[0],pre_gamma,valuetresh);
	oldvalue=avg;
	if (avg<0.01) {
		printf ("  Image with extremely low values (%.3f) for %s, using boost=1\n",avg,title);
		return 1.0;
	}

	if (avg < minlimit) boost=minlimit/avg;
	else if (avg > maxlimit) boost=maxlimit/avg;
	else boost=1;
	if (boost > 1+tresh) boost=1+tresh;
	if (boost < 1-tresh) boost=1-tresh;	
	printf("  %s boost : %.2f, change: %.2f->%.2f (target: %.2f-%.2f)\n", 	
		title,boost,oldvalue, avg*boost,minlimit,maxlimit);
	*newvalue=avg*boost;
	return boost;
	}
	

void get_brightness(const float value,const float br_gamma,const float contr_gamma, float *new_br, float *new_contr,const float overallbr){
	float tmp;  // temporary variable to store brightness
	float old_contrast=0;
	float new_contrast;
	const bool debug=FALSE;
	//if (br_gamma<1) tmp=value*br_gamma;
	//else if (br_gamma>1) tmp=(br_gamma-1) + value*(1-br_gamma+1);
	//else tmp=value;
	tmp=pow(value,br_gamma);
	if (tmp>overallbr) old_contrast=pow((tmp-overallbr)/(1-overallbr),contr_gamma);
	else if (tmp<overallbr) old_contrast=pow((overallbr-tmp)/overallbr,contr_gamma);
	else old_contrast=0;
	
	//applying contrast gamma
	new_contrast=pow(old_contrast,contr_gamma);
	
	//calculating new brightness
	if (tmp>overallbr) *new_br=overallbr + new_contrast*(1.0-overallbr);
	else if (tmp<overallbr) *new_br=overallbr - new_contrast*overallbr;	
	else *new_br=overallbr;
	
	if (debug) {
		printf ("  old brightness: %.3f , old contrast: %.3f\n",value,old_contrast);
		printf ("   new brightness: %.3f, new contrast: %.3f\n",*new_br,new_contrast);
		printf ("   applied gammas: brightness: %.3f contrast %.3f\n",br_gamma,contr_gamma);}

	*new_contr=new_contrast;
	}


	
//void get_brightness(const float value,const float br_gamma,const float contr_gamma, float *new_br, float *new_contr,const float overallbr){
	//float tmp;  // temporary variable to store brightness
	//float old_contrast=0;
	//float new_contrast;
	//const bool debug=FALSE;
	//tmp=pow(value,br_gamma);
	//if (tmp>overallbr) old_contrast=pow((tmp-overallbr)/(1-overallbr),contr_gamma);
	//else if (tmp<overallbr) old_contrast=pow((overallbr-tmp)/overallbr,contr_gamma);
	//else old_contrast=0;
	
	////applying contrast gamma
	//new_contrast=pow(old_contrast,contr_gamma);
	
	////calculating new brightness
	//if (tmp>overallbr) *new_br=overallbr + new_contrast*(1.0-overallbr);
	//else if (tmp<overallbr) *new_br=overallbr - new_contrast*overallbr;	
	//else *new_br=overallbr;
	
	//if (debug) {
		//printf ("  old brightness: %.3f , old contrast: %.3f\n",value,old_contrast);
		//printf ("   new brightness: %.3f, new contrast: %.3f\n",*new_br,new_contrast);
		//printf ("   applied gammas: brightness: %.3f contrast %.3f\n",br_gamma,contr_gamma);}

	//*new_contr=new_contrast;
	//}



void brightness_calibrate(const float *array, float *br_gamma, float *contr_gamma){  //,float br_min,float br_max, float contr_min, float contr_max){
	
	*br_gamma=1.0;
	*contr_gamma=1.0;
	float l_br_gamma=1.0;
	float l_contr_gamma=1.0;
	float new_br=1.0;
	float new_contr=1.0;
	int i,j;
	float sum;
	float tmpbr=0,tmpcontr=0;
	float br_avg,contr_avg;
	float oldbr, oldcontr;
	const bool debug=FALSE;
	float br_min,contr_min,br_max,contr_max;
	float halfoffset=0.02;
	//br_min=maindata.minbr;contr_min=maindata.mincontr;br_max=maindata.maxbr;contr_max=maindata.maxcontr;
	
	oldcontr=-1; //just to initialize it
	//calculating overall brightness
	for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1){
		get_brightness(array[i],1,1,&new_br,&new_contr);
			tmpbr+=new_br;
			}
	oldbr=tmpbr/POINTSPERLINE/POINTSPERLINE;
	//calculating overall contrast
	for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1){
		get_brightness(array[i],1,1,&new_br,&new_contr,oldbr);
			tmpcontr+=new_contr;}	
	oldcontr=tmpcontr/POINTSPERLINE/POINTSPERLINE;	
	
	
	if (oldbr-halfoffset<maindata.minbr) {br_min=maindata.minbr;br_max=maindata.minbr+2*halfoffset;}
	else if (oldbr+halfoffset>maindata.maxbr) {br_max=maindata.maxbr;br_min=maindata.maxbr-2*halfoffset;}
	else {br_max=oldbr+halfoffset;br_min=oldbr-halfoffset;}
	if (oldcontr-halfoffset<maindata.mincontr) {contr_min=maindata.mincontr;contr_max=maindata.mincontr+2*halfoffset;}
	else if (oldcontr+halfoffset>maindata.maxcontr) {contr_max=maindata.maxcontr;contr_min=maindata.maxcontr-2*halfoffset;}
	else {contr_max=oldcontr+halfoffset;contr_min=oldcontr-halfoffset;}	
	
	
	//in every step we test brightness and then contrast
	for (j=0;j<100;j+=1){
		if (debug) printf ("  brightness_calibrate - iteration: %1d\n",j);
		//testing brightness
		sum=0;
		for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1){
			get_brightness(array[i],l_br_gamma,l_contr_gamma,&new_br,&new_contr);
			sum+=new_br;}
		br_avg=sum/POINTSPERLINE/POINTSPERLINE;
		//if (j==0) oldbr=br_avg;
		if (br_avg> br_min && br_avg<br_max) ;
		else if (br_avg<br_min)l_br_gamma*=0.985;
		else l_br_gamma*=1.015;
		
		//testing contrast
		sum=0;
		for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1){
			get_brightness(array[i],l_br_gamma,l_contr_gamma,&new_br,&new_contr,br_avg);
			sum+=new_contr;
			if (debug && isnan(new_contr)){
				printf ("  Received nan contrast: value: %.2f, br_gamma: %.2f, contrast gamma: %.2f\n",
				array[i],l_br_gamma,l_contr_gamma);
				exit(10);}
			}
		contr_avg=sum/POINTSPERLINE/POINTSPERLINE;
		//if (j==0) oldcontr=contr_avg;
		if (contr_avg> contr_min && contr_avg<contr_max) ;
		else if (contr_avg>contr_max)l_contr_gamma*=1.015;
		else l_contr_gamma*=0.985;	
		
		if (debug) printf(" Iter. results: br gamma: %.2f / value: %.3f; contrast gamma: %.2f / value: %.3f(targ:%.3f - %.3f)\n",
					l_br_gamma,br_avg,l_contr_gamma,contr_avg,contr_min,contr_max);
		}
	

	*br_gamma= l_br_gamma  ;
	*contr_gamma=l_contr_gamma;

	printf("  Brightness gamma : %.2f, change: %.2f->%.2f (target: %.2f-%.2f / %.2f-%.2f)\n", 
		*br_gamma,oldbr,br_avg,br_min,br_max,maindata.minbr,maindata.maxbr);
	printf("  Contrast gamma   : %.2f, change: %.2f->%.2f (target: %.2f-%.2f / %.2f-%.2f)\n", 
		*contr_gamma,oldcontr,contr_avg,contr_min,contr_max,maindata.mincontr,maindata.maxcontr);
	
}


void get_ordered(float *array,int length){
	float tmp;
	int i,j;
	const bool verbose=FALSE;
	
	//ordering
	for (j=0;j<length-1;j+=1){
		for (i=0;i<length-1-j;i+=1){
			if (array[i]>array[i+1]){tmp=array[i+1];array[i+1]=array[i];array[i]=tmp;}
		}
	}
	
	//printing values 
	//for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=20){
		//printf ("  position: %3d: %.3f\n",i,array[i]);}
	
	if (verbose==TRUE) {
		//normal average
		float sum1=0;
		int wcount=0;
		for (j=0;j<length;j+=1){
			sum1	+=array[j];}
		printf ( "   Normal average: %.3f\n", sum1/length);
		//weightordered average
		int sum2=0;
		for (j=0;j<length;j+=1){
			sum2	+=array[j]*j;wcount+=j;}
		printf ( "   Weightordered average: %.3f ( + %.2f%%)\n", (float)sum2/wcount , (float)(sum2/(float)wcount) / (sum1/length * 100.0 - 100.0));}	
	}

float get_waverage(float *array,float gamma, float croptresh){
	//values under croptresh will be ommited from calculation
	int i,wcount;
	int effpos;
	
	float sum;
	sum=0;wcount=0;effpos=0;

	for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1){
		if (pow(array[i],gamma)<croptresh) continue;
		effpos+=1;
		sum+= pow(array[i],gamma) * effpos;
		wcount+=effpos;
		}
	if (wcount==0) {
		printf ("   (!! Not enough values above the treshold...)\n");
		return 0;}
	return (float)sum/wcount;
	}

float get_mp_waverage(float *array,float gamma, float croptresh){
	//values under croptresh will be ommited from calculation
	int i,wcount;
	int effpos;
	
	float sum;
	sum=0;wcount=0;effpos=0;

	for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1){
		if (my_pow(array[i],gamma)<croptresh) continue;
		effpos+=1;
		sum+= my_pow(array[i],gamma) * effpos;
		wcount+=effpos;
		}
	if (wcount==0) {
		printf ("   (!! Not enough values above the treshold...)\n");
		return 0;}
	return (float)sum/wcount;
	}

float get_average(float *array,int count){
	int i;
	float sum=0;
	for (i=0;i<count;i+=1){
		sum+=array[i];}
	return (float)sum/count;

}
		

float my_pow(const float base,const float exp){
	float weight;
	weight=pow(base,0.15);
	return pow(base,exp)*weight + base*(1-weight);
}

float saturation_calibrate(float *array,float minlimit,float maxlimit,const char *title, float *oldvalue,float *newvalue,float valuetresh){
	float avg,gamma,newr,newg,newb,new_sat;
	int j,basepos,basepos_sample,x,y,xpoint,ypoint,halfstepx,halfstepy;
	const bool debug=FALSE;
	//testing if there are enough samples for calibration - rework!
	
	get_ordered(&array[0],POINTSPERLINE*POINTSPERLINE);
	avg=get_mp_waverage(&array[0],1,valuetresh);
	*oldvalue=avg;
	if (avg<0.01) {
		printf ("  Image with extremely low saturation (%.3f), not saturating...\n",avg);
		return 1.0;
	}

	
	gamma=1.0;
	for (j=0;j<60;j+=1){
		
		
		//calculating actual saturation from samples
		halfstepx=maindata.source_x_size/POINTSPERLINE/2.0;
		halfstepy=maindata.source_y_size/POINTSPERLINE/2.0;
		for (xpoint=0;xpoint<POINTSPERLINE;xpoint+=1) {for (ypoint=0;ypoint<POINTSPERLINE;ypoint+=1) {	
			x=halfstepx+xpoint*maindata.source_x_size/POINTSPERLINE;
			y=halfstepy+ypoint*maindata.source_y_size/POINTSPERLINE;
			basepos=get_basepos(x,y,maindata.source_x_size);
			basepos_sample=get_basepos(xpoint,ypoint,POINTSPERLINE);
			resaturate(basepos,gamma,&newr,&newg,&newb,&new_sat);
			array[basepos_sample]=new_sat;}}
	
		get_ordered(&array[0],POINTSPERLINE*POINTSPERLINE);
		avg=get_mp_waverage(&array[0],gamma,valuetresh);
		//if (j==0) *oldvalue=avg;
		if (avg>= minlimit && avg<=maxlimit) break;
		
		if (avg<minlimit) {
			gamma=gamma-0.01;
			//avg=get_waverage(&array[0],newgamma1);
			}
			
		if (avg>minlimit) {
			gamma=gamma+0.01;
			//avg=get_waverage(&array[0],newgamma1);
			}	
		if (debug) printf ("Changing gamma to: %.2f, old avg: %.3f\n", gamma,avg);
		}		
			
		
	printf("  %s gamma : %.2f, change: %.2f->%.2f (target: %.2f-%.2f)\n", 
		title,gamma,*oldvalue,avg,minlimit,maxlimit);
	*newvalue=avg;
	return gamma;
}

void resaturate(int basepos, float saturation_gamma, float *newr,float *newg,float *newb,float *applied_sat){
	//this will calculate needed saturation (from current one and gamma)
	//calculate max possible saturation
	//applies needed/possible saturation
	//shifts rgb (to preserve brightness)
	//publish achieved saturation and new rgb values as well
	float due_sat,maxrgb,minrgb,possible_boost,boost;
	const bool debug=FALSE;
	const bool debug2=FALSE;
		
	if(sat[basepos]==0 || saturation_gamma==1){
		boost=1;
		*applied_sat=sat[basepos];}
	else {
		due_sat=my_pow(sat[basepos],saturation_gamma);
		maxrgb=max(r[basepos] , max (g[basepos] , b[basepos]));
		minrgb=min(r[basepos] , min (g[basepos] , b[basepos]));
	
		possible_boost=min(   (1-br[basepos])/maxrgb  ,  (br[basepos])/(-minrgb)  )    ;
		
		boost=min(due_sat/sat[basepos],	possible_boost);
		*applied_sat=boost*sat[basepos];
		if (debug && possible_boost==boost){
			printf (" minrgb %.3f, maxrgb: %.3f, br: %.3f \n",minrgb,maxrgb,br[basepos]);
			printf (" starting sat: %.3f, Due sat: %.3f, required boost %.3f, possible boost: %.3f, final boost: %.3f\n",
			sat[basepos],due_sat,due_sat/sat[basepos],possible_boost,boost);
		}
		}
		
	//new rgb
	*newr=r[basepos]* boost;
	*newg=g[basepos]* boost;
	*newb=b[basepos]* boost;
	if (debug2 && (isnan(*newr) || isnan(*newg) || isnan(*newb) )){
		printf (" minrgb %.3f, maxrgb: %.3f, br: %.3f \n",minrgb,maxrgb,br[basepos]);
		printf (" starting sat: %.3f, Due sat: %.3f, required boost %.3f, possible boost: %.3f, final boost: %.3f\n",
			sat[basepos],due_sat,due_sat/sat[basepos],possible_boost,boost);}
		
}


void add_offset(float *bg_weight,const unsigned int x_size,const unsigned int y_size,const unsigned int offset){
	float *tmp1;
	float *tmp2;
	int basepos,basepos_c;
	float maxvalue;
	unsigned int i,j,k,x,y;
	const bool debug=FALSE;
	int x_near[8]={0,1,1,1,0,-1,-1,-1};	
	int y_near[8]={1,1,0,-1,-1,-1,0,1};		
	
	tmp1 = new float[x_size*y_size];
	tmp2 = new float[x_size*y_size];
	
	if (debug) printf("   adding offset: %2d\n",offset);
	
	for (i=0;i<y_size*x_size;i+=1){
		tmp1[i]=bg_weight[i];}
	
	for (k=0;k<offset;k+=1) {
		for (x=0;x<x_size;x+=1) { for  (y=0;y<y_size;y+=1) {
			basepos_c=get_basepos(x,y,x_size);
			//if (debug and basepos_c%2000==0) printf("    Iter: %2d: value: %.3f\n",k,tmp1[basepos_c]);
			maxvalue=tmp1[basepos_c];		
			for (j=0;j<8;j+=1) {
				if (x+x_near[j]<0 || x+x_near[j]>=x_size  || y+y_near[j]<0 || y+y_near[j]>=y_size) continue;
				basepos=get_basepos(x+x_near[j],y+y_near[j],x_size);
				if (tmp1[basepos]>maxvalue) maxvalue=tmp1[basepos];
				if (maxvalue==1) break;}
			tmp2[basepos_c]=maxvalue;
			//if (debug and basepos_c%2000==0) printf("    Iter: %2d: value: %.3f\n",k,tmp2[basepos_c]);
			}}
		for (i=0;i<y_size*x_size;i+=1){	tmp1[i]=tmp2[i];}	}	

	for (i=0;i<y_size*x_size;i+=1){bg_weight[i]=tmp1[i];}
	
	delete[] tmp1;
	delete[] tmp2;
	
}



	
void apply_sharp_boost(const float sharp_boost){
	int x,y,basepos;
	float weight, local_boost,diff,br_old;
	const bool debug=FALSE;
	const float bottomtresh=0.08;
	const float uppertresh=0.3;
	
	for (x=0;x<maindata.source_x_size;x+=1) {
		for (y=0;y<maindata.source_y_size;y+=1) {
			basepos=get_basepos(x,y,maindata.source_x_size);
			
			//sharpening
			if (debug) br_old=br[basepos];
			diff=(br[basepos]-mask1[basepos]);
			if      (br[basepos]<=bottomtresh     && diff<0 )weight=0;
			else if (br[basepos]>=(1-bottomtresh) && diff>0) weight=0;
			else if (br[basepos]< uppertresh      && diff<0) weight=(  br[basepos]-bottomtresh)/(uppertresh-bottomtresh);
			else if (br[basepos]> (1-uppertresh)  && diff>0) weight=(1-br[basepos]-bottomtresh)/(uppertresh-bottomtresh);
			else                                             weight=1;
			local_boost=(sharp_boost-1.0)*weight + 1.0;
			br[basepos]=(br[basepos]-mask1[basepos])*local_boost+mask1[basepos];
			if (debug && br[basepos]>0.90 && local_boost >1) printf("  old br: %.3f , new: %.3f,mask: %.3f, initial boost: %.3f, applied boost: %.3f\n",
				br_old,br[basepos],mask1[basepos],sharp_boost,local_boost);
			if (br[basepos]<0) br[basepos]=0;
			if (br[basepos]>1) br[basepos]=1;
			}}
				
	}

bool search_for_nan(){
	int x,y,basepos;
	bool nanfound=FALSE;
	for (x=0;x<maindata.source_x_size;x+=1) {
		for (y=0;y<maindata.source_y_size;y+=1) {
			basepos=get_basepos(x,y,maindata.source_x_size);
			if ( isnan(br[basepos]) || isnan(r[basepos]) || isnan(g[basepos]) || isnan(b[basepos]) ){
				printf ("nan found: br: %.3f, rgb: %.3f %.3f %.3f\n",
					br[basepos],r[basepos],g[basepos],b[basepos]);
				nanfound=TRUE;
				}
			if ( isnan(sat[basepos])) {
				printf(" nan found sat:%.3f\n",sat[basepos]);
				nanfound=TRUE;
				}
			}}
	return nanfound;
	}
			
	

void process(float saturation_gamma){
	int x,y,basepos;
	float applied_sat;
	
	if (maindata.bw==TRUE){
		for (x=0;x<maindata.source_x_size;x+=1) {
			if (x<maindata.source_x_size/2 && maindata.halfmode==TRUE) {
				for (y=0;y<maindata.source_y_size;y+=1) {
					basepos=get_basepos(x,y,maindata.source_x_size);
					r[basepos]=r_orig[basepos];				
					g[basepos]=g_orig[basepos];				
					b[basepos]=b_orig[basepos];				
					br[basepos]=br_orig[basepos];}				
				 continue;}
			for (y=0;y<maindata.source_y_size;y+=1) {
				basepos=get_basepos(x,y,maindata.source_x_size);		
				r[basepos]=0;
				g[basepos]=0;
				b[basepos]=0;	}}}
	else{
		for (x=0;x<maindata.source_x_size;x+=1) {
			if (x<maindata.source_x_size/2 && maindata.halfmode==TRUE){
				for (y=0;y<maindata.source_y_size;y+=1) {
					// referting rgb alignment changes
					basepos=get_basepos(x,y,maindata.source_x_size);
					r[basepos]=r_orig[basepos];				
					g[basepos]=g_orig[basepos];				
					b[basepos]=b_orig[basepos];				
					br[basepos]=br_orig[basepos];}	
				continue;}
			for (y=0;y<maindata.source_y_size;y+=1) {
				basepos=get_basepos(x,y,maindata.source_x_size);
				resaturate(basepos, saturation_gamma, &r[basepos],&g[basepos],&b[basepos],&applied_sat);
				}}}
	}


void put_in_stat(float value,int *stat){
	if (value>1) stat[0]+= 1;
	else if (value<1) stat[1]+= 1;	
	else stat[2]+= 1;
	}


void insert_text(CImg<unsigned char>* img,float outer_x, float outer_y, float sizeratio){
	int img_x_pos,img_y_pos,i;
	int label_x_pos,label_y_pos;
	int size,neededx,neededy,label_x_size,label_y_size,blur_radius,blur_radius_fg;
	int label_basepos;
	//int oldr,oldg,oldb,tmpr,tmpg,tmpb;
	int foregroundcol[3]={255,255,40};
	int backgroundcol[3]={0,0,70};
	int newr,newg,newb;
	int full_offset;
	unsigned char w1[] = { 255 };
	unsigned char w2[] = { 0 };
	float px_fg_weight,px_bg_weight,px_orig_weight;
		
	const bool debug=FALSE;
	
	size=(maindata.source_x_size+maindata.source_y_size)/80;
	blur_radius=size*sizeratio/7;
	blur_radius_fg=size*sizeratio/50;
	
	CImg<unsigned char> empty;
	empty.draw_text(0,0,maindata.title,w1,w2,1,size*sizeratio);
	neededx=empty.width();
	neededy=empty.height();
	full_offset=blur_radius+int(maindata.toffset*(min(neededx,neededy)))+1;
	label_x_size=neededx+2*full_offset;
	label_y_size=neededy+2*full_offset;

	if (debug) printf ("   Core values of label: %2dx%2d, adding offset: %.2d (%2d + %2d + 1)\n",
		neededx,neededy,full_offset,blur_radius,int(maindata.toffset*(min(neededx,neededy))));
	
	//printf ("    Label size: %2d %2d, text size: %2d x %2d, blur radius: %2d\n",label_x_size,label_y_size,neededx,neededy,blur_radius);
	if (maindata.savelabel) empty.save_png("label.png");
	
	float *bg_weight= new float[label_x_size*label_y_size];
	float *bg_weight_blurred= new float[label_x_size*label_y_size];
	float *fg_weight= new float[label_x_size*label_y_size];	
	float *fg_weight_blurred= new float[label_x_size*label_y_size];	
	
	
	//putting in weights for foreground&background and blurring what is to be blurred
	for (label_x_pos=0;label_x_pos<label_x_size;label_x_pos+=1) { for (label_y_pos=0;label_y_pos<label_y_size;label_y_pos+=1) {
		label_basepos=get_basepos(label_x_pos,label_y_pos,label_x_size);
		if (label_x_pos<full_offset || label_y_pos<full_offset 
			|| label_x_pos >=full_offset+neededx || label_y_pos >=full_offset+neededy) {
			bg_weight[label_basepos]=0;
			fg_weight[label_basepos]=0;}
		else {
			//empty_basepos=get_basepos(label_x-blur_radius-1,label_y-blur_radius-1,neededx);
			bg_weight[label_basepos]=pow(empty(label_x_pos-full_offset,label_y_pos-full_offset,0)/255.0,1.0/2.2);
			fg_weight[label_basepos]=pow(empty(label_x_pos-full_offset,label_y_pos-full_offset,0)/255.0,1.0/2.2);
			if (debug && label_x_pos%10==1 && label_y_pos%10==1)
				printf ("  For label position %2d %2d entering value: %.3f\n",label_x_pos,label_y_pos,fg_weight[label_basepos]);
			}
		}}	
	//blurring
	add_offset(&bg_weight[0],(unsigned int) label_x_size,(unsigned int) label_y_size,(unsigned int) int(maindata.toffset*(min(neededx,neededy))));
	blur(&bg_weight[0],&bg_weight_blurred[0],(float)blur_radius,label_x_size,label_y_size,__LINE__);
	blur(&fg_weight[0],&fg_weight_blurred[0],(float)blur_radius_fg,label_x_size,label_y_size,__LINE__);
	for (i=0;i<	label_x_size*label_y_size;i+=1){
		fg_weight_blurred[i]=3*fg_weight_blurred[i];
		if(fg_weight_blurred[i]>1.0) fg_weight_blurred[i]=1.0;
		bg_weight_blurred[i]=1*bg_weight_blurred[i];
		if(bg_weight_blurred[i]>1.0) bg_weight_blurred[i]=1.0;		
		
		}
			
	////putting background&foreground into img
	for (label_x_pos=0;label_x_pos<label_x_size;label_x_pos+=1) { for (label_y_pos=0;label_y_pos<label_y_size;label_y_pos+=1) {
		label_basepos=get_basepos(label_x_pos,label_y_pos,label_x_size);
		img_x_pos=maindata.source_x_size*outer_x-label_x_size+label_x_pos;
		if (img_x_pos<0 ||img_x_pos>=maindata.source_x_size) continue; 
		img_y_pos=maindata.source_y_size*outer_y-label_y_size+label_y_pos;
		if (img_y_pos<0 ||img_y_pos>=maindata.source_y_size) continue; 
		
		//here we calculate weights of foreground, background and original pixel
		px_fg_weight=fg_weight_blurred[label_basepos];
		if (bg_weight_blurred[label_basepos]>fg_weight_blurred[label_basepos])
			px_bg_weight=bg_weight_blurred[label_basepos]-fg_weight_blurred[label_basepos];
		else px_bg_weight=0;
		px_fg_weight=px_fg_weight*maindata.topacity;
		px_bg_weight=px_bg_weight*maindata.topacity;
		px_orig_weight=1-px_fg_weight-px_bg_weight;
		if( debug && label_x_pos%10==0 && label_y_pos%10==0) 
			printf ("  weights: %.3f - %.3f - %.3f\n",px_fg_weight,px_bg_weight,px_orig_weight);
		
		newr=(*img)(img_x_pos,img_y_pos,0)*px_orig_weight + backgroundcol[0]*px_bg_weight + foregroundcol[0]*px_fg_weight;
		newg=(*img)(img_x_pos,img_y_pos,1)*px_orig_weight + backgroundcol[1]*px_bg_weight + foregroundcol[1]*px_fg_weight;
		newb=(*img)(img_x_pos,img_y_pos,2)*px_orig_weight + backgroundcol[2]*px_bg_weight + foregroundcol[2]*px_fg_weight;

		(*img)(img_x_pos,img_y_pos,0)=newr;
		(*img)(img_x_pos,img_y_pos,1)=newg;
		(*img)(img_x_pos,img_y_pos,2)=newb;	
	
	}}
	
	delete[] bg_weight;
	delete[] bg_weight_blurred;
	delete[] fg_weight;
	delete[] fg_weight_blurred;
}

	
void insert_final(CImg<unsigned char>* final_img, const float *source,const int output) {
	int x,y,basepos;
	float Rnew,Bnew,Gnew;
	unsigned char Rfinal,Gfinal,Bfinal;
	const bool debug=FALSE;
	
	for (x=0;x<maindata.source_x_size;x+=1) {
		//if (x<maindata.source_x_size/2 && maindata.halfmode==TRUE) continue;
		for (y=0;y<maindata.source_y_size;y+=1) {
			basepos=get_basepos(x,y,maindata.source_x_size);
			if (output==IMAGE){
				Rnew=r[basepos]+source[basepos];
				Gnew=g[basepos]+source[basepos];
				Bnew=b[basepos]+source[basepos];}
			else {
				Rnew=source[basepos];
				Gnew=source[basepos];
				Bnew=source[basepos];}
						
			if (Rnew<0) {Rnew=0;}
			if (Gnew<0) {Gnew=0;}
			if (Bnew<0) {Bnew=0;}
			if (Rnew>1) {Rnew=1;}
			if (Gnew>1) {Gnew=1;}
			if (Bnew>1) {Bnew=1;}
			Rfinal=round(pow(Rnew,2.2)*255.0);
			Gfinal=round(pow(Gnew,2.2)*255.0);
			Bfinal=round(pow(Bnew,2.2)*255.0);
			//inserting into image
			(*final_img)(x,y,0,0)=Rfinal;
			(*final_img)(x,y,0,1)=Gfinal;
			(*final_img)(x,y,0,2)=Bfinal;
			if(debug && basepos<10) printf (" final values: %3d: %.4f %.4f %.4f\n", basepos, Rnew,Gnew,Bnew);
			if(debug && basepos<10) printf (" final values: %3d: %u %u %u\n", basepos, Rfinal,Gfinal,Bfinal);
			}}
	}


float RGBtoHue(float R,float G,float B){
	float minvalue,maxvalue,hue,delta;
	
	if (R==G && G==B)	return -1;
	
	minvalue=min(R,min(G,B));
	maxvalue=max(R,max(G,B));
	delta=maxvalue-minvalue;
	
	if (R == maxvalue) 
		hue = ( G-B ) / delta;		// between yellow & magenta
	else if (G == maxvalue)
		hue = 2 + ( B-R ) / delta;	// between cyan & yellow
	else
		hue = 4 + ( R-G ) / delta;	//
	
	if (hue<0)
		hue=hue+6;
	
	return hue/6.0;
}
	
	
float calculate_saturation(const int basepos){
	const bool debug=FALSE;
	float brightness,chrome,saturation,hue,weight,partial;
	int i;
	
	chrome=max(max(r[basepos],g[basepos]),b[basepos]) - min(min(r[basepos],g[basepos]),b[basepos]);
	if (chrome==0) return 0;
	
	brightness=calculate_brightness(r[basepos]+br[basepos],g[basepos]+br[basepos],b[basepos]+br[basepos]);
	if (brightness<0.005) brightness=0.005;
	saturation=pow(brightness,0.1)*chrome;
	
	
	//weighting the saturation
	hue=RGBtoHue(r[basepos]+br[basepos],g[basepos]+br[basepos],b[basepos]+br[basepos]);
	
	partial=-1;
	for (i=0;i<6;i+=1){
		
		//print "Testing step: ",i,", hue: ",hue
		if (hue<i/6.0 || hue>=(i+1)/6.0) continue;
		partial=(hue-i/6.0)*6;
		weight=partial*remaptable[i+1] + (1-partial) * remaptable[i];
		if (debug) printf(" For hue: %.3f  got weight: %.3f\n",hue,weight);
		break;}
	if (partial==-1 ){
		if (debug) printf ("this should not happed - hue %.3f (rgb: %.3f %.3f %.3f),chrome: %.3f\n ",
		hue,r[basepos]+br[basepos],g[basepos]+br[basepos],b[basepos]+br[basepos],chrome);
		weight=1;}
	
	saturation=saturation*weight;

	
	//printf (" %.3f\n",saturation);
	if (debug &&  isnan(saturation)) printf(" sat is nan for rgb: %.3f %.3f %.3f, br: %.3f / %.3f, chroma: %.3f\n",
		r[basepos],g[basepos],b[basepos],br[basepos],brightness,chrome);
	return saturation;
	
	}
	
void populate_sat(){
	int x,y,basepos;
	for (x=0;x<maindata.source_x_size;x+=1) {
		for (y=0;y<maindata.source_y_size;y+=1) {
			basepos=get_basepos(x,y,maindata.source_x_size);
			sat[basepos]=calculate_saturation(basepos);}}
	}	

void realign(float change, int rchange,int gchange, int bchange, float *rdiff,float *gdiff,float *bdiff){
	const bool debug=FALSE;
	float currentweight,reminder;
	
	*rdiff-=rchange*change;
	*gdiff-=gchange*change;
	*bdiff-=bchange*change;	
	if (debug) printf ("     realign subresults: %6.3f %6.3f %6.3f\n",
		*rdiff,*gdiff,*bdiff);

	currentweight=(RWEIGHT*rchange+GWEIGHT*gchange+BWEIGHT*bchange);
	reminder=-currentweight/(1-currentweight);

	//fullchange=change;// (rchange*RWEIGHT + gchange*GWEIGHT + bchange*BWEIGHT);

	if (debug) printf ("     realign reminder: %6.3f\n",reminder);
	*rdiff+= (rchange-1)*change*reminder;
	*gdiff+= (gchange-1)*change*reminder;
	*bdiff+= (bchange-1)*change*reminder;
	if (debug) printf ("    realign result: diffs: %6.4f %6.4f %6.4f, check average %6.4f\n",
		*rdiff,*gdiff,*bdiff,calculate_brightness(*rdiff,*gdiff,*bdiff));
}
	

void get_disalignment(){
	int halfstepx,halfstepy,x,y,basepos,count,xpoint,ypoint;
	float rarray[POINTSPERLINE*POINTSPERLINE];
	float garray[POINTSPERLINE*POINTSPERLINE];
	float barray[POINTSPERLINE*POINTSPERLINE];
	const bool debug=FALSE;	
	int bottom,top;
	float rbottom,rtop,gbottom,gtop,bbottom,btop;
	
	int i;
	float oldrbottom,oldrtop,oldgbottom,oldgtop,oldbbottom,oldbtop;

	maindata.rdiff=0;maindata.gdiff=0;maindata.bdiff=0;


	
	count=0;//sumr=0;sumg=0;sumb=0;
	halfstepx=maindata.source_x_size/POINTSPERLINE/2.0;
	halfstepy=maindata.source_y_size/POINTSPERLINE/2.0;
	for (xpoint=0;xpoint<POINTSPERLINE;xpoint+=1) {for (ypoint=0;ypoint<POINTSPERLINE;ypoint+=1) {	
		x=halfstepx+xpoint*maindata.source_x_size/POINTSPERLINE;
		y=halfstepy+ypoint*maindata.source_y_size/POINTSPERLINE;
		//printf (" Testing pixel %4d x %4d\n",x,y);
		basepos=get_basepos(x,y,maindata.source_x_size);
		if (br[basepos] + (max (r[basepos], max(g[basepos],b[basepos]))) > 0.98) continue;
		if (br[basepos] - (min (r[basepos], min(g[basepos],b[basepos]))) < 0.02) continue;
		rarray[count]=r[basepos];
		garray[count]=g[basepos];
		barray[count]=b[basepos];	
		count=count+1;	
		}}

	if (count <80) {
		printf ("   Not enough good pixels for rgb alignment check...\n");
		return;
	}

	if (debug) printf ("  Good pixels for alignment: %1d\n",count);
	//sorting
	get_ordered(&rarray[0],count);
	get_ordered(&garray[0],count);
	get_ordered(&barray[0],count);	
	
	bottom=0.2*count;
	top=0.8*count;	

	oldrbottom=get_average(&rarray[0],bottom);
	oldrtop   =get_average(&rarray[top],count-top);
	oldgbottom=get_average(&garray[0],bottom);
	oldgtop   =get_average(&garray[top],count-top);
	oldbbottom=get_average(&barray[0],bottom);
	oldbtop   =get_average(&barray[top],count-top);
	rmiddle+=(oldrbottom+oldrtop)/2.0;
	gmiddle+=(oldgbottom+oldgtop)/2.0;
	bmiddle+=(oldbbottom+oldbtop)/2.0;
	if (oldrbottom>maxofminr) maxofminr=oldrbottom;
	if (oldgbottom>maxofming) maxofming=oldgbottom;
	if (oldbbottom>maxofminb) maxofminb=oldbbottom;
	if (oldrtop<minofmaxr)   minofmaxr=oldrtop;
	if (oldgtop<minofmaxg)   minofmaxg=oldgtop;
	if (oldbtop<minofmaxb)   minofmaxb=oldbtop;
	
	for (i=0;i<5;i+=1){
	
		rbottom=oldrbottom+maindata.rdiff;
		rtop   =oldrtop+maindata.rdiff;
		gbottom=oldgbottom+maindata.gdiff;
		gtop   =oldgtop+maindata.gdiff;
		bbottom=oldbbottom+maindata.bdiff;
		btop   =oldbtop+maindata.bdiff;

		if (debug) printf ("   Alignment iteration: %1d, tresholds: %3d - %3d \n",i,bottom,top);
		if (debug) printf ("    r tresholds: %6.3f - %6.3f \n",rbottom,rtop);
		if (debug) printf ("    g tresholds: %6.3f - %6.3f \n",gbottom,gtop);
		if (debug) printf ("    b tresholds: %6.3f - %6.3f \n",bbottom,btop);
		if (rbottom>RTARGET){
			realign(rbottom,1,0,0,&maindata.rdiff,&maindata.gdiff,&maindata.bdiff);
			if (debug) printf ("      changing r by: %6.3f\n",maindata.rdiff);
			continue;}
		if (rtop<RTARGET){
			realign(rtop,1,0,0,&maindata.rdiff,&maindata.gdiff,&maindata.bdiff);			
			if (debug) printf ("      changing r by: %6.3f\n",maindata.rdiff);
			continue;}
		if (gbottom>GTARGET){
			realign(gbottom,0,1,0,&maindata.rdiff,&maindata.gdiff,&maindata.bdiff);	
			if (debug) printf ("      changing g by: %3f\n",maindata.gdiff);
			continue;}
		if (gtop<GTARGET){
			realign(gtop,0,1,0,&maindata.rdiff,&maindata.gdiff,&maindata.bdiff);
			if (debug) printf ("      changing g by: %3f\n",maindata.gdiff);
			continue;}
		if (bbottom>BTARGET){
			realign(bbottom,0,0,1,&maindata.rdiff,&maindata.gdiff,&maindata.bdiff);
			if (debug) printf ("      changing b by: %3f\n",maindata.bdiff);
			continue;}
		if (btop<BTARGET){
			realign(btop,0,0,1,&maindata.rdiff,&maindata.gdiff,&maindata.bdiff);
			if (debug) printf ("      changing b by: %3f\n",maindata.bdiff);
			continue;}			
		//no need for further iterations
		break;
	}
	
	if (maindata.rdiff==0 && maindata.gdiff==0 && maindata.bdiff==0) return;
	
	rgbalign_stat+=1;
	for (i=0;i<maindata.source_x_size*maindata.source_y_size;i+=1) {
		r[i]+=maindata.rdiff;
		g[i]+=maindata.gdiff;
		b[i]+=maindata.bdiff;}
		
	printf("  Fixing RGB disalignment: %.3f %.3f %.3f\n",maindata.rdiff,maindata.gdiff,maindata.bdiff);
	if (debug) {
		if (debug) printf ("    new r tresholds: %6.3f - %6.3f \n",rbottom,rtop);
		if (debug) printf ("    new g tresholds: %6.3f - %6.3f \n",gbottom,gtop);
		if (debug) printf ("    new b tresholds: %6.3f - %6.3f \n",bbottom,btop);}
}

void populate(const CImg<unsigned char>* srcimgL){
	int x,y,basepos,R,G,B;
	int x1,y1;
	//float sumbr,summask;
	int tmp;
	const bool debug=FALSE;

	//first allocating memory for arrays
	if (maindata.source_size>allocated){
		delete [] r;delete [] g;delete [] b;delete [] br;delete [] sat;
		float* r_tmp = new float[maindata.source_size];
		float* g_tmp = new float[maindata.source_size];
		float* b_tmp = new float[maindata.source_size];
		float* br_tmp = new float[maindata.source_size];
		float* r_orig_tmp = new float[maindata.source_size];
		float* g_orig_tmp = new float[maindata.source_size];
		float* b_orig_tmp = new float[maindata.source_size];
		float* br_orig_tmp = new float[maindata.source_size];
		float* sat_tmp = new float[maindata.source_size];
		float* mask1_tmp = new float[maindata.source_size];
		float* tmp1_tmp = new float[maindata.source_size];
		
		
						
		r=r_tmp;
		g=g_tmp;
		b=b_tmp;
		br=br_tmp;
		r_orig=r_orig_tmp;
		g_orig=g_orig_tmp;
		b_orig=b_orig_tmp;
		br_orig=br_orig_tmp;
		sat=sat_tmp;
		mask1=mask1_tmp;
		tmp1=tmp1_tmp;
		allocated=maindata.source_size;
		//delete r_tmp;
		}
	
	if (maindata.rotation==8 || maindata.rotation==6  || maindata.rotation==7  || maindata.rotation==5 ){
		tmp=maindata.source_x_size;
		maindata.source_x_size=maindata.source_y_size;
		maindata.source_y_size=tmp;}
		
	for (x=0;x<maindata.source_x_size;x+=1) {
		for (y=0;y<maindata.source_y_size;y+=1) {
			basepos=get_basepos(x,y,maindata.source_x_size);

			//2 = The 0th row is at the visual top of the image, and the 0th column is the visual right-hand side.
			if (maindata.rotation==2) {x1=maindata.source_x_size-x-1	;y1=y;}
			//3 = The 0th row is at the visual bottom of the image, and the 0th column is the visual right-hand side.
			else if (maindata.rotation==3) {x1=maindata.source_x_size-x-1;y1=maindata.source_y_size-y-1;}
			//4 = The 0th row is at the visual bottom of the image, and the 0th column is the visual left-hand side.
			else if (maindata.rotation==4) {x1=x						;y1=maindata.source_y_size-y-1;}
			//5 = The 0th row is the visual left-hand side of the image, and the 0th column is the visual top.
			else if (maindata.rotation==5) {x1=y						;y1=x;}
			//6 = The 0th row is the visual right-hand side of the image, and the 0th column is the visual top.
			else if (maindata.rotation==6) {x1=y						;y1=maindata.source_x_size-x-1;}
			//7 = The 0th row is the visual right-hand side of the image, and the 0th column is the visual bottom.
			else if (maindata.rotation==7) {x1=maindata.source_y_size-y-1;y1=maindata.source_x_size-x-1;}
			//8 = The 0th row is the visual left-hand side of the image, and the 0th column is the visual bottom.
			else if (maindata.rotation==8) {x1=maindata.source_y_size-y-1;y1=x;}	
			//1 = The 0th row is at the visual top of the image, and the 0th column is the visual left-hand side.
			else 							{x1=x						;y1=y;}
			R=(int)(*srcimgL)(x1,y1,0);
			G=(int)(*srcimgL)(x1,y1,1);
			B=(int)(*srcimgL)(x1,y1,2);
			
			if (debug && basepos<10) printf("  Source values for %2d: %3d %3d %3d\n",basepos, R,G,B);
			
			
			basepos=get_basepos(x,y,maindata.source_x_size);
			r[basepos]=pow((float)R/255,1/2.2);
			g[basepos]=pow((float)G/255,1/2.2);			
			b[basepos]=pow((float)B/255,1/2.2);
			if (debug && basepos<10) printf("  Source values for %2d: %.4f %.4f %.4f\n",basepos, r[basepos],g[basepos],b[basepos]);
			br[basepos]=calculate_brightness(r[basepos],g[basepos],b[basepos]);
			//(r[basepos]+g[basepos]+b[basepos])/3.0;
			r[basepos]=r[basepos]-br[basepos];
			g[basepos]=g[basepos]-br[basepos];
			b[basepos]=b[basepos]-br[basepos];
			//br[basepos]=0.15+br[basepos]*0.9; // delete this !
			
			}}
			
	//if half mode we must save some data into *_orig arrays
	if (maindata.halfmode){
		for (x=0;x<maindata.source_y_size*maindata.source_x_size;x+=1){
			r_orig[x]=r[x];
			g_orig[x]=g[x];
			b_orig[x]=b[x];
			br_orig[x]=br[x];}}
			
	}
float calculate_brightness(const float r,const float g, const float b){
	float brightness;
	//brightness=( 4.0 * (3.0*r+6.0*g+b)/10.0 + max(r,max(g,b))  ) / 5.0;
	brightness=r*RWEIGHT + g*GWEIGHT + b*BWEIGHT;
	//brightness=r*0.3 + g*0.5+ b*0.2;
	
	return brightness;
	}

float parse_exif(char *char_filename){
	const bool verbose=FALSE;
	// Read the JPEG file into a buffer
	FILE *fp = fopen(char_filename, "rb");
	if (!fp) { 
		if(verbose) printf("Can't open file.\n"); 
		return -1; 
		}
	fseek(fp, 0, SEEK_END);
	unsigned long fsize = ftell(fp);
	rewind(fp);
	unsigned char *buf = new unsigned char[fsize];
	if (fread(buf, 1, fsize, fp) != fsize) {
		if(verbose) printf("Can't read file.\n");
		delete[] buf;
		return -2;
		}
	fclose(fp);
	
	// Parse EXIF
	EXIFInfo result;
	int code = result.parseFrom(buf, fsize);
	delete[] buf;
	if (code) {
		if(verbose) printf("Error parsing EXIF: code %d\n", code);
		return -3;
		}
		
	return result.Orientation;
}

int test_file(const char* file){
	//0-file exists, 1-exit but cannot read,2 do not exist
	
	FILE *tmp;
	if ((tmp = fopen(file, "r")) == NULL) {
		//printf (" error: %1d\n", errno);
		return errno;
	}

	fclose(tmp);
	return 0;
}

void savedata(const float *samples,string newfilename,const char *prefix){
	string datafilename;
	int i;
	datafilename=prefix;
	datafilename.append("_");
	datafilename=datafilename+newfilename;
	datafilename.append(".dat");
	
	//+"_"+newfilename+".dat";
	
	FILE* outputfile=fopen(datafilename.c_str(), "w");;
	if (outputfile != NULL) {
		//myfile.open (datafilename.c_str());
		for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1) fprintf(outputfile,"%.3f\n", samples[i]);
		fclose(outputfile);
		printf ("   Samples data saved to %s\n",datafilename.c_str());}
	else printf (" Failed to write to a file\n");
	
	}

string get_new_name(string filename,const int number){
	//gets actual name, strips path and replaces * and # if present
	
	int filenamelen,dotpos,slashpos,patternlength,aspos,i,crosspos,newnamelen;
	string basename,newname;
	const bool debug=FALSE;
	
	filenamelen=filename.length();
	slashpos=filename.find_last_of("/\\", filenamelen);
	dotpos = filename.find_last_of(".", filenamelen);
	if (debug) printf ("%1d, slash: %1d, dot: %.1d\n",filenamelen,slashpos,dotpos);
	basename=filename.substr(slashpos+1,dotpos-slashpos-1);
	if (debug) printf ("basename: %s\n",basename.c_str());
	if (debug) printf ("pattern defined: %1d\n",maindata.patterndefined);

	
	//modifying savename
	if (maindata.patterndefined==FALSE) newname="final_"+basename;
	else {
		patternlength=strlen(maindata.pattern);
		if (debug) printf ("patternlength: %1d\n",patternlength);			
		newname=maindata.pattern;
		aspos=newname.find_first_of("%");
		if (debug) printf (" find result: %.1d\n",aspos); 
		if (aspos >-1) {
			if (debug) printf ("   replacing asterix on pos: %1d...\n",aspos);
			newname.insert(aspos, basename);
			}
		
		crosspos=newname.find_first_of("#");
		if (crosspos >-1) {
			if (debug) printf ("   inserting number...\n");
			char buffer[20]; sprintf(buffer, "%04d", number+1);
			string number_str(buffer);
			if (debug) printf (" inserting number: %s, on pos: %1d\n",number_str.c_str(),crosspos);
			newname.insert(crosspos, number_str);
			}
		}
		
	if (debug) printf (" newname before cleaning: %s\n",newname.c_str());	
	//cleaning and shrotening newname if needed, also appending suffix
	for (i=0;i<10000;i+=1){
		aspos=newname.find_first_of("%#");
		if (aspos<0) break;
		newname.erase(aspos,1);}
	
	newnamelen=newname.length();
	if (newnamelen>52) newname.erase(50,string::npos);
	
	if (maindata.output==JPG) newname.append(".jpg");
	else newname.append(".png");
	
	if (debug) printf (" newname: %s\n",newname.c_str());
	return newname ;
}
	
	

//  ##############################   M A I N   #######################################	


int main( int argc,  char** argv){
	//char filename[1000];
	//int n;
	float brightness_gamma,contrast_gamma,saturation_gamma,sharpness_boost,tmp;
	//int filestatus=-1;
	
	set_initial();
		
	cimg::exception_mode(0);  //0 quiet, 1- console only errors
	
	arg_processing(argc, argv);
	maindata.toffset=0.1*maindata.toffset;
	//printf (" New maindata.toffset=%.3f\n",maindata.toffset);

	//testing span of min/maxbr
	//printf ("%.3f %.3f \n",maindata.minbr,maindata.maxbr);
	if (maindata.minbr+0.05>maindata.maxbr) {
		printf (" ! The gap between minimal and maximal brightness is too low, changing max brightness!\n");
		maindata.maxbr=maindata.minbr+0.05;
		}
	if (maindata.mincontr+0.05>maindata.maxcontr) {
		printf (" ! The gap between minimal and maximal brightness is too low, changing max contrast!\n");
		maindata.maxcontr=maindata.mincontr+0.05;
		}

	
	// EXIT IF NO IMAGES
	if (img_count <1) {
		cout << " ! TERMINATING: No images to process... \n";
		exit(1);}

   	// LISTING IMAGES 
   	cout <<" * Image(s): ";
	for (int i=0;i<img_count;i=i+1) {
		cout <<images[i];
		if(i<img_count-1)	cout <<", ";}
		//else	cout <<", "; }
	cout <<"\n"; 
	cout << " * Images count: " << img_count << "\n";

	//ITERATING over images
	CImg<unsigned char> srcimg;
	for (int n=0;n<img_count;n++) {

		//opening image
		filename = images[n];
		char *char_filename=new char[filename.size()+1]  ;
		char_filename[filename.size()]=0; 				
		memcpy(char_filename,filename.c_str(),filename.size()); 
		
		
		//creating new name 
		string newfilename=get_new_name(images[n],n);
		const char *char_savename = newfilename.c_str();
		
		//testing if file exists
		if (maindata.skip==TRUE && ( test_file(char_savename)==0 || test_file(char_savename)==13) ){
	        printf ("==> File %s exitst, skipping...\n",char_savename);
	        continue;
	        }

		maindata.rotation=parse_exif(char_filename);
		//printf ("  Orientation: %1d\n",maindata.rotation);

		////opening (trying to open) the file
		//try {
			//CImg<unsigned char> srcimg(char_filename) ;
			////del srcimg;
		//} catch(CImgException &e) { 
			//printf ("\n==>  CImg library error for file %s :\n%s\nSkipping the file...\n",char_filename,e.what());
			//continue;
		//}
		//CImg<unsigned char> srcimg(char_filename) ;
		
		//opening (trying to open) the file
		//CImg<unsigned char> srcimg;
		try {
			srcimg.load(char_filename) ;
		} catch(CImgException &e) { 
			printf ("\n==>  CImg library error for file %s :\n%s\nSkipping the file...\n",char_filename,e.what());
			continue;}



		
		maindata.source_x_size = srcimg.width();maindata.source_y_size = srcimg.height();
		printf ("==> Image  %2d/%2d: %-15s ( %4d x %4d)\n",n+1,img_count,char_filename,maindata.source_x_size,maindata.source_y_size);

		maindata.source_size=maindata.source_y_size*maindata.source_x_size;
		
		//populating internal array, save for sat -it will be done later on
		if(maindata.rotation<=0 || maindata.rotation>8)maindata.rotation=1; 
		if (maindata.rotation>1) printf ("    (rotating, original orientation: %d)\n",maindata.rotation);
		populate(&srcimg);
		
		//processing color disalignment
		if (maindata.rgbalign) get_disalignment();

		//processing/changing brightness
		take_samples(BRIGHT);
		
		if (maindata.nobr==FALSE) brightness_calibrate(&sample_br[0] , &brightness_gamma, &contrast_gamma);
		//,
		//	maindata.minbr,maindata.maxbr,maindata.mincontr,maindata.maxcontr);
		else{
			brightness_gamma=1.0;
			contrast_gamma=1.0;}
		
		put_in_stat(brightness_gamma,&br_stat[0]);
		put_in_stat(contrast_gamma,&contr_stat[0]);
		
		
		//calculating & applying sharpening - relative radius 0.1
		change_brightness(&br[0],brightness_gamma,contrast_gamma);  //modifying br
		if (maindata.nosharp==FALSE) {
			blur(&br[0],&mask1[0],0.1,maindata.source_x_size,maindata.source_y_size,__LINE__);  // blurring br to mask1
			take_samples(SHARP);										//taking samples
			get_ordered(&sample_sharp1[0],POINTSPERLINE*POINTSPERLINE);								//ordering them
			sharpness_boost=linear_calibrate(&sample_sharp1[0],maindata.minsharp1,maindata.maxsharp1,1.0,"Sharpness1",2,&tmp,0.1);
			put_in_stat(1.0/sharpness_boost,&sharp_stat1[0]);		 	//calculating sharpness boost
			apply_sharp_boost(sharpness_boost);							//changing br
			if (maindata.savedata) savedata(&sample_sharp1[0],newfilename,"Sharp1");
			}
		else
			put_in_stat(1.0,&sharp_stat1[0]);	
		//saving mask if needed
		if (maindata.savemask==TRUE) {
			CImg<unsigned char> sharp_mask(maindata.source_x_size,maindata.source_y_size,1,3);
			insert_final(&sharp_mask,&mask1[0],MASK);
			string newmaskfilename=("mask1_"+newfilename);
			const char *char_newmaskfilename = newmaskfilename.c_str();
			sharp_mask.save_png(char_newmaskfilename);
			//sharp_mask.~CImg();
			}
		
		//calculating & applying sharpening - relative radius 0.01
		//change_brightness(&br[0],brightness_gamma,contrast_gamma);  //modifying br
		if (maindata.nosharp==FALSE) {
			blur(&br[0],&mask1[0],0.01,maindata.source_x_size,maindata.source_y_size,__LINE__);  // blurring br to mask1
			take_samples(SHARP);										//taking samples
			get_ordered(&sample_sharp1[0],POINTSPERLINE*POINTSPERLINE);								//ordering them
			sharpness_boost=linear_calibrate(&sample_sharp1[0],maindata.minsharp2,maindata.maxsharp2,1.0,"Sharpness2",2,&tmp,0.08);
			put_in_stat(1.0/sharpness_boost,&sharp_stat2[0]);		 	//calculating sharpness boost
			apply_sharp_boost(sharpness_boost);							//changing br
			if (maindata.savedata) savedata(&sample_sharp1[0],newfilename,"Sharp2");
			}
		else
			put_in_stat(1.0,&sharp_stat2[0]);
		//saving mask if needed
		if (maindata.savemask==TRUE) {
			CImg<unsigned char> sharp_mask(maindata.source_x_size,maindata.source_y_size,1,3);
			insert_final(&sharp_mask,&mask1[0],MASK);
			string newmaskfilename=("mask2_"+newfilename);
			const char *char_newmaskfilename = newmaskfilename.c_str();
			sharp_mask.save_png(char_newmaskfilename);
			//sharp_mask.~CImg();
			}


		
		//calculating sat changes
		if (maindata.bw==TRUE) saturation_gamma=1.0;
		else if (maindata.nosat==FALSE) {
			populate_sat();						//populating sat array
			take_samples(SATUR);
			float oldvalue=0;float newvalue=0;		
			saturation_gamma=saturation_calibrate(&sample_sat[0],maindata.minsat,maindata.maxsat,"Saturation",&oldvalue,&newvalue,0.025);
			//saturation_boost=1.0;
			//saturation_boost=linear_calibrate(&sample_sat[0],maindata.minsat,maindata.maxsat,saturation_gamma,"Saturation",0.5,&newvalue);
			put_in_stat(oldvalue/newvalue,&sat_stat[0]);
			if (maindata.savedata) savedata(&sample_sat[0],newfilename,"Satur");
		}
		else{
			saturation_gamma=1.0;
			}
		
		process(saturation_gamma);
		
		
		// creating NEW IMAGE
		CImg<unsigned char> final_img(maindata.source_x_size,maindata.source_y_size,1,3);
		insert_final(&final_img,&br[0],IMAGE);
		

			
		//inserting text
		if (maindata.insertingtext) insert_text(&final_img,maindata.xpos,maindata.ypos,maindata.textsize);
		

		
		//ulozenie
		if (maindata.mpx_resize>0){
			int new_x,new_y;
			new_x=maindata.source_x_size * pow(1000000*maindata.mpx_resize/maindata.source_x_size/maindata.source_y_size,0.5);
			new_y=maindata.source_y_size * pow(1000000*maindata.mpx_resize/maindata.source_x_size/maindata.source_y_size,0.5);
			printf ("  Rescaling to %2d x %2d (%.2f MPx)\n",new_x,new_y,new_x*new_y/1000000.0);
			final_img.resize(new_x,new_y,-100,-100,5);
			}
		try{
			if 			(maindata.output==PNG) final_img.save_png(char_savename);	
			else		final_img.save_jpeg(char_savename,89);
			printf("  Saved as: %s\n",newfilename.c_str());}
		catch(CImgException &e) { 
			printf ("\n ! Failed to save the file (%s) with error:\n%s\nSkipping the file...\n",newfilename.c_str(),e.what());
			
		}
		
		
		srcimg.assign();
		final_img.assign();
		}
			
	printf( "==SUMMARY (per change - count of images):\n");
	printf ("   %-21s |%10s |%10s |%11s|\n","Type of modif.","Decreased","Increased", "Not changed");
	printf ("   %-21s |%10d |%10d |%11d|\n","Brightness",br_stat[0],br_stat[1],br_stat[2]);
	printf ("   %-21s |%10d |%10d |%11d|\n","Contrast",contr_stat[0],contr_stat[1],contr_stat[2]);
	printf ("   %-21s |%10d |%10d |%11d|\n","Sharpness(0.1)",sharp_stat1[0],sharp_stat1[1],sharp_stat1[2]);	
	printf ("   %-21s |%10d |%10d |%11d|\n","Sharpness(0.01)",sharp_stat2[0],sharp_stat2[1],sharp_stat2[2]);	
	printf ("   %-21s |%10d |%10d |%11d|\n","Saturation",sat_stat[0],sat_stat[1],sat_stat[2]);		

	//printf ("  innter limits of r: %6.3f - %6.3f, middle value: %6.3f\n",maxofminr,minofmaxr,rmiddle/img_count);
	//printf ("  innter limits of g: %6.3f - %6.3f, middle value: %6.3f\n",maxofming,minofmaxg,gmiddle/img_count);
	//printf ("  innter limits of b: %6.3f - %6.3f, middle value: %6.3f\n",maxofminb,minofmaxb,bmiddle/img_count);
	printf("   RGB aligned images: %3d\n",rgbalign_stat);
	}
