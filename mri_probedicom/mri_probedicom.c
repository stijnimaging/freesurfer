#ifndef OPENGL
#define OPENGL 
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/file.h>
#include <sys/types.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <ctype.h>
#include <dirent.h>
#ifndef Darwin
#include <malloc.h>
#endif
#ifdef HAVE_APPLE_GLUT_FRAMEWORK
#  include <GLUT/glut.h>
#else
#  include <GL/glut.h>
#endif
#include "error.h"
#include "diag.h"
#include "mri.h"
#include "mri_identify.h"
#include "DICOMRead.h"
#include "mri2.h"
#include "bfileio.h"
#include "proto.h"
#include "version.h"

int main(int argc, char *argv[]) ;

static char vcid[] = "$Id: mri_probedicom.c,v 1.12 2006/11/08 01:46:38 greve Exp $";
char *Progname = NULL;

static int  parse_commandline(int argc, char **argv);
static void check_options(void);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;
static void argnerr(char *option, int n);
static int  singledash(char *flag);
int GetDirective(char *directivestring);
int GetDimLength(char *dicomfile, int dimtype);
//DCM_ELEMENT *GetElementFromFile(char *dicomfile, long grpid, long elid);

#define QRY_FILETYPE        0
#define QRY_TAG             1
#define QRY_REPRESENTATION  2
#define QRY_DESCRIPTION     3
#define QRY_MULTIPLICITY    4
#define QRY_LENGTH          5
#define QRY_VALUE           6

char* dicomfile = NULL; 
char* directivestring = NULL;
int   directive;
long grouptag = -1, elementtag = -1;
int debug, verbose;
char *outputfile = NULL;
int  outputbfile = 0;
char *tagname = NULL;
int GettingPixelData = 0;
FILE *fp;
int DisplayImage = 0;
int DoPartialDump = 1;

//int AllocElement(DCM_ELEMENT *e);
//int FreeElement(DCM_ELEMENT *e);
//DCM_OBJECT *GetObjectFromFile(char *fname, unsigned long options);
int DumpElement(FILE *fp, DCM_ELEMENT *e);
char *RepString(int RepCode);
char *ElementValueString(DCM_ELEMENT *e);
int PartialDump(char *dicomfile, FILE *fp);
int DumpSiemensASCII(char *dicomfile, FILE *fpout);

/*size_t RepSize(int RepCode);*/
char *ElementValueFormat(DCM_ELEMENT *e);

#define TMPSTRLEN 10000
static char tmpstr[TMPSTRLEN];

int RenderImage(int argc, char **argv);
int ImageWidth;
int ImageHeight;
GLubyte *ImageBuff;

int DoPatientName = 1;

/*---------------------------------------------------------------*/
int main(int argc, char **argv)
{
  DCM_OBJECT *object;
  CONDITION cond;
  DCM_ELEMENT element;
  DCM_TAG tag;
  unsigned int rtnLength;
  void * Ctx = NULL;
  int nrows, ncols, endian;
  int nargs;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, "$Id: mri_probedicom.c,v 1.12 2006/11/08 01:46:38 greve Exp $", "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  tmpstr[0] = 'a'; /* to stop compiler warning */

  Progname = argv[0] ;
  argc --;
  argv++;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  if(argc == 0) usage_exit();

  parse_commandline(argc, argv);
  check_options();

  /*--------------------------------------------------------------*/
  if(directive == QRY_FILETYPE){

    if(! IsDICOM(dicomfile)){
      printf("notdicom\n");
      exit(0);
    }

    cond=DCM_OpenFile(dicomfile, DCM_PART10FILE|DCM_ACCEPTVRMISMATCH, &object);
    if (cond == DCM_NORMAL){
      printf("part10\n");
      exit(0);
    }

    cond=DCM_OpenFile(dicomfile, DCM_ORDERLITTLEENDIAN|DCM_ACCEPTVRMISMATCH, &object);
    if (cond == DCM_NORMAL){
      printf("littleendian\n");
      exit(0);
    }

    cond=DCM_OpenFile(dicomfile, DCM_ORDERBIGENDIAN|DCM_ACCEPTVRMISMATCH, &object);
    if (cond == DCM_NORMAL){
      printf("bigendian\n");
      exit(0);
    }

    fprintf(stderr,"ERROR: cannot determine file type\n");
    exit(1);
  }/*--------------------------------------------------------------*/

  if(! IsDICOM(dicomfile)){
    fprintf(stderr,"ERROR: %s is not a dicom file\n",dicomfile);
    exit(1);
  }

  if(DisplayImage){
    RenderImage(argc,argv);
    return(0);
  }

  tag=DCM_MAKETAG(grouptag,elementtag);

  object = GetObjectFromFile(dicomfile, 0);
  if(object == NULL) exit(1);

  cond = DCM_GetElement(&object, tag, &element);
  if(cond != DCM_NORMAL){
    COND_DumpConditions();
    exit(1);
  }

  AllocElementData(&element);

  cond = DCM_GetElementValue(&object, &element, &rtnLength, &Ctx);
  if(cond != DCM_NORMAL){
    COND_DumpConditions();
    exit(1);
  }

  switch(directive){

  case QRY_TAG: 
    printf("%d\n",element.tag);
    break;
  case QRY_REPRESENTATION: 
    printf("%s\n",RepString(element.representation));
    break;
  case QRY_DESCRIPTION: 
    printf("%s\n",element.description);
    break;
  case QRY_MULTIPLICITY: 
    printf("%ld\n",element.multiplicity);
    break;
  case QRY_LENGTH: 
    printf("%d\n",element.length);
    break;
  case QRY_VALUE: 
    if(!GettingPixelData){
      if(outputfile == NULL)
	printf("%s\n",ElementValueString(&element));
      else{
	fp = fopen(outputfile,"w");
	fprintf(fp,"%s\n",ElementValueString(&element));
	fclose(fp);
      }
    }
    else {
      if(outputbfile){
	sprintf(tmpstr,"%s.hdr",outputfile);
	ncols = GetDimLength(dicomfile,0);
	nrows = GetDimLength(dicomfile,1);
	endian = bf_getarchendian();
	fp = fopen(tmpstr,"w");
	fprintf(fp,"%d %d 1 %d\n",nrows,ncols,endian);
        fclose(fp);
	sprintf(tmpstr,"%s.bshort",outputfile);
      }
      else
	sprintf(tmpstr,"%s",outputfile);
      
      //printf("Writing Pixel Data to %s\n",tmpstr);
      fp = fopen(tmpstr,"w");
      fwrite(element.d.string,sizeof(char),element.length,fp);
      fclose(fp);
      //printf("Done\n");
    }
    break;
    
  }
  
  DCM_CloseObject(&object);
  
  /*DumpElement(stdout,&element);
  DCM_DumpElements(&object,0);*/

  return(0);
}
/*---------------------------------------------------------------*/
/* -----//////////////////<<<<<<<<>>>>>>>>>>\\\\\\\\\\\\\\\\---- */
/*---------------------------------------------------------------*/

/* --------------------------------------------- */
static int parse_commandline(int argc, char **argv)
{
  int  nargc , nargsused;
  char **pargv, *option ;
  int rt;

  if(argc < 1) usage_exit();

  nargc   = argc;
  pargv = argv;
  while(nargc > 0){

    option = pargv[0];
    if(debug) printf("%d %s\n",nargc,option);
    nargc -= 1;
    pargv += 1;

    nargsused = 0;

    if (!strcasecmp(option, "--help"))  print_help() ;
    else if (!strcasecmp(option, "--version")) print_version() ;
    else if (!strcasecmp(option, "--debug"))   debug = 1;
    else if (!strcasecmp(option, "--verbose")) verbose = 1;
    else if (!strcasecmp(option, "--no-name")) DoPatientName = 0;

    /* -------- source volume inputs ------ */
    else if (!strcmp(option, "--i")){
      if(nargc < 1) argnerr(option,1);
      dicomfile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--o")){
      if(nargc < 1) argnerr(option,1);
      outputfile = pargv[0];
      nargsused = 1;
      grouptag = 0x7FE0;
      elementtag = 0x10;
      DoPartialDump = 0;
    }
    else if (!strcmp(option, "--ob")){
      if(nargc < 1) argnerr(option,1);
      outputfile = pargv[0];
      outputbfile = 1;
      nargsused = 1;
      grouptag = 0x7FE0;
      elementtag = 0x10;
      DoPartialDump = 0;
    }
    else if (!strcmp(option, "--d")){
      if(nargc < 1) argnerr(option,1);
      directivestring = pargv[0];
      nargsused = 1;
      DoPartialDump = 0;
    }
    else if (!strcmp(option, "--n")){
      if(nargc < 1) argnerr(option,1);
      tagname = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--t")){
      if(nargc < 2) argnerr(option,2);
      sscanf(pargv[0],"%lx",&grouptag);
      sscanf(pargv[1],"%lx",&elementtag);
      nargsused = 2;
      DoPartialDump = 0;
    }
    else if (!strcmp(option, "--g")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%lx",&grouptag);
      nargsused = 1;
    }
    else if (!strcmp(option, "--e")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%lx",&elementtag);
      nargsused = 1;
    }
    else if (!strcmp(option, "--view")){
      DisplayImage = 1;
      DoPartialDump = 0;
      nargsused = 0;
    }
    else if (!strcmp(option, "--partial")){
      DoPartialDump = 1;
      nargsused = 0;
    }
    else if(!strcmp(option, "--dictionary") ||
      !strcmp(option, "--dic")){
      rt = system("dcm_print_dictionary");
      if(rt != 0){
  printf("ERROR: is dcm_print_dictionary in your path?\n");
  exit(1);
      }
      exit(0);
    }
    else{
      fprintf(stderr,"ERROR: Option %s unknown\n",option);
      if(singledash(option))
  fprintf(stderr,"       Did you really mean -%s ?\n",option);
      exit(-1);
    }
    nargc -= nargsused;
    pargv += nargsused;
  }
  return(0);
}
/* ------------------------------------------------------ */
static void usage_exit(void)
{
  print_usage() ;
  exit(1) ;
}
/* --------------------------------------------- */
static void print_usage(void)
{
  fprintf(stdout, "USAGE: %s \n",Progname) ;
  fprintf(stdout, "\n");
  fprintf(stdout, "   --i dicomfile     : path to dicom file \n");
  fprintf(stdout, "   --t group element : dicom group and element\n");
  fprintf(stdout, "   --d directive     : <val> filetype, tag, desc, mult, rep \n");
  fprintf(stdout, "   --no-name         : do not print patient name (10,10) with dump \n");
  fprintf(stdout, "   --view            : view the image  \n");
  fprintf(stdout, "   --o file          : dump binary pixel data into file  \n");
  fprintf(stdout, "   --ob stem         : dump binary pixel data into bshort  \n");
  fprintf(stdout, "   --dictionary      : dump dicom dictionary and exit\n");
  fprintf(stdout, "   --help            : how to use this program \n");
  fprintf(stdout, "\n");
}
/* --------------------------------------------- */
static void print_help(void)
{
  printf("\n");
  print_usage() ;
  printf("\n");

  printf("This program allows the user to query a dicom file. \n") ;

  printf(
"DESCRIPTION\n"
"\n"
"  In its most basic usage, the user supplies the DICOM group and\n"
"  element IDs of the data item to be queried along with the path to a\n"
"  DICOM file, and  mri_probedicom prints the value of the data item to\n"
"  stdout. If the file is not a DICOM file it will exit with a non-zero\n"
"  status. It is also possible to view the image, dump the pixel\n"
"  data to a file, and print out a basic set of information. \n"
"\n"
"  This uses the DICOM CTN libraries from Mallinckrodt Institute of\n"
"  Radiology (http://dicomctn.wustl.edu/DICOM/ctn-docs/doc_index.html).\n"
"\n"
"ARGUMENTS\n"
"\n"
"  --i dicomfile\n"
"\n"
"      Path to the dicomfile to probe. If this is the only option, a\n"
"      basic set of data will be printed out, including the Siemens\n"
"      ASCII header (if its a Siemens DICOM file).\n"
"\n"
"  --t group element\n"
"\n"
"      Group and element IDs in hexidecimal. Eg, --t 10 10 will return\n"
"      the Patient Name. The software will compute the actual tag.\n"
"\n"
"  --d directive\n"
"\n"
"      Specifies the aspect of the data item to probe. Possible values \n"
"      are: \n"
"        val - print out the value of the item (default)\n"
"        filetype - type of file. Return values are bigendian, littleendian,\n"
"          part10, or notadicom. Even if the file is not a DICOM file,\n"
"          the exit status will still be zero. It is not neccesary\n"
"          to supply a tag with this directive.\n"
"        tag - numeric value of the tag created by combining the group and\n"
"          element IDs.\n"
"        desc - description of the item.\n"
"        mult - multiplicity\n"
"        rep  - representation\n"
"\n"
"  --no-name\n"
"\n"
"Do not do not print patient name (10,10) with the basic set of information.\n"
"\n"
"  --view\n"
"\n"
"     Display image in an X window. Ignores tag option.\n"
"\n"
"  --o filename\n"
"\n"
"     Dump the binary pixel data to filename. \n"
"\n"
"  --ob stem\n"
"\n"
"     Dump the binary pixel data to stem.bshort and create header\n"
"     stem.hdr\n"
"\n"
"  --dictionary\n"
"\n"
"     Print out the DICOM dictionary. This just calls the CTN program \n"
"     dcm_print_dictionary (which must be in your path). Ignores all\n"
"     other options.\n"
"\n"
"AUTHOR\n"
"\n"
"     Written by Douglas N. Greve.\n"
"\n"
"BUG REPORTING\n"
"\n"
"     Send bug reports to analysis-bugs@nmr.mgh.harvard.edu\n"
"\n"
"BUGS\n"
"\n"
"     This has only been tested on Siemens DICOM files but it should\n"
"     work on any DICOM file. Note: the CTN software does not support\n"
"     float and double DICOM data formats.\n"
);




  exit(1) ;
}
/* --------------------------------------------- */
static void print_version(void)
{
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}
/* --------------------------------------------- */
static void argnerr(char *option, int n)
{
  if(n==1)
    fprintf(stderr,"ERROR: %s flag needs %d argument\n",option,n);
  else
    fprintf(stderr,"ERROR: %s flag needs %d arguments\n",option,n);
  exit(-1);
}
/*---------------------------------------------------------------*/
static int singledash(char *flag)
{
  int len;
  len = strlen(flag);
  if(len < 2) return(0);

  if(flag[0] == '-' && flag[1] != '-') return(1);
  return(0);
}
/* --------------------------------------------- */
static void check_options(void)
{

  if(dicomfile == NULL){
    fprintf(stderr,"ERROR: no file name supplied\n");
    exit(1);
  }

  if(DoPartialDump){
    PartialDump(dicomfile,stdout);
    DumpSiemensASCII(dicomfile, stdout);
    exit(0);
  }

  if(directivestring == NULL) directivestring = "value";
  directive = GetDirective(directivestring);

  if(!DisplayImage){
    if(directive != QRY_FILETYPE && (grouptag == -1 || elementtag == -1)){
      fprintf(stderr,"ERROR: must specify group and element when querying %s\n",
        directivestring);
      exit(1);
    }
  }

  if(grouptag == 0x7FE0 && elementtag == 0x10) GettingPixelData = 1;

  if(GettingPixelData && outputfile == NULL && directive == QRY_VALUE){
    fprintf(stderr,"ERROR: must specify output file when querying value of  pixel data\n");
    exit(1);
  }

  return;
}
/* ------------------------------------------------------------ */
int GetDirective(char *directivestring)
{
  if(! strncasecmp(directivestring,"filetype",1)) return(QRY_FILETYPE);
  if(! strncasecmp(directivestring,"tag",1)) return(QRY_TAG);
  if(! strncasecmp(directivestring,"representation",1)) return(QRY_REPRESENTATION);
  if(! strncasecmp(directivestring,"description",1)) return(QRY_DESCRIPTION);
  if(! strncasecmp(directivestring,"multiplicity",1)) return(QRY_MULTIPLICITY);
  if(! strncasecmp(directivestring,"length",1)) return(QRY_LENGTH);
  if(! strncasecmp(directivestring,"value",1)) return(QRY_VALUE);
  fprintf(stderr,"ERROR: Directive %s unrecognized\n",directivestring);
  exit(1);
}
/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/
int DumpElement(FILE *fp, DCM_ELEMENT *e)
{
  char *s;
  fprintf(fp,"tag %d\n",e->tag);
  fprintf(fp,"repcode %d\n",e->representation);
  fprintf(fp,"rep %s\n",RepString(e->representation));
  fprintf(fp,"desc %s\n",e->description);
  fprintf(fp,"mult %ld\n",e->multiplicity);
  fprintf(fp,"len %d\n",e->length);
  s = ElementValueString(e);
  fprintf(fp,"%s\n",s);
  free(&s);

  return(0);
}
/*---------------------------------------------------------------*/
char *RepString(int RepCode)
{
  char* repstring=NULL;

  switch(RepCode){

  case DCM_AE: repstring = "Application Entity"; break;
  case DCM_AS: repstring = "Age String"; break;
  case DCM_AT: repstring = "Attribute Tag"; break;
  case DCM_CS: repstring = "Code String"; break;
  case DCM_DA: repstring = "Date String"; break;
  case DCM_DS: repstring = "Decimal String"; break;
  case DCM_DT: repstring = "Date Time String"; break;
  case DCM_FD: repstring = "Floating Point Double"; break;
  case DCM_FL: repstring = "Floating Point Single"; break;
  case DCM_IS: repstring = "Integer String"; break;
  case DCM_LO: repstring = "Long String"; break;
  case DCM_LT: repstring = "Long Text"; break;
  case DCM_OB: repstring = "Other Byte String"; break;
  case DCM_OW: repstring = "Other Word String"; break;
  case DCM_PN: repstring = "Person Name"; break;
  case DCM_SS: repstring = "Signed Short"; break;
  case DCM_SH: repstring = "Short String"; break;
  case DCM_SL: repstring = "Signed Long"; break;
  case DCM_SQ: repstring = "Sequence of Items"; break;
  case DCM_ST: repstring = "Short Text"; break;
  case DCM_TM: repstring = "Time String"; break;
  case DCM_UI: repstring = "Unique Identifier"; break;
  case DCM_UL: repstring = "Unsigned Long"; break;
  case DCM_US: repstring = "Unsigned Short"; break;
  default: 
    fprintf(stderr,"RepString: %d unrecognized",RepCode);

  }
  return(repstring);
}
/*---------------------------------------------------------------*/
/*---------------------------------------------------------------
  ElementValueString() - returns the value of the element as a
  null terminated string. Does not parse multiple valued elements.
  For string elements, it just copies the string and adds a 
  terminator. For others, it just uses sprrintf.
  ---------------------------------------------------------------*/
char *ElementValueString(DCM_ELEMENT *e)
{
	// declared at top of file:  extern char tmpstr[TMPSTRLEN];
  char* evstring;
  int len;

  memset(&tmpstr[0],0,TMPSTRLEN);

  switch(e->representation){

  case DCM_AE: 
  case DCM_AS: 
  case DCM_CS: 
  case DCM_DA: 
  case DCM_DS: 
  case DCM_DT: 
  case DCM_IS: 
  case DCM_LO: 
  case DCM_LT: 
  case DCM_OB: 
  case DCM_OW: 
  case DCM_PN: 
  case DCM_SH: 
  case DCM_ST: 
  case DCM_TM: 
  case DCM_UI: 
    sprintf(tmpstr,"%s",e->d.string); 
    break;
  case DCM_SS: sprintf(tmpstr,"%d",(int)(*(e->d.ss)));  break;
  case DCM_SL: sprintf(tmpstr,"%ld",(long)(*(e->d.sl))); break;
  case DCM_UL: sprintf(tmpstr,"%ld",(long)(*(e->d.ul))); break;
  case DCM_US: sprintf(tmpstr,"%d",(int)(*(e->d.us)));  break;
  case DCM_AT: sprintf(tmpstr,"%ld",(long)(*(e->d.at))); break;
  default: 
    fprintf(stderr,"ElementValueSting: %d unrecognized",e->representation);
    return(NULL);
  }

  len = strlen(tmpstr);
  evstring = (char *) calloc(len+1,sizeof(char));
  memcpy(evstring,tmpstr,len+1);

  return(evstring);
}

/*------------------------------------------------------*/
int GetDimLength(char *dicomfile, int dimtype)
{
  int dimlength;
  DCM_OBJECT *object;
  CONDITION cond;
  DCM_ELEMENT element;
  DCM_TAG tag;
  unsigned int rtnLength;
  void * Ctx = NULL;

  object = GetObjectFromFile(dicomfile, 0);
  if(object == NULL) exit(1);

  if(dimtype == 0) tag=DCM_MAKETAG(0x28,0x11); /* ncols */
  else             tag=DCM_MAKETAG(0x28,0x10); /* nrows */

  cond = DCM_GetElement(&object, tag, &element);
  if(cond != DCM_NORMAL){
    COND_DumpConditions();
    exit(1);
  }
  AllocElementData(&element);
  cond = DCM_GetElementValue(&object, &element, &rtnLength, &Ctx);
  if(cond != DCM_NORMAL){
    COND_DumpConditions();
    exit(1);
  }
  dimlength = *element.d.us;

  FreeElementData(&element);
  DCM_CloseObject(&object);

  return(dimlength);
}
/*---------------------------------------------------------------*/
int PartialDump(char *dicomfile, FILE *fp)
{
  DCM_ELEMENT *e;

  e = GetElementFromFile(dicomfile, 0x8, 0x70);
  if(e != NULL){
    fprintf(fp,"Manufacturer %s\n",e->d.string);
    FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x8, 0x1090);
  if(e != NULL){
  fprintf(fp,"ScannerModel %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x1020);
  if(e != NULL){
    fprintf(fp,"SoftwareVersion %s\n",e->d.string);
    FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x1000);
  if(e != NULL){
  fprintf(fp,"ScannerSerialNo %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x8, 0x80);
  if(e != NULL){
  fprintf(fp,"Institution %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  // This should be "MoCoSeries" for on-scanner motion cor
  e = GetElementFromFile(dicomfile, 0x8, 0x103e);
  if(e != NULL){
  fprintf(fp,"SeriesDescription %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x20, 0xd);
  if(e != NULL){
  fprintf(fp,"StudyUID %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x8, 0x20);
  if(e != NULL){
  fprintf(fp,"StudyDate %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x8, 0x30);
  if(e != NULL){
  fprintf(fp,"StudyTime %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  if(DoPatientName){
    e = GetElementFromFile(dicomfile, 0x10, 0x10);
    if(e != NULL){
      fprintf(fp,"PatientName %s\n",e->d.string);
      FreeElementData(e); free(e);
    }
  }

  e = GetElementFromFile(dicomfile, 0x20, 0x11);
  if(e != NULL){
  fprintf(fp,"SeriesNo %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x20, 0x13);
  if(e != NULL){
  fprintf(fp,"ImageNo %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x23);
  if(e != NULL){
  fprintf(fp,"AcquisitionType %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x84);
  if(e != NULL){
  fprintf(fp,"ImagingFrequency %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x95);
  if(e != NULL){
  fprintf(fp,"PixelFrequency %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x85);
  if(e != NULL){
  fprintf(fp,"ImagedNucleus %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x86);
  if(e != NULL){
  fprintf(fp,"EchoNumber %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x87);
  if(e != NULL){
  fprintf(fp,"FieldStrength %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x24);
  if(e != NULL){
  fprintf(fp,"PulseSequence %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x1030);
  if(e != NULL){
  fprintf(fp,"ProtocolName %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x20);
  if(e != NULL){
  fprintf(fp,"ScanningSequence %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x1251);
  if(e != NULL){
  fprintf(fp,"TransmittingCoil %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x5100);
  if(e != NULL){
  fprintf(fp,"PatientPosition %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x1314);
  if(e != NULL){
  fprintf(fp,"FlipAngle %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x81);
  if(e != NULL){
  fprintf(fp,"EchoTime %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x80);
  if(e != NULL){
  fprintf(fp,"RepetitionTime %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x89);
  if(e != NULL){
  fprintf(fp,"NPhaseEnc %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x1312);
  if(e != NULL){
  fprintf(fp,"PhaseEncDir %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x88);
  if(e != NULL){
    fprintf(fp,"SliceDistance %s\n",e->d.string);
    FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x18, 0x50);
  if(e != NULL){
  fprintf(fp,"SliceThickness %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x28, 0x30);
  if(e != NULL){
  fprintf(fp,"PixelSpacing %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x28, 0x10);
  if(e != NULL){
  fprintf(fp,"NRows %d\n",*(e->d.us));
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x28, 0x11);
  if(e != NULL){
  fprintf(fp,"NCols %d\n",*(e->d.us));
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x28, 0x100);
  if(e != NULL){
  fprintf(fp,"BitsPerPixel %d\n",*(e->d.us));
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x28, 0x102);
  if(e != NULL){
  fprintf(fp,"HighBit %d\n",*(e->d.us));
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x28, 0x106);
  if(e != NULL){
  fprintf(fp,"SmallestValue %d\n",*(e->d.us));
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x28, 0x107);
  if(e != NULL){
  fprintf(fp,"LargestValue %d\n",*(e->d.us));
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x20, 0x37);
  if(e != NULL){
  fprintf(fp,"ImageOrientation %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x20, 0x32);
  if(e != NULL){
  fprintf(fp,"ImagePosition %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  e = GetElementFromFile(dicomfile, 0x20, 0x1041);
  if(e != NULL){
  fprintf(fp,"SliceLocation %s\n",e->d.string);
  FreeElementData(e); free(e);
  }

  return(0);
}
/*---------------------------------------------------------------*/
int DumpSiemensASCII(char *dicomfile, FILE *fpout)
{

  DCM_ELEMENT *e;
  FILE *fp;
  // declared at top of file: char tmpstr[TMPSTRLEN]; 
  int dumpline, nthchar;
  char *rt;
  char *BeginStr;
  int LenBeginStr;
  char *TestStr;
  int nTest;
  
  e = GetElementFromFile(dicomfile, 0x8, 0x70);
  if(e == NULL){
    printf("ERROR: reading dicom file %s\n",dicomfile);
    exit(1);
  }

  /* Siemens appears to add a space onto the end of their
     Manufacturer sting*/
  if(strcmp(e->d.string,"SIEMENS") != 0 && 
     strcmp(e->d.string,"SIEMENS ") != 0){ 
    printf("Not Siemens --%s--\n",e->d.string);
    FreeElementData(e); free(e);
    return(1);
  }
  FreeElementData(e); free(e);

  fp = fopen(dicomfile,"r");
  if(fp == NULL){
    printf("ERROR: could not open dicom file %s\n",dicomfile);
    exit(1);
  }

  BeginStr = "### ASCCONV BEGIN ###";
  LenBeginStr = strlen(BeginStr);
  TestStr = (char *) calloc(LenBeginStr+1,sizeof(char));

  /* This section steps through the file char-by-char until
     the BeginStr is matched */
  dumpline = 0;
  nthchar = 0;
  while(1){
    fseek(fp,nthchar, SEEK_SET);
    nTest = fread(TestStr,sizeof(char),LenBeginStr,fp);
    if(nTest != LenBeginStr) break;
    if(strcmp(TestStr,BeginStr)==0){
      //printf("Turning Dump On\n");
      fseek(fp,nthchar, SEEK_SET);
      dumpline = 1;
      break;
    }
    nthchar ++;
  }
  free(TestStr);

  /* No match found */
  if(! dumpline) {
    fprintf(fpout,"ERROR: this looks like a SIEMENS DICOM File,\n");
    fprintf(fpout,"       but I can't find the start of the ASCII Header\n");
    return(1);
  }


  /* Once the Begin String has been matched, this section 
     prints each line until the End String is matched */
  while(1){
    rt = fgets(tmpstr,TMPSTRLEN,fp);
    if(rt == NULL) break;

    if(strncmp(tmpstr,"### ASCCONV END ###",19)==0) {
      //printf("Turning Dump Off\n");
      break;
    }

    if(dumpline ) fprintf(fpout,"%s",tmpstr);
  }

  fclose(fp);

  return(0);
}


/*---------------------------------------------------------------*/
char *ElementValueFormat(DCM_ELEMENT *e)
{
  char * formatstring;

  switch(e->representation){

  case DCM_AE: 
  case DCM_AS: 
  case DCM_CS: 
  case DCM_DA: 
  case DCM_DS: 
  case DCM_DT: 
  case DCM_IS: 
  case DCM_LO: 
  case DCM_LT: 
  case DCM_OB: 
  case DCM_OW: 
  case DCM_PN: 
  case DCM_SH: 
  case DCM_ST: 
  case DCM_TM: 
  case DCM_UI: 
    formatstring = "%s";
    break;
  case DCM_SS: formatstring = "%d";  break;
  case DCM_SL: formatstring = "%ld"; break;
  case DCM_UL: formatstring = "%lu"; break;
  case DCM_US: formatstring = "%u";  break;
  case DCM_AT: formatstring = "%ld"; break;
  case DCM_FD: 
    fprintf(stderr,"ERROR: double type not available in DCM\n");
    exit(1);
    break;
  case DCM_FL: 
    fprintf(stderr,"ERROR: float type not available in DCM\n");
    exit(1);
    break;
  default: 
    fprintf(stderr,"ElementValueFormat: %d unrecognized",e->representation);
    return(NULL);
  }

  return(formatstring);
}
/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/
void init(void)
{    
   glClearColor (0.0, 0.0, 0.0, 0.0);
   glShadeModel(GL_FLAT);
   /*makeCheckImage();*/
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}
void display(void)
{
  static int first = 1;
  if(first) glClear(GL_COLOR_BUFFER_BIT);
  glRasterPos2i(0, 0);
  glDrawPixels(ImageWidth, ImageHeight, GL_RGB, 
         GL_UNSIGNED_BYTE, ImageBuff);
  glFlush();
  first = 0;
}
void reshape(int w, int h)
{
   glViewport(0, 0, (GLsizei) w, (GLsizei) h);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluOrtho2D(0.0, (GLdouble) w, 0.0, (GLdouble) h);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}

int RenderImage(int argc, char **argv){
  int nrows, ncols, row, col, n,m;
  char c;
  DCM_OBJECT *object;
  CONDITION cond;
  DCM_ELEMENT element;
  DCM_TAG tag;
  unsigned int rtnLength;
  void * Ctx = NULL;
  short * pixeldata;
  short minpixel, maxpixel;
  int nvoxs,nthvox;

  ncols = GetDimLength(dicomfile,0);
  nrows = GetDimLength(dicomfile,1);
  printf("nrows = %d, ncols = %d\n",nrows,ncols);
  nvoxs = nrows*ncols;

  /** Get pixel data **/
  object = GetObjectFromFile(dicomfile, 0);
  if(object == NULL) exit(1);

  tag=DCM_MAKETAG(0x7FE0,0x10);
  cond = DCM_GetElement(&object, tag, &element);
  if(cond != DCM_NORMAL){
    COND_DumpConditions();
    exit(1);
  }
  AllocElementData(&element);
  cond = DCM_GetElementValue(&object, &element, &rtnLength, &Ctx);
  if(cond != DCM_NORMAL){
    COND_DumpConditions();
    exit(1);
  }

  pixeldata = (short *) element.d.string;
  maxpixel = pixeldata[0];
  minpixel = pixeldata[0];
  for(n=0;n<nvoxs;n++){
    if(maxpixel < pixeldata[n]) maxpixel = pixeldata[n];
    if(minpixel > pixeldata[n]) minpixel = pixeldata[n];
  }
  printf("min = %d, max = %d\n",minpixel,maxpixel);

  ImageWidth = ncols;
  ImageHeight = nrows;
  ImageBuff = (GLubyte *) calloc((nrows)*(ncols)*3,sizeof(GLubyte));

  nthvox = 0;
  for(row=0; row < ImageHeight; row++){
    n = ImageHeight-row-1;
    for(col=0; col < ImageWidth; col++){
      c = 255*(pixeldata[nthvox]-minpixel)/(maxpixel-minpixel) + 1;
      m = col*3 + n*3*ImageWidth;
      ImageBuff[0 + m] = (GLubyte) c;
      ImageBuff[1 + m] = (GLubyte) c;
      ImageBuff[2 + m] = (GLubyte) c;
      nthvox ++;
    }
  }

  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
  glutInitWindowSize(ncols,nrows);
  glutInitWindowPosition(100, 100);
  sprintf(tmpstr,"mri_probedicom: %s",dicomfile);
  glutCreateWindow(tmpstr);
  init();
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutMainLoop();

  printf("Done Rendering\n");

  FreeElementData(&element);
  pixeldata = NULL;
  DCM_CloseObject(&object);

  return(0);
}



