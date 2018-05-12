#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <memory.h>
#include <conio.h>
#include <math.h>
#include <time.h>
#include <iostream>
#include <cmath>
#include <vector>

#if _DEBUG
#pragma comment(lib, "D:/Program Files/opencv/build/x86/vc14/lib/opencv_highgui2413d.lib")
#pragma comment(lib, "D:/Program Files/opencv/build/x86/vc14/lib/opencv_imgproc2413d.lib")
#pragma comment(lib, "D:/Program Files/opencv/build/x86/vc14/lib/opencv_core2413d.lib")
#else
#pragma comment(lib, "D:/Program Files/opencv/build/x86/vc14/lib/opencv_highgui2413.lib")
#pragma comment(lib, "D:/Program Files/opencv/build/x86/vc14/lib/opencv_imgproc2413.lib")
#pragma comment(lib, "D:/Program Files/opencv/build/x86/vc14/lib/opencv_core2413.lib")
#endif

#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

const int maxn = 2000000000;
const double topi = acos(-1.0)/180.0;	// pi/180
#define BOUND(x,min,max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define	nint(x)			(int)((x>0)?(x+0.5):(x-0.5))
#define	sqr(x)			((x)*(x))

struct point2d
{
	double x;
	double y;
};

struct point3d
{
    double x;
    double y;
    double z;
};

typedef double  MATRIX[3][3] ; 

#define	PNTS_PER_LINE		32
#define	LINES_PER_BLK		12
#define	PTNUM_PER_BLK		(32*12)
#define	BKNUM_PER_FRM		180
#define	SCANDATASIZE		(180*12)

#define	M_PI		3.1415926536

#define	INVALIDDOUBLE		99999999.9


typedef struct {
	float			x, y, z;
	u_char			i;
} point3fi;

typedef struct {
	int x, y;
} point2i;

typedef struct {
	long			millisec;
	point3fi		points[PTNUM_PER_BLK];
} ONEVDNDATA;

typedef struct {
	point3d			ang;
	point3d			shv;
	long			millisec;
	point3fi		points[PTNUM_PER_BLK];
	int				lab[PTNUM_PER_BLK];
} ONEDSVDATA;

typedef struct {
	ONEDSVDATA		dsv[BKNUM_PER_FRM];
	point3d			ang;
	point3d			shv;
	MATRIX			rot;
} ONEDSVFRAME;


typedef struct {
	int				wid;
	int				len;
	point3fi		*pts;
	int				*regionID;
	int				regnum;
	IplImage		*rMap;
	IplImage		*lMap;
	point3d			ang;
	point3d			shv;
} RMAP;

#define	INVAFLG			0
#define	LINEFLG			10
#define	NODEFLG			11
#define	SEGFLG			12
#define	ROADFLG			13
#define	HIGHFLG			14

#define	OBGLAB0			0
#define	OBGLAB1			1
#define	OBGLAB2			2
#define	OBGLAB3			3
#define	OBGLAB4			4
#define	OBGLAB5			5
#define	OBGLAB6			6
#define	OBGLAB7			7
#define OBJGROUND		100
#define	OBJBACKGROUND	101


#define UNKNOWN			0
#define NONVALID		-9999
#define GROUND			-999
#define	BACKGROUND		-99
#define EDGEPT			-9
#define	CHECKEDLABEL	-7

typedef struct {
	int				sno;
	int				milli;
	int				fno;
	int				rno;
	point3d			cp;
	point2i			ip;
	int				pnum;
	double			mind;
	int				prid;
	int				lab;
} ONESEED;

typedef struct {
	int				prid;
	int				sid;
	int				eid;
	int				sfn;
	int				efn;
	int				lab;
	int				snum;
} ONEPRID;

typedef struct {
	int				x1;
	int				y1;
	int				x2;
	int				y2;
	int				oldLab;
	int				newLab;
} ONEREC;

