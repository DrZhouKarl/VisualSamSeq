#include "VisualSamSeq.h"

HANDLE	dfp=INVALID_HANDLE_VALUE;
int		dsvlbytesiz = sizeof (ONEDSVDATA);
int		dsvbytesiz = dsvlbytesiz-sizeof(int)*LINES_PER_BLK*PNTS_PER_LINE;
int		dFrmNum=0;
int		dFrmNo=0;
int		sFrmNo=0;
int		eFrmNo=0;

ONEDSVFRAME	*onefrm;

ONESEED		*seeds = NULL;
int			seednum = 0;
ONEPRID		*prids = NULL;
int			pridnum = 0;
int			pridbufnum = 0;
int			seedidx = -1;

#define		PROCESSALL	27
int			processlab=0;

#define	SAMPLESIZE_X	256
#define	SAMPLESIZE_Y	128
#define	SAMPIXELSIZE	0.05	

int rmwid;
int rmlen;
IplImage * img = NULL; 
IplImage * lab = NULL; 
IplImage * outimg = NULL; 
IplImage * outlab = NULL; 

#define	MAXLABNUM	30
BYTE	col[MAXLABNUM][3];

int waitkeydelay=0;
int newLab = 0;

std::vector<ONEREC> recRecord[10000];

void ReadColorTable (char *szFile)
{
	memset (col, 0, MAXLABNUM*3);

	char	i_line[80];
    FILE	*fp;

	fp = fopen (szFile, "r");
	if (!fp) 
		return;

	int n=0;
	while (1) {
		if (fgets (i_line, 80, fp) == NULL)
			break;
		col[n][0] = atoi (strtok (i_line, ",\t\n"));
		col[n][1] = atoi (strtok (NULL, ",\t\n"));
		col[n][2] = atoi (strtok (NULL, ",\t\n"));
		n++;
		if (n>=MAXLABNUM)
			break;
	}
	fclose (fp);
}

//读取一帧vel32数据（一帧为180×12×32个激光点）保存到onefrm->dsv，未作坐标转换
BOOL ReadOneDsvlFrame ()
{
	DWORD	dwReadBytes;
	int		i;

	for (i=0; i<BKNUM_PER_FRM; i++) {
		if (!ReadFile( dfp, (ONEDSVDATA *)&onefrm->dsv[i], dsvlbytesiz, &dwReadBytes, NULL ) || (dsvlbytesiz != dwReadBytes))
			break;
	}

	if (i<BKNUM_PER_FRM)
		return FALSE;
	else
		return TRUE;
}

void JumpTo (int fno)
{
	LARGE_INTEGER li;

	li.QuadPart = __int64 (dsvlbytesiz)*BKNUM_PER_FRM*fno;
	li.LowPart = SetFilePointer( dfp, li.LowPart, &li.HighPart, FILE_BEGIN );
	dFrmNo = fno;
}

void ReleaseSegLog ()
{
	if (seeds)
		delete []seeds;
	if (prids)
		delete []prids;
}

int seed_compare_sno (const void *t1, const void *t2)
{
	ONESEED	*trj1, *trj2;

	trj1 = (ONESEED *)t1;
	trj2 = (ONESEED *)t2;

	if (trj1->sno>trj2->sno)
		return 1;
	else if (trj1->sno<trj2->sno)
		return -1;
	else
		return 0;
}

int seed_compare_fno (const void *t1, const void *t2)
{
	ONESEED	*trj1, *trj2;

	trj1 = (ONESEED *)t1;
	trj2 = (ONESEED *)t2;

	if (trj1->fno>trj2->fno)
		return 1;
	else if (trj1->fno<trj2->fno)
		return -1;
	else
		return 0;
}

void LoadSegLogFile (char *szFile)
{
	char			i_line[200];
    FILE			*segfp;

	int	*streamcnt = new int[MAXLABNUM];
	memset (streamcnt, 0, sizeof(int)*MAXLABNUM);
	int	*samplecnt = new int[MAXLABNUM];
	memset (samplecnt, 0, sizeof(int)*MAXLABNUM);

	segfp = fopen (szFile, "r");
	if (!segfp) 
		return;

	pridnum = 0;
	seednum = 0;
	while (1) {
		if (fgets (i_line, 200, segfp) == NULL)
			break;
		if (strnicmp (i_line, "prid", 4) == 0) {
			pridnum ++;
			continue;
		}
		if (strnicmp (i_line, "seed", 4) == 0) {
			seednum ++;
			continue;
		}
	}
	if (!seednum || !pridnum)
		return;

	fseek (segfp, 0L, SEEK_SET);
	seeds = new ONESEED[seednum];
	memset (seeds, 0, sizeof(ONESEED)*seednum);
	pridbufnum = pridnum*2;
	prids = new ONEPRID[pridbufnum];
	memset (prids, 0, sizeof (ONEPRID)*pridbufnum);

	int streamtotal=0, sampletotal=0;
	int sno=0, pno=0, fnum;
	char *str;
	while (1) {
		if (fgets (i_line, 200, segfp) == NULL)
			break;
		if (strnicmp (i_line, "prid", 4) == 0) {
			strtok (i_line, "= ,\t\n");
			prids[pno].prid = atoi (strtok (NULL, "= ,\t\n"));
			prids[pno].sfn = atoi (strtok (NULL, "= ,\t\n"));
			prids[pno].efn = atoi (strtok (NULL, "= ,\t\n"));
			prids[pno].snum = atoi (strtok (NULL, "= ,\t\n"));
			prids[pno].sid = sno;
			prids[pno].eid = sno+prids[pno].snum-1;
			str = strtok (NULL, "= ,\t\n");
			if (str) prids[pno].lab = atoi (str);
			streamcnt[prids[pno].lab]++;
			streamtotal++;
			if (++pno>=pridnum)
				break;
			continue;
		}
		if (strnicmp (i_line, "seed", 4) == 0) {
			strtok (i_line, "= ,\t\n");
			seeds[sno].sno = sno;
			seeds[sno].fno = atoi (strtok (NULL, "= ,\t\n"));
			seeds[sno].milli = atoi (strtok (NULL, "= ,\t\n"));
			seeds[sno].rno  = atoi (strtok (NULL, "= ,\t\n")); 
			seeds[sno].cp.x = atof (strtok (NULL, "= ,\t\n"));
			seeds[sno].cp.y = atof (strtok (NULL, "= ,\t\n"));
			seeds[sno].cp.z = atof (strtok (NULL, "= ,\t\n")); 
			seeds[sno].ip.x = atoi (strtok (NULL, "= ,\t\n"));
			seeds[sno].ip.y = atoi (strtok (NULL, "= ,\t\n"));
			seeds[sno].pnum = atoi (strtok (NULL, "= ,\t\n"));
			seeds[sno].mind = atof (strtok (NULL, "= ,\t\n"));
			if (pno>0) {
				seeds[sno].prid = prids[pno-1].prid;
				seeds[sno].lab = prids[pno-1].lab;
			}
			samplecnt[seeds[sno].lab]++;
			sampletotal++;
			if (++sno>=seednum)
				break;
			continue;
		}
	}
	fclose (segfp);
	seednum=sno;
	if (!seednum)
		return;
	qsort ((void *)seeds, seednum, sizeof(ONESEED), seed_compare_fno);
	sFrmNo=seeds[0].fno;
	eFrmNo=seeds[seednum-1].fno;
	for (int i=0; i<MAXLABNUM; i++) {
		if (!streamcnt[i]) continue;
		printf ("LABEL%d --- stream %d(%.2f%%) sample %d(%.2f%%)\n", i, streamcnt[i], streamcnt[i]/(double)streamtotal*100, samplecnt[i], samplecnt[i]/(double)sampletotal*100);
	}
	printf ("TOTAL --- stream %d sample %d\n", streamtotal, sampletotal);
}

//flg=true:add new labels
//flg=false:edit existing labels
void LabelAll (int x1, int y1, int x2, int y2, int lab, bool flg)
{
	vector <int> pridlist;
	pridlist.clear();
	int n, prid, sno, pno;
	bool cont;

	for (sno=0; sno<=seednum; sno++) {
		if (seeds[sno].fno<dFrmNo)
			continue;
		if (seeds[sno].fno>dFrmNo)
			break;
		if (flg) {
			if (seeds[sno].lab)
				continue;
		}
		else {
			if (!seeds[sno].lab)
				continue;
		}
		cont=true;
		for (int y=y1; y<=y2; y++) {
			for (int x=x1; x<=x2; x++) {
				int i=x/LINES_PER_BLK;
				int j=x%LINES_PER_BLK;
				int k=y;
				if (i<0||i>=BKNUM_PER_FRM||j<0||j>=LINES_PER_BLK||k<0||k>=PNTS_PER_LINE) 
					continue;
				if (onefrm->dsv[i].lab[j*PNTS_PER_LINE+k] == seeds[sno].prid) {
					prid = seeds[sno].prid;
					for (n=0; n<pridlist.size(); n++) {
						if (pridlist[n]==prid) 
							break;
					}
					if (n>=pridlist.size()) 
						pridlist.push_back(prid);
					cont=false;
					break;
				}
			}
			if (!cont) break;
		}
	}

	for (n=0; n<pridlist.size(); n++) {
		prid = pridlist[n];
		for (int pno=0; pno<pridnum; pno++) {
			if (prids[pno].prid == prid) {
				if (flg) {
					if (!prids[pno].lab)
						prids[pno].lab = lab;
				}
				else {
					if (prids[pno].lab)
						prids[pno].lab = lab;
				}
			}
		}
		for (int sno=0; sno<seednum; sno++) {
			if (seeds[sno].prid == prid) {
				if (flg) {
					if (!seeds[sno].lab)
						seeds[sno].lab = lab;
				}
				else {
					if (seeds[sno].lab)
						seeds[sno].lab = lab;
				}
			}
		}
	}		
}

void Replace(int x1, int y1, int x2, int y2, int newLab, int oldLab) {
	for (int y = y1; y <= y2; y++) {
		for (int x = x1; x <= x2; x++) {
			int i = x / LINES_PER_BLK;
			int j = x % LINES_PER_BLK;
			int k = y;
			if (i<0 || i >= BKNUM_PER_FRM || j<0 || j >= LINES_PER_BLK || k<0 || k >= PNTS_PER_LINE)
				continue;
			if (onefrm->dsv[i].lab[j*PNTS_PER_LINE + k] == oldLab) {
				onefrm->dsv[i].lab[j*PNTS_PER_LINE + k] = -newLab;
			}
		}
	}
}

void UpdateBitmaps ()
{
	point3fi	*p;
	double rng;
	int i, j, k, x, y, n;

	// Update REC fix of label
	for (i = 0; i < recRecord[dFrmNo].size(); i++) {
		int x1 = recRecord[dFrmNo][i].x1;
		int x2 = recRecord[dFrmNo][i].x2;
		int y1 = recRecord[dFrmNo][i].y1;
		int y2 = recRecord[dFrmNo][i].y2;
		int _newlab = recRecord[dFrmNo][i].newLab;
		int _oldlab = recRecord[dFrmNo][i].oldLab;
		Replace(min(x1, x2) * 2, nint(min(y1, y2) / 4.5), max(x1, x2) * 2, nint(max(y1, y2) / 4.5), _newlab, _oldlab);
	}

	//生成距离图像及相应的数据、宽180×12、高32
	cvZero (img);
	cvZero (lab);
	for (i=0; i<BKNUM_PER_FRM; i++) {
		for (j=0; j<LINES_PER_BLK; j++) {
			for (k=0; k<PNTS_PER_LINE; k++) {
				p = &onefrm->dsv[i].points[j*PNTS_PER_LINE+k];
				if (!p->i)
					continue;
				rng=sqrt(sqr(p->x)+sqr(p->y)+sqr(p->z));
				x=i*LINES_PER_BLK+j;
				y=k;
				if (onefrm->dsv[i].lab[j*PNTS_PER_LINE+k]==GROUND) {
					lab->imageData[(y*rmwid+x)*3] = 0;
					lab->imageData[(y*rmwid+x)*3+1] = 128;
					lab->imageData[(y*rmwid+x)*3+2] = 0;
				}
				else if (onefrm->dsv[i].lab[j*PNTS_PER_LINE+k]<0) {
					n = -onefrm->dsv[i].lab[j*PNTS_PER_LINE+k];
					if (n>0 && n<MAXLABNUM) {
						lab->imageData[(y*rmwid+x)*3] = col[n][2];
						lab->imageData[(y*rmwid+x)*3+1] = col[n][1];
						lab->imageData[(y*rmwid+x)*3+2] = col[n][0];;
					}
				}
				else {
					img->imageData[(y*rmwid+x)*3] = 
					img->imageData[(y*rmwid+x)*3+1] = 
					img->imageData[(y*rmwid+x)*3+2] = min(255,int(rng*10));
					lab->imageData[(y*rmwid+x)*3] = 
					lab->imageData[(y*rmwid+x)*3+1] = 
					lab->imageData[(y*rmwid+x)*3+2] = min(255,int(rng*10));
				}
			}
		}
	}		
	for (; seedidx<=seednum; seedidx++) {
		if (seeds[seedidx].fno<dFrmNo)
			continue;
		if (seeds[seedidx].fno>dFrmNo)
			break;
		n=seeds[seedidx].lab;
		if (n<0)
			continue;
		if (!col[n][0] && !col[n][1] && !col[n][2])
			continue;
		for (y=0; y<rmlen; y++) {
			for (x=0; x<rmwid; x++) {
				i=x/LINES_PER_BLK;
				j=x%LINES_PER_BLK;
				k=y;
				if (onefrm->dsv[i].lab[j*PNTS_PER_LINE+k] == seeds[seedidx].prid) {
					if (!n) {
						img->imageData[(y*rmwid+x)*3] = col[n][2];
						img->imageData[(y*rmwid+x)*3+1] = col[n][1];
						img->imageData[(y*rmwid+x)*3+2] = col[n][0];
					}
					else {
						lab->imageData[(y*rmwid+x)*3] = col[n][2];
						lab->imageData[(y*rmwid+x)*3+1] = col[n][1];
						lab->imageData[(y*rmwid+x)*3+2] = col[n][0];
					}
				}
			}
		}
	}
	
}

void CallbackLabel(int event, int x, int y, int flags, void *ustc)
{
	if (waitkeydelay) 
		return;

	static CvPoint lu, rb;

	if (event == CV_EVENT_LBUTTONDOWN) {
		lu.x = x; lu.y = y;
		cvResize (img, outimg);
	}
	if (event == CV_EVENT_LBUTTONUP) {
		rb.x = x; rb.y = y;
		cvRectangle (outimg, lu, rb, cvScalar(255, 0, 0), 3);
		cvShowImage("image", outimg);

		printf("label = ");
		char LabelKey;
		int nlab;
		while (1) {
			LabelKey=cvWaitKey(0);
			if (LabelKey == 'q'|| LabelKey == 27) {
				break;
			}
			else if (LabelKey == 'u')
				nlab = -1;
			else if (LabelKey >= '0' && LabelKey <= '9')
				nlab = LabelKey - '0';
			else if (LabelKey >= 'a' && LabelKey <= 'o')
				nlab = LabelKey - 'a'+10;
			else
				continue;
			LabelAll (min(lu.x,rb.x)*2,nint(min(lu.y,rb.y)/4.5),max(lu.x,rb.x)*2,nint(max(lu.y,rb.y)/4.5),nlab, true);
			seedidx = 0;
			UpdateBitmaps ();
			cvResize (img, outimg);
			cvShowImage("image",outimg);
			cvResize (lab, outlab);
			cvShowImage("label",outlab);
			break;
		}
		printf("%c\n", LabelKey);
	}
}

void CallbackEdit(int event, int x, int y, int flags, void *ustc)
{
	if (waitkeydelay)
		return;

	static CvPoint lu, rb;

	if (event == CV_EVENT_LBUTTONDOWN) {
		lu.x = x; lu.y = y;
		cvResize (lab, outlab);
	}
	if (event == CV_EVENT_LBUTTONUP) {
		rb.x = x; rb.y = y;
		cvRectangle (outlab, lu, rb, cvScalar(255, 0, 0), 3);
		cvShowImage("label", outlab);
	}
	if (event == CV_EVENT_RBUTTONDOWN) {
		int xx = x * 2;
		int yy = nint(y / 4.5);
		int i = xx / LINES_PER_BLK;
		int j = xx % LINES_PER_BLK;
		int k = yy;
		if (i < 0 || i >= BKNUM_PER_FRM || j < 0 || j >= LINES_PER_BLK || k < 0 || k >= PNTS_PER_LINE)
			return;
		int oldLab = onefrm->dsv[i].lab[j*PNTS_PER_LINE + k];
		printf("Choose label: %d\n", oldLab);

		ONEREC tmp;
		tmp.x1 = lu.x; tmp.x2 = rb.x;
		tmp.y1 = lu.y; tmp.y2 = rb.y;
		tmp.oldLab = oldLab; tmp.newLab = newLab;
		recRecord[dFrmNo].push_back(tmp);

		seedidx = 0;
		UpdateBitmaps();
		cvResize(img, outimg);
		CvFont font;
		cvInitFont(&font, CV_FONT_HERSHEY_DUPLEX, 1, 1, 0, 2);
		char str[10];
		sprintf(str, "Fno%d", dFrmNo);
		cvPutText(outimg, str, cvPoint(50, 50), &font, CV_RGB(0, 0, 255));
		cvShowImage("image", outimg);
		cvResize(lab, outlab);
		cvShowImage("label", outlab);
	}
}

void VisualizeSampleStream ()
{
	rmwid = LINES_PER_BLK*BKNUM_PER_FRM;
	rmlen = PNTS_PER_LINE;
	img = cvCreateImage (cvSize (rmwid, rmlen),IPL_DEPTH_8U,3); 
	lab = cvCreateImage (cvSize (rmwid, rmlen),IPL_DEPTH_8U,3); 
	outimg = cvCreateImage (cvSize (rmwid/2, rmlen*4.5),IPL_DEPTH_8U,3); 
	outlab = cvCreateImage (cvSize (rmwid/2, rmlen*4.5),IPL_DEPTH_8U,3); 

	CvFont font;
	cvInitFont(&font,CV_FONT_HERSHEY_DUPLEX, 1,1, 0, 2);

	dFrmNo = max(1,sFrmNo);
	JumpTo (dFrmNo);

	char WaitKey=0;
	seedidx = 0;
	while (ReadOneDsvlFrame ())
	{
		UpdateBitmaps ();
		//cv::setMouseCallback("image", CallbackLabel, 0);
		cv::setMouseCallback("label", CallbackEdit, 0);
		cvResize (img, outimg);
		char str[10];
		sprintf (str, "Fno%d", dFrmNo);
		cvPutText(outimg, str,cvPoint(50,50),&font,CV_RGB(0,0,255));
		cvShowImage("image",outimg);
		cvResize (lab, outlab);
		cvShowImage("label",outlab);

gotoFlag1:
		WaitKey = cvWaitKey(waitkeydelay);
		printf("%d\n", WaitKey);
		dFrmNo++;

		if (dFrmNo>=eFrmNo) 
			break;

		if (WaitKey==27)
			break;
		if (WaitKey=='z') {
			if (waitkeydelay==1)
				waitkeydelay=0;
			else
				waitkeydelay=1;
		}
		else if (WaitKey=='p') {
			if ((dFrmNo-2)>0) 
				dFrmNo-=2;
			else
				dFrmNo--;
			JumpTo (dFrmNo);
			seedidx = 0;
			waitkeydelay=0;
		}
		else if (WaitKey == 'r') {
			dFrmNo--;
			seedidx = 0;
			recRecord[dFrmNo].clear();
			JumpTo (dFrmNo);
			printf("Frame restored!\n");
			continue;
		}
		else if (WaitKey >= '0' && WaitKey <= '9') {
			newLab = WaitKey - '0';
			dFrmNo--;
			printf("new Label: %d\n", newLab);
			goto gotoFlag1;
		}
		else if (WaitKey >= 'a' && WaitKey <= 'o') {
			newLab = WaitKey - 'a' + 10;
			dFrmNo--;
			printf("new Label: %c\n", WaitKey);
			goto gotoFlag1;
		}

	}
	cvReleaseImage(&img);
	cvReleaseImage(&lab);
	cvReleaseImage(&outimg);
	cvReleaseImage(&outlab);

}

void OutputSegLogFile (char *szFile)
{
	char			i_line[200];
    FILE			*segfp;

	if (!seednum)
		return;
	qsort ((void *)seeds, seednum, sizeof(ONESEED), seed_compare_sno);

	printf("output seglog to %s ...\n", szFile);

	segfp = fopen (szFile, "w");
	if (!segfp) 
		return;

	for (int i=0; i<pridnum; i++) {
		if (prids[i].lab<0)
			continue;

		fprintf (segfp, "prid=%d,%d,%d,%d,%d\n", 
			prids[i].prid, prids[i].sfn, prids[i].efn, prids[i].eid-prids[i].sid+1, prids[i].lab); 

		for (int j=prids[i].sid; j<=prids[i].eid; j++) {
			fprintf (segfp, "seed=%d,%d,%d,%.3f,%.3f,%.3f,%d,%d,%d,%.3f\n", 
				seeds[j].fno, seeds[j].milli, seeds[j].rno, 
				seeds[j].cp.x, seeds[j].cp.y, seeds[j].cp.z, 
				seeds[j].ip.x, seeds[j].ip.y, seeds[j].pnum, seeds[j].mind);
		}
	}

	fclose (segfp);

}

//输出结果到dsvl文件，在DsvViewer中可视化
void Write2LabelFile (char *szFile) 
{
	HANDLE	odfp=INVALID_HANDLE_VALUE;

	odfp = CreateFile( szFile,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL );
	if( odfp == INVALID_HANDLE_VALUE ) {
		printf("File open failure : %s\n", szFile);
		return;
	}

	printf("output dsv + labels to %s ...\n", szFile);
	
	DWORD dwSizeLow,dwSizeHigh,dwError;  //低位，高位，错误代码
	dwSizeLow = GetFileSize((HANDLE)dfp, &dwSizeHigh);  //就是他了
	if (dwSizeLow == 0xFFFFFFFF && (dwError = GetLastError()) != NO_ERROR ){ 
		return;
	} 
	else {
		LONGLONG llSize,llPow;
		llPow = 4294967296; //(LONGLONG)pow(2,32);
		llSize = dwSizeHigh*llPow+dwSizeLow; //可能是大于4G的怪物
		dFrmNum = llSize/180/dsvlbytesiz;
	}

	SetFilePointer( dfp, 0, 0, FILE_BEGIN );
	dFrmNo = 0;

	ONEDSVFRAME *outfrm= new ONEDSVFRAME[1];

	while (ReadOneDsvlFrame ()) {

		memcpy (outfrm, onefrm, sizeof(ONEDSVFRAME));
		int i, j, k, sno;
		for (i=0; i<BKNUM_PER_FRM; i++) 
			memset (outfrm->dsv[i].lab, 0, sizeof (int)*PTNUM_PER_BLK);
		
		// Update REC fix of label
		for (i = 0; i < recRecord[dFrmNo].size(); i++) {
			int x1 = recRecord[dFrmNo][i].x1;
			int x2 = recRecord[dFrmNo][i].x2;
			int y1 = recRecord[dFrmNo][i].y1;
			int y2 = recRecord[dFrmNo][i].y2;
			int _newlab = recRecord[dFrmNo][i].newLab;
			int _oldlab = recRecord[dFrmNo][i].oldLab;
			Replace(min(x1, x2) * 2, nint(min(y1, y2) / 4.5), max(x1, x2) * 2, nint(max(y1, y2) / 4.5), _newlab, _oldlab);
		}

		for (i=0; i<BKNUM_PER_FRM; i++) {
			for (j=0; j<LINES_PER_BLK; j++) {
				for (k=0; k<PNTS_PER_LINE; k++) {
					outfrm->dsv[i].lab[j*PNTS_PER_LINE + k] = onefrm->dsv[i].lab[j*PNTS_PER_LINE + k];
					/*if (onefrm->dsv[i].lab[j*PNTS_PER_LINE+k]==GROUND || onefrm->dsv[i].lab[j*PNTS_PER_LINE+k]==BACKGROUND) 
						outfrm->dsv[i].lab[j*PNTS_PER_LINE+k] = onefrm->dsv[i].lab[j*PNTS_PER_LINE+k];
					else if (onefrm->dsv[i].lab[j*PNTS_PER_LINE+k]<0)
						outfrm->dsv[i].lab[j*PNTS_PER_LINE+k] = -onefrm->dsv[i].lab[j*PNTS_PER_LINE+k];*/
				}
			}
		}

		for (sno=0; sno<=seednum; sno++) {
			if (seeds[sno].fno<dFrmNo)
				continue;
			if (seeds[sno].fno>dFrmNo)
				break;
			for (i=0; i<BKNUM_PER_FRM; i++) {
				for (j=0; j<LINES_PER_BLK; j++) {
					for (k=0; k<PNTS_PER_LINE; k++) {
						point3fi	*p;
						p = &onefrm->dsv[i].points[j*PNTS_PER_LINE+k];
						if (!p->i)
							continue;
						if (onefrm->dsv[i].lab[j*PNTS_PER_LINE+k] == seeds[sno].prid) 
							outfrm->dsv[i].lab[j*PNTS_PER_LINE+k] = seeds[sno].lab;
					}
				}
			}
		}

		DWORD	dwBytes;
		for (int i=0; i<BKNUM_PER_FRM; i++) {
			WriteFile( odfp, (char *)&outfrm->dsv[i], dsvlbytesiz, &dwBytes, NULL );
		}
		if (!(dFrmNo%100))
			printf("%d(%d)\n", dFrmNo, dFrmNum);
		dFrmNo++;
	}
	CloseHandle( odfp );
	delete []outfrm;
}

void main (int argc, char *argv[])
{
	if (argc<4) {
		fprintf (stderr,"Usage : %s [dsvlfile] [logfile] [colortable] [outputlogfile] [odsvlfile]\n", argv[0]);
		fprintf (stderr,"Example : %s dsvlfile logfile colortable\n", argv[0]);
		fprintf (stderr,"Example : %s dsvlfile logfile colortable outputlogfile\n", argv[0]);
		fprintf (stderr,"Example : %s dsvlfile logfile colortable outputlogfile odsvlfile\n", argv[0]);
		fprintf (stderr,"Example : %s dsvlfile logfile colortable NULL odsvlfile\n", argv[0]);
		fprintf (stderr,"Example : %s dsvlfile logfile colortable NULL NULL\n", argv[0]);
		exit(1);
	}

 	dfp = CreateFile( argv[1],GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL );
	if( dfp == INVALID_HANDLE_VALUE ) {
		printf("File open failure : %s\n", argv[1]);
		getchar ();
		exit (1);
	}
	onefrm= new ONEDSVFRAME[1];

	ReadColorTable (argv[3]);
	LoadSegLogFile (argv[2]);
	VisualizeSampleStream ();

	if (argc>4) {
		if (strnicmp (argv[4], "NULL", 4) != 0)
			OutputSegLogFile (argv[4]);
	}
	if (argc>5) {
		if (strnicmp (argv[5], "NULL", 4) != 0)
			Write2LabelFile (argv[5]);
	}

	delete []onefrm;
	CloseHandle( dfp );
	ReleaseSegLog ();
	exit (1);
}

