// Cphix2 - tool to normalize images - modifies saturation, brightness, sharpness 
// and distribution of brightness
// look for 'const char *version=' to find out version :)
// 
// It is command line tool working in batch mode.
// Intended platform: Linux (though it should work elsewhere)
// To compile run 'make' in unpacked directory (Makefile included)
// No configure skript available.
// You have to take care of compiled binary by yourselves, and copy it wherever suitable.
// Dependencies: gcc (obviously) and Cimg (C Image Library)
//
// for Valgrind debugging, compile binary with following
// g++ -g cphix2.cpp -fno-inline  -L/usr/X11R6/lib -lm -lpthread -lX11 -Wall -Iinclude
//
// Contact: tiborb95 at gmail dot com
// 

#include <iostream>
using namespace std;
#include <vector>

#include <CImg.h>
using namespace cimg_library;
#include<stdio.h> 
#include<stdlib.h>  
#include <fstream>
#include <sstream>
#include <string>
#include <math.h>
#include "easyexif/exif.h"
#include <errno.h>

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


static 	vector<string> images; 
static int img_count=0;
static float *r,*g,*b, *br, *sat, *mask1, *tmp1;   //,*r_blur,*g_blur,*b_blur; sat taking from raw values now
static float sample_br[POINTSPERLINE*POINTSPERLINE];
static float sample_sat[POINTSPERLINE*POINTSPERLINE];
static float sample_sharp1[POINTSPERLINE*POINTSPERLINE];
static int allocated=0;
static int br_stat[3]={0,0,0};
static int contr_stat[3]={0,0,0};
static int sharp_stat[3]={0,0,0};
static int sat_stat[3]={0,0,0};
void help ();
static string filename; 
const char *version="1.6";
//const float remaptable[7]={1.3,0.7,1.1,0.8,1.4,0.9,1.3};
const float remaptable[7]={1.3,0.7,1.4,0.8,1.4,0.9,1.3};

void get_brightness(float value, float br_gamma, float contr_gamma, float *new_br, float *new_contr);
void get_ordered(float *array);
float get_waverage(float *array,float gamma);
float calculate_brightness(float r,float g, float b);
void resaturate(int basepos, float saturation_gamma, float *newr,float *newg,float *newb,float *applied_sat);
int test_file(const char* file);
float my_pow(float base,float exp);

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
	bool nobr;
	bool nosat;
	bool savemask;
	bool nosharp;
	float mpx_resize;
	int output;
	float textsize ;
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
} MyMainVals;
static MyMainVals  maindata={0.45,0.68,0.30,0.45,FALSE,0.23,0.43,0.13,0.3,FALSE,FALSE,FALSE,FALSE,0,JPG,1,FALSE,FALSE};


void arg_processing (int argc,char** argv){
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
		if (strcmp (argv[n],"--skip") == 0 ) { 
			cout << " * Skipping if final image exists \n";
			maindata.skip=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--version") == 0 || strcmp (argv[n],"-v") == 0 ) { 
			printf (" * Version: %s\n",version);
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--savemask") == 0 ) { 
			cout << " * Saving blurred mask. \n";
			maindata.savemask=TRUE;
			shadow_argv[n]=true; }
		if (strcmp (argv[n],"--title") == 0 || strcmp (argv[n],"--text") == 0 ) { 
			if (shadow_argv[n+1]==false && n+1<argc) {
				maindata.title=argv[n+1];
				printf (" * Text to be inserted: %s \n",maindata.title);
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
		if (strcmp (argv[n],"--textsize") == 0) {
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
		// --minsharp				
		if (strcmp (argv[n],"--minsharp") == 0 ||  strcmp (argv[n],"--minsh") == 0 ) {
			if (shadow_argv[n+1]==false && n+1<argc) {
				tmpfloat=atof(argv[n+1]);
				if (tmpfloat==0)
					printf (" ! Check the argument for --minsharp  \n");
				else if (tmpfloat<0.01 || tmpfloat>0.5)
					printf (" ! --minsharp got value out of allowed range (0,01-0.5), using default... \n");
				else {
					maindata.minsharp1=tmpfloat;
					printf (" * Minimal sharpness set to: %.2f \n",maindata.minsharp1);}
				shadow_argv[n]=true;shadow_argv[n+1]=true; }
			else {
				cout << " --minsharp needs an parameter (float or integer) \n";
				shadow_argv[n]=true; }
				if (maindata.maxsharp1<maindata.minsharp1+0.05) maindata.maxsharp1=maindata.minsharp1+0.05; 
				}			
		
				
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
	cout << "As by now, \"final_\" prefix is appended to the filename. Original images are not touched (of course).\n";	
	cout << "\n =====   CLI switches =======\n";
	printf("%-20s  %s\n", "--half", "Process only half of image");
	printf("%-20s  %s\n", "--nosat", "Do not modify saturation");
	printf("%-20s  %s\n", "--nobr", "Do not modify brightness (&contrast)");
	printf("%-20s  %s\n", "--nosharp", "No sharpening (USM)");	
	printf("%-20s  %s\n", "--version; -v", "Prints out version (and proceeds with processing)");
	printf("%-20s  %s\n", "--title $YOURTEXT", "Insert text into left bottom corner.");
	printf("%-20s  %s\n", " ","(Use '' to encapsulate a text with blanks)");
	printf("%-20s  %s\n", "--textsize $FLOAT", "Relative size of text (default: 1).");	
	printf("%-20s  %s\n", "--skip", "Skip image if final image exists.");	
	printf("%-20s  %s\n", "--mpx $DECIMAL", "Final size of image in MPx");
	printf("%-20s  %s\n", "--minsat $DECIMAL", "Modifies bottom saturation target");
	printf("%-20s  %s\n", "--minsharp $DECIMAL", "Modifies bottom sharpness target");
	printf("%-20s  %s\n", "--png", "Save as 8-bit png, default is jpg");
	printf("%-20s  %s\n", "--newname $PATTERN", "Naming pattern for final images, * will be replaced");
	printf("%-20s  %s\n", " "					, " by old filename, # by images counter, so with pattern");
	printf("%-20s  %s\n", " "					, " 'vacation_#_*', the name IMG_0586.jpg will be changed");
	printf("%-20s  %s\n", " "					, " to 'vacation_0001_IMG_0586.jpg' ");
	printf("%-20s  %s\n", "--help; -h", "Prints help and quits");
	printf("\n");
	}

inline int get_basepos(int x, int y, int width){
	return y*width + x;}

void take_samples(int action){
	int x,y,xpoint,ypoint,basepos,halfstepx,halfstepy,basepos_sample;
	
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
		if (action==SHARP)
			sample_sharp1[basepos_sample]=abs(br[basepos]-mask1[basepos]);
		if (action==SATUR)
			{
			sample_sat[basepos_sample]=sat[basepos];
			}
		}}
	
}


void blur(float *input, float *output,float relradius,int x_dim,int y_dim){
	int steps[10]={0,0,0,0,0,0,0,0,0,0};
	int cummulative, expected;
	float sum;
	int count,radius,i,j,x,y,basepos,basepos_local;
	const bool debug=FALSE;
	const bool verbose=FALSE;
	
	if (verbose==TRUE) printf( " Starting blur()\n");
	
	if(relradius>=1.0) radius=(int)relradius; // the value is to be takes as absolute in pixels
	else {
		radius=((x_dim+y_dim) * relradius);
		if (radius>500) radius=500;
		if (radius<1) radius=1;}
	if (verbose==TRUE) printf ( "   Calculated absolute radius for blur: %2d (received: %.2f)\n",radius,relradius);
	
	steps[0]=1;cummulative=1;
	for (i=1;i<10;i+=1) {
		expected=steps[i-1]*2;
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

void change_brightness(float *array, float br_gamma, float contr_gamma){
	float new_br,new_contr;
	int x,y,basepos;
	
	for (x=0;x<maindata.source_x_size;x+=1) {
		if (x<maindata.source_x_size/2 && maindata.halfmode==TRUE) continue;
		for (y=0;y<maindata.source_y_size;y+=1) {	
		basepos=get_basepos(x,y,maindata.source_x_size);		
		get_brightness(array[basepos],br_gamma,contr_gamma,&new_br,&new_contr);
		array[basepos]=new_br;
		}}
	
	}

void apply_gamma(float *array, float gamma){
	int x,y,basepos;
	
	for (x=0;x<maindata.source_x_size;x+=1) {
		if (x<maindata.source_x_size/2 && maindata.halfmode==TRUE) continue;
		for (y=0;y<maindata.source_y_size;y+=1) {
		basepos=get_basepos(x,y,maindata.source_x_size);
		array[basepos]=my_pow(array[basepos],gamma);}
	}
	}

float linear_calibrate(float *array,float minlimit,float maxlimit,float pre_gamma,const char *title,float tresh, float *newvalue){
	//int i,good,outofrange;
	float avg,boost;
	float oldvalue;

	
	//testing if values are big enough for meaningfull processing
		
	avg=get_waverage(&array[0],pre_gamma);
	oldvalue=avg;
	if (avg<0.01) {
		printf ("  Image with extremely low values (%.3f), using boost=1\n",avg);
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
	
	
void get_brightness(float value, float br_gamma, float contr_gamma, float *new_br, float *new_contr){
	float tmp;
	float old_contrast=0;
	float new_contrast;
	const bool debug=FALSE;
	tmp=my_pow(value,br_gamma);
	if (tmp>0.5) old_contrast=my_pow((tmp-0.5)/0.5,contr_gamma);
	if (tmp<0.5) old_contrast=my_pow((0.5-tmp)/0.5,contr_gamma);
	
	//applying contrast gamma
	new_contrast=my_pow(old_contrast,contr_gamma);
	
	//calculating new brightness
	if (tmp>0.5) *new_br=0.5 + new_contrast/2.0;
	else if (tmp<0.5) *new_br=0.5 - new_contrast/2.0;	
	else *new_br=0.5;
	
	if (debug) {
		printf ("  old brightness: %.3f , old contrast: %.3f\n",value,old_contrast);
		printf ("   new brightness: %.3f, new contrast: %.3f\n",*new_br,new_contrast);
		printf ("   applied gammas: brightness: %.3f contrast %.3f\n",br_gamma,contr_gamma);}

	*new_contr=new_contrast;
	}


void double_calibrate(float *array, float *br_gamma, float *contr_gamma,float br_min,float br_max, float contr_min, float contr_max){
	
	*br_gamma=1.0;
	*contr_gamma=1.0;
	float l_br_gamma=1.0;
	float l_contr_gamma=1.0;
	float new_br=1.0;
	float new_contr=1.0;
	int i,j;
	float sum;
	float br_avg,contr_avg;
	float oldbr, oldcontr;
	const bool debug=FALSE;
	
	oldcontr=-1; //just to initialize it
	
	//in every step we test brightness and then contrast
	for (j=0;j<30;j+=1){
		if (debug) printf ("  double_calibrate - iteration: %1d\n",j);
		//testing brightness
		sum=0;
		for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1){
			get_brightness(array[i],l_br_gamma,l_contr_gamma,&new_br,&new_contr);
			sum+=new_br;}
		br_avg=sum/POINTSPERLINE/POINTSPERLINE;
		if (j==0) oldbr=br_avg;
		if (br_avg> br_min && br_avg<br_max) ;
		else if (br_avg<br_min)l_br_gamma-=0.02;
		else l_br_gamma+=0.02;
		
		//testing contrast
		sum=0;
		for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1){
			get_brightness(array[i],l_br_gamma,l_contr_gamma,&new_br,&new_contr);
			sum+=new_contr;}
		contr_avg=sum/POINTSPERLINE/POINTSPERLINE;
		if (j==0) oldcontr=contr_avg;
		if (contr_avg> contr_min && contr_avg<contr_max) ;
		else if (contr_avg>contr_max)l_contr_gamma+=0.02;
		else l_contr_gamma-=0.02;	
		
		if (debug) printf(" Iter. results: br gamma: %.2f / value: %.3f; contrast gamma:%.2f / value: %.3f(targ:%.3f - %.3f)\n",
					l_br_gamma,br_avg,l_contr_gamma,contr_avg,contr_min,contr_max);
		}
	

	*br_gamma= l_br_gamma  ;
	*contr_gamma=l_contr_gamma;

	printf("  Brightness gamma: %.2f, change: %.2f->%.2f (target: %.2f-%.2f)\n", 
		*br_gamma,oldbr,br_avg,br_min,br_max);
	printf("  Contrast gamma  : %.2f, change: %.2f->%.2f (target: %.2f-%.2f)\n", 
		*contr_gamma,oldcontr,contr_avg,contr_min,contr_max);
	
}


void get_ordered(float *array){
	float tmp;
	int i,j;
	const bool verbose=FALSE;
	
	//ordering
	for (j=0;j<POINTSPERLINE*POINTSPERLINE-1;j+=1){
		for (i=0;i<POINTSPERLINE*POINTSPERLINE-1-j;i+=1){
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
		for (j=0;j<POINTSPERLINE*POINTSPERLINE;j+=1){
			sum1	+=array[j];}
		printf ( "   Normal average: %.3f\n", sum1/POINTSPERLINE/POINTSPERLINE);
		//weightordered average
		int sum2=0;
		for (j=0;j<POINTSPERLINE*POINTSPERLINE;j+=1){
			sum2	+=array[j]*j;wcount+=j;}
		printf ( "   Weightordered average: %.3f ( + %.2f%%)\n", (float)sum2/wcount , (float)(sum2/(float)wcount) / (sum1/(float)POINTSPERLINE/(float)POINTSPERLINE) * 100.0 - 100.0);}	
	}

float get_waverage(float *array,float gamma){
	int i,wcount;
	float sum;
	
	sum=0;wcount=0;

	for (i=0;i<POINTSPERLINE*POINTSPERLINE;i+=1){
		sum+= my_pow(array[i],gamma) * i;
		wcount+=i;
		}
	return (float)sum/wcount;
	}
	

float my_pow(float base,float exp){
	float weight;
	weight=pow(base,0.15);
	return pow(base,exp)*weight + base*(1-weight);
}

float saturation_calibrate(float *array,float minlimit,float maxlimit,const char *title, float *oldvalue,float *newvalue){
	float avg,gamma,newr,newg,newb,new_sat;
	int j,basepos,basepos_sample,x,y,xpoint,ypoint,halfstepx,halfstepy;
	const bool debug=FALSE;
	//testing if there are enough samples for calibration - rework!
	
	get_ordered(&array[0]);
	avg=get_waverage(&array[0],1.0);
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
	
		get_ordered(&array[0]);
		avg=get_waverage(&array[0],gamma);
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
			
		
	printf("  %s gamma: %.2f, change: %.2f->%.2f (target: %.2f-%.2f)\n", 
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
	
void apply_sharp_boost(float sharp_boost){
	int x,y,basepos;
	float weight, local_boost,diff;
	for (x=0;x<maindata.source_x_size;x+=1) {
		if (x<maindata.source_x_size/2 && maindata.halfmode==TRUE) continue;	
		for (y=0;y<maindata.source_y_size;y+=1) {
			basepos=get_basepos(x,y,maindata.source_x_size);
			
			//sharpening
			diff=(br[basepos]-mask1[basepos]);
			if (br[basepos]<0.15 and diff<0) weight=br[basepos]/0.15;
			else if (br[basepos]>0.85 and diff>0) weight=(1-br[basepos])/0.15;
			else weight=1;
			local_boost=(sharp_boost-1.0)*weight + 1.0;
			br[basepos]=(br[basepos]-mask1[basepos])*local_boost+mask1[basepos];
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
			
	

void process(float saturation_gamma,float saturation_pre_boost){
	int x,y,basepos;
	float applied_sat;
	
	for (x=0;x<maindata.source_x_size;x+=1) {
		if (x<maindata.source_x_size/2 && maindata.halfmode==TRUE) continue;
		for (y=0;y<maindata.source_y_size;y+=1) {
			basepos=get_basepos(x,y,maindata.source_x_size);
			resaturate(basepos, saturation_gamma, &r[basepos],&g[basepos],&b[basepos],&applied_sat);
			}}
	}


void put_in_stat(float value,int *stat){
	if (value>1) stat[0]+= 1;
	else if (value<1) stat[1]+= 1;	
	else stat[2]+= 1;
	}


void insert_text(CImg<unsigned char>* img,float outer_x, float outer_y, float sizeratio){
	int img_x_pos,img_y_pos,i;
	int label_x_pos,label_y_pos;
	int offset,size,neededx,neededy,label_x_size,label_y_size,blur_radius;
	int label_basepos;
	int oldr,oldg,oldb;
	int foregroundcol[3]={255,255,0};
	int backgroundcol[3]={0,0,150};
	int newr,newg,newb;
	unsigned char w1[] = { 255 };
	size=(maindata.source_x_size+maindata.source_y_size)/80;
	blur_radius=size*sizeratio/10;
	offset=size/10;
	if (offset<1) offset=1;
	
	CImg<unsigned char> empty;
	empty.draw_text(0,0,maindata.title,w1,NULL,1,size*sizeratio);
	neededx=empty.width();
	neededy=empty.height();
	label_x_size=neededx+2*blur_radius+2;
	label_y_size=neededy+2*blur_radius+2;
	//printf ("    Label size: %2d %2d, text size: %2d x %2d, blur radius: %2d\n",label_x_size,label_y_size,neededx,neededy,blur_radius);
	//empty.save_jpeg("label.png");
	
	float *bg_weight= new float[label_x_size*label_y_size];
	float *bg_weight_blurred= new float[label_x_size*label_y_size];
	float *fg_weight= new float[label_x_size*label_y_size];	
	float *fg_weight_blurred= new float[label_x_size*label_y_size];	
	
	
	//putting in weights for foreground&background and blurring what is to be blurred
	for (label_x_pos=0;label_x_pos<label_x_size;label_x_pos+=1) { for (label_y_pos=0;label_y_pos<label_y_size;label_y_pos+=1) {
		label_basepos=get_basepos(label_x_pos,label_y_pos,label_x_size);
		if (label_x_pos<blur_radius+1 || label_y_pos<blur_radius+1 
			|| label_x_pos >=blur_radius+1+neededx || label_y_pos >=blur_radius+1+neededy) {
			bg_weight[label_basepos]=0;
			fg_weight[label_basepos]=0;}
		else {
			//empty_basepos=get_basepos(label_x-blur_radius-1,label_y-blur_radius-1,neededx);
			bg_weight[label_basepos]=empty(label_x_pos-blur_radius-1,label_y_pos-blur_radius-1,0)/255.0;
			fg_weight[label_basepos]=empty(label_x_pos-blur_radius-1,label_y_pos-blur_radius-1,0)/255.0;
			//if (label_x_pos%10==1 && label_y_pos%10==1)
				//printf ("  For label position %2d %2d entering value: %.3f\n",label_x_pos,label_y_pos,fg_weight[label_basepos]);
			}
		}}	
	//blurring
	blur(&bg_weight[0],&bg_weight_blurred[0],(float)blur_radius,label_x_size,label_y_size);
	blur(&fg_weight[0],&fg_weight_blurred[0],1,label_x_size,label_y_size);
	for (i=0;i<	label_x_size*label_y_size;i+=1){
		fg_weight_blurred[i]=2*fg_weight_blurred[i];
		if(fg_weight_blurred[i]>1.0) fg_weight_blurred[i]=1.0;
		bg_weight_blurred[i]=6*bg_weight_blurred[i];
		if(bg_weight_blurred[i]>1.0) bg_weight_blurred[i]=1.0;		
		
		}
			
	////putting background&foreground into img
	for (label_x_pos=0;label_x_pos<label_x_size;label_x_pos+=1) { for (label_y_pos=0;label_y_pos<label_y_size;label_y_pos+=1) {
		label_basepos=get_basepos(label_x_pos,label_y_pos,label_x_size);
		img_x_pos=maindata.source_x_size*outer_x-label_x_size+label_x_pos;
		img_y_pos=maindata.source_y_size*outer_y-label_y_size+label_y_pos;
		oldr=(int)(*img)(img_x_pos,img_y_pos,0);
		oldg=(int)(*img)(img_x_pos,img_y_pos,1);	
		oldb=(int)(*img)(img_x_pos,img_y_pos,2);
		//applying background
		oldr=bg_weight_blurred[label_basepos]*backgroundcol[0] + oldr*(1-bg_weight_blurred[label_basepos]);
		oldg=bg_weight_blurred[label_basepos]*backgroundcol[1] + oldg*(1-bg_weight_blurred[label_basepos]);
		oldb=bg_weight_blurred[label_basepos]*backgroundcol[2] + oldb*(1-bg_weight_blurred[label_basepos]);
		
		newr=fg_weight_blurred[label_basepos]*foregroundcol[0] + oldr*(1-fg_weight[label_basepos]);
		newg=fg_weight_blurred[label_basepos]*foregroundcol[1] + oldg*(1-fg_weight[label_basepos]);
		newb=fg_weight_blurred[label_basepos]*foregroundcol[2] + oldb*(1-fg_weight[label_basepos]);
		if (newr>255) newr=255;
		if (newg>255) newg=255;
		if (newb>255) newb=255;

		(*img)(img_x_pos,img_y_pos,0)=newr;
		(*img)(img_x_pos,img_y_pos,1)=newg;
		(*img)(img_x_pos,img_y_pos,2)=newb;
	}}

}

	
void insert_final(CImg<unsigned char>* final_img, float *source,int output) {
	int x,y,basepos;
	float Rnew,Bnew,Gnew;
	char Rfinal,Gfinal,Bfinal;
	
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
			Rfinal=my_pow(Rnew,2.2)*255;
			Gfinal=my_pow(Gnew,2.2)*255;
			Bfinal=my_pow(Bnew,2.2)*255;
			//inserting into image
			(*final_img)(x,y,0,0)=Rfinal;
			(*final_img)(x,y,0,1)=Gfinal;
			(*final_img)(x,y,0,2)=Bfinal;}}
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
	
	
float calculate_saturation(int basepos){
	const bool debug=FALSE;
	float brightness,chrome,saturation,hue,weight,partial;
	int i;
	
	chrome=max(max(r[basepos],g[basepos]),b[basepos]) - min(min(r[basepos],g[basepos]),b[basepos]);
	if (chrome==0) return 0;
	
	brightness=calculate_brightness(r[basepos]+br[basepos],g[basepos]+br[basepos],b[basepos]+br[basepos]);
	if (brightness<0.005) brightness=0.005;
	saturation=my_pow(brightness,0.1)*chrome;
	
	
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

//float get_disalignment(){
	//int halfstepx,halfstepy,x,y,basepos,count,xpoint,ypoint;
	//float sumr,sumg,sumb;
	
	//count=0;sumr=0;sumg=0;sumb=0;
	//halfstepx=maindata.source_x_size/POINTSPERLINE/2.0;
	//halfstepy=maindata.source_y_size/POINTSPERLINE/2.0;
	//for (xpoint=0;xpoint<POINTSPERLINE;xpoint+=1) {for (ypoint=0;ypoint<POINTSPERLINE;ypoint+=1) {	
		//x=halfstepx+xpoint*maindata.source_x_size/POINTSPERLINE;
		//y=halfstepy+ypoint*maindata.source_y_size/POINTSPERLINE;
		////printf (" Testing pixel %4d x %4d\n",x,y);
		//basepos=get_basepos(x,y,maindata.source_x_size);
		//if (br[basepos] + (max (r[basepos], max(g[basepos],b[basepos]))) > 0.98) continue;
		//if (br[basepos] - (min (r[basepos], min(g[basepos],b[basepos]))) < 0.02) continue;
		//sumr+=r[basepos];
		//sumg+=g[basepos];
		//sumb+=b[basepos];
		//count+=1;
		//}}
		
	//for (x=0;x<maindata.source_x_size*maindata.source_y_size;x+=1){
	//r[x]=r[x]-sumr/count;
	//g[x]=g[x]-sumg/count;
	//b[x]=b[x]-sumb/count;}
	
	//printf("  rgb disalignment: %.3f %.3f %.3f (samples: %1d)\n",sumr/count,sumg/count,sumb/count,count);
//}

void populate(CImg<unsigned char>* srcimgL){
	int x,y,basepos,R,G,B;
	int x1,y1;
	//float sumbr,summask;
	int tmp;

	//first allocating memory for arrays
	if (maindata.source_size>allocated){
		delete [] r;delete [] g;delete [] b;delete [] br;delete [] sat;
		float* r_tmp = new float[maindata.source_size];
		float* g_tmp = new float[maindata.source_size];
		float* b_tmp = new float[maindata.source_size];
		float* br_tmp = new float[maindata.source_size];
		float* sat_tmp = new float[maindata.source_size];
		float* mask1_tmp = new float[maindata.source_size];
		float* tmp1_tmp = new float[maindata.source_size];
						
		r=r_tmp;
		g=g_tmp;
		b=b_tmp;
		br=br_tmp;
		sat=sat_tmp;
		mask1=mask1_tmp;
		tmp1=tmp1_tmp;
		allocated=maindata.source_size;}
	
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
			
			basepos=get_basepos(x,y,maindata.source_x_size);
			r[basepos]=my_pow((float)R/255,1/2.2);
			g[basepos]=my_pow((float)G/255,1/2.2);			
			b[basepos]=my_pow((float)B/255,1/2.2);
			br[basepos]=calculate_brightness(r[basepos],g[basepos],b[basepos]);
			//(r[basepos]+g[basepos]+b[basepos])/3.0;
			r[basepos]=r[basepos]-br[basepos];
			g[basepos]=g[basepos]-br[basepos];
			b[basepos]=b[basepos]-br[basepos];
			//br[basepos]=0.15+br[basepos]*0.9; // delete this !
			}}
	}
float calculate_brightness(float r,float g, float b){
	float brightness;
	//brightness=( 4.0 * (3.0*r+6.0*g+b)/10.0 + max(r,max(g,b))  ) / 5.0;
	brightness=r*0.6 + b*0.1 + g*0.3;
	
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

string get_new_name(string filename,int number){
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
		aspos=newname.find_first_of("*");
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
		aspos=newname.find_first_of("*#");
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


int main(int argc, char** argv){
	//char filename[1000];
	//int n;
	float brightness_gamma,contrast_gamma,saturation_gamma,saturation_boost,sharpness_boost,tmp;
	//int filestatus=-1;
	
	arg_processing(argc, argv);
	
	// EXIT IF NO IMAGES
	if (img_count <1) {
		cout << " ! TERMINATING: No images to process... \n";
		exit(1);}

   	// LISTING IMAGES 
   	cout <<" * Image(s): ";
	for (int i=0;i<img_count;i=i+1) {	cout <<images[i]<< " ";}
	cout <<"\n"; 
	cout << " * Images count: " << img_count << "\n";

	//ITERATING over images
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

		CImg<unsigned char> srcimg(char_filename) ;
		
		maindata.source_x_size = srcimg.width();maindata.source_y_size = srcimg.height();
		printf ("==> Image  %2d/%2d: %-15s ( %4d x %4d)\n",n+1,img_count,char_filename,maindata.source_x_size,maindata.source_y_size);

		maindata.source_size=maindata.source_y_size*maindata.source_x_size;
		
		//populating internal array, save for sat -it will be done later on
		if(maindata.rotation<=0 || maindata.rotation>8)maindata.rotation=1; 
		if (maindata.rotation>1) printf ("    (rotating, original orientation: %d)\n",maindata.rotation);
		populate(&srcimg);
		
		//processing color disalignment
		//get_disalignment();

		//processing/changing brightness
		take_samples(BRIGHT);
		
		if (maindata.nobr==FALSE) double_calibrate(&sample_br[0] , &brightness_gamma, &contrast_gamma,
			maindata.minbr,maindata.maxbr,maindata.mincontr,maindata.maxcontr);
		else{
			brightness_gamma=1.0;
			contrast_gamma=1.0;}
		
		put_in_stat(brightness_gamma,&br_stat[0]);
		put_in_stat(contrast_gamma,&contr_stat[0]);
		
		
		//calculating & applying sharpening
		change_brightness(&br[0],brightness_gamma,contrast_gamma);  //modifying br
		if (maindata.nosharp==FALSE) {
			blur(&br[0],&mask1[0],0.1,maindata.source_x_size,maindata.source_y_size);  // blurring br to mask1
			take_samples(SHARP);										//taking samples
			get_ordered(&sample_sharp1[0]);								//ordering them
			sharpness_boost=linear_calibrate(&sample_sharp1[0],maindata.minsharp1,maindata.maxsharp1,1.0,"Sharpness",2,&tmp);
			put_in_stat(1.0/sharpness_boost,&sharp_stat[0]);		 	//calculating sharpness boost
			apply_sharp_boost(sharpness_boost);							//changing br
			}
		else
			put_in_stat(1.0,&sharp_stat[0]);	
		

		
		//calculating sat changes
		if (maindata.nosat==false) {
			populate_sat();						//populating sat array
			take_samples(SATUR);
			float oldvalue=0;float newvalue=0;		
			saturation_gamma=saturation_calibrate(&sample_sat[0],maindata.minsat,maindata.maxsat,"Saturation",&oldvalue,&newvalue);
			saturation_boost=1.0;
			//saturation_boost=linear_calibrate(&sample_sat[0],maindata.minsat,maindata.maxsat,saturation_gamma,"Saturation",0.5,&newvalue);
			put_in_stat(oldvalue/newvalue,&sat_stat[0]);
		}
		else{
			saturation_gamma=1.0;
			saturation_boost=1.0;}
		
		process(saturation_gamma,saturation_boost);
		
		
		// creating NEW IMAGE
		CImg<unsigned char> final_img(maindata.source_x_size,maindata.source_y_size,1,3);
		insert_final(&final_img,&br[0],IMAGE);
		
		//saving mask if needed
		if (maindata.savemask==TRUE) {
			CImg<unsigned char> sharp_mask(maindata.source_x_size,maindata.source_y_size,1,3);
			insert_final(&sharp_mask,&mask1[0],MASK);
			string newmaskfilename=("mask_"+newfilename);
			const char *char_newmaskfilename = newmaskfilename.c_str();
			sharp_mask.save_jpeg(char_newmaskfilename,89);}
			
		//inserting text
		insert_text(&final_img,0.95,0.92,maindata.textsize);
		

		
		//ulozenie
		if (maindata.mpx_resize>0){
			int new_x,new_y;
			new_x=maindata.source_x_size * my_pow(1000000*maindata.mpx_resize/maindata.source_x_size/maindata.source_y_size,0.5);
			new_y=maindata.source_y_size * my_pow(1000000*maindata.mpx_resize/maindata.source_x_size/maindata.source_y_size,0.5);
			printf ("  Rescaling to %2d x %2d (%.2f MPx)\n",new_x,new_y,new_x*new_y/1000000.0);
			final_img.resize(new_x,new_y,-100,-100,5);
			}
		if 			(maindata.output==PNG) final_img.save_png(char_savename);	
		else		final_img.save_jpeg(char_savename,89);
		printf("  Saved as: %s\n",newfilename.c_str());
		//final_img.save_png(char_savename);	
		
		}
			
	printf( "==SUMMARY (per change - count of images):\n");
	printf ("   %-20s |%10s |%10s |%11s|\n","Type of modif.","Decreased","Increased", "Not changed");
	printf ("   %-20s |%10d |%10d |%11d|\n","Brightness",br_stat[0],br_stat[1],br_stat[2]);
	printf ("   %-20s |%10d |%10d |%11d|\n","Contrast",contr_stat[0],contr_stat[1],contr_stat[2]);
	printf ("   %-20s |%10d |%10d |%11d|\n","Saturation",sat_stat[0],sat_stat[1],sat_stat[2]);		
	printf ("   %-20s |%10d |%10d |%11d|\n","Sharpness",sharp_stat[0],sharp_stat[1],sharp_stat[2]);	
	}
