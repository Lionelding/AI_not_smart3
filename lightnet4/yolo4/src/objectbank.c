#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "objectbank.h"



Opticalflow drawOptFlowMap(CvMat* flow, CvMat *cflowmap, int step, double scale, CvScalar color) {

	int x, y;
	(void) scale;
	int tr=(cflowmap->rows/step)+1;
	int tc=(cflowmap->cols/step)+1;
	double degreeStore[tr*tc];
	double xStore[tr*tc];
	double yStore[tr*tc];
	int i=0;

	for(y = 0; y < cflowmap->rows; y= step+y){
		for(x = 0; x < cflowmap->cols; x=step+x){

			CvPoint2D32f fxy = CV_MAT_ELEM(*flow, CvPoint2D32f, y, x);
			CvPoint start=cvPoint(x, y);
			CvPoint end=cvPoint(cvRound(x+fxy.x), cvRound(y+fxy.y));

            cvLine(cflowmap, cvPoint(x,y), cvPoint(cvRound(x+fxy.x), cvRound(y+fxy.y)),color, 1, 8, 0);
            cvCircle(cflowmap, cvPoint(x,y), 2, color, -1, 8, 0);

            float degree=computeDegree(start.x, start.y, end.x, end.y);
            int magnitude=computeMagnitude(start.x, start.y, end.x, end.y);
            degreeStore[i]=degree+0.01*magnitude;
            xStore[i]=end.x-start.x;
            yStore[i]=end.y-start.y;

            i=i+1;

		}
	}

	uchar data[12] = {0,0,255,0,0,255,0,0,255,0,0,255};
	CvMat src = cvMat(1,12, CV_8UC1, data);
	IplImage* imgA = cvCreateImageHeader(cvSize(src.cols,src.rows), 8, 1);
	cvGetImage (& src, imgA);

	//IplImage* imgA;
	//imgA = cvCreateImage(cvSize(src.cols,src.rows),8,1);
//	IplImage* ipltemp=src;
//	cvCopy(&ipltemp,imgA);


	//IplImage tmp;
	//IplImage* imgA = cvGetImage((CvArr*) src, &tmp);

	//IplImage *imgA= cvCreateImage(cvSize(1, i-1),IPL_DEPTH_8U,1);
	//imgA=xStore;
	//IplImage ipltemp=src;
	//cvCopy(&ipltemp,imgA);


	CvHistogram *hist_red;
    IplImage *hist_img = cvCreateImage(cvSize(300,240), 8, 3);
    cvSet( hist_img, cvScalarAll(255), 0 );
	int hist_size = 256;
    float range[]={0,256};
    float* ranges[] = { range };

	hist_red = cvCreateHist(1, &hist_size, CV_HIST_ARRAY, ranges, 1);
	cvCalcHist( &imgA, hist_red, 0, NULL );
	float max_value = 0.0;
	float max = 0.0;
	float w_scale = 0.0;
	cvGetMinMaxHistValue( hist_red, 0, &max_value, 0, 0 );
	max_value = (max > max_value) ? max : max_value;

	cvScale( hist_red->bins, hist_red->bins, ((float)hist_img->height)/max_value, 0 );
	w_scale = ((float)hist_img->width)/hist_size;
	int iii;
    for(iii = 0; iii < hist_size; iii++ )
    {
      cvRectangle( hist_img, cvPoint((int)iii*w_scale , hist_img->height),
        cvPoint((int)(iii+1)*w_scale, hist_img->height - cvRound(cvGetReal1D(hist_red->bins,iii))),
        CV_RGB(255,0,0), -1, 8, 0 );
    }

    /* create a window to show the histogram of the image */
    cvNamedWindow("Histogram", 1);
    cvShowImage( "Histogram", hist_img);

    cvWaitKey(0);


	//printf("cflowmap->row: %i, cflowmap->col: %i, tr: %i, tc: %i, i: %i\n", cflowmap->rows, cflowmap->cols, tr, tc, i);
	quickSort(degreeStore, 0, i-1);
	//printArray(degreeStore, i);

    int medianOfMedian=i*0.75;

    int s;
    int sScope=3;
    int mergedDegree=0;
    int mergedMagnitude=0;
    for(s=(-sScope/2);s<(sScope/2+1);s++){

    	double degreeStoreElement=degreeStore[medianOfMedian+s];
    	int medianMagnitude=extractIndexFromFloat(degreeStoreElement);
    	int medianDegree=(int)degreeStoreElement;
    	printf("medianDegree: %i, medianMagnitude: %i\n", medianDegree, medianMagnitude);
    	if(s==(-sScope/2)){
    		//if not, the first degree will be affected by degree of 0
    		mergedDegree=medianDegree;
    		mergedMagnitude=medianMagnitude;
    	}

    	mergedDegree=addDegree(mergedDegree, medianDegree);
    	mergedMagnitude=addMagnitude(mergedMagnitude, medianMagnitude);

    }

    printf("mergedDegree: %i, mergdeMagnitude: %i\n", mergedDegree, mergedMagnitude);
    Opticalflow medianflow=create_opticalflowFB(mergedDegree, mergedMagnitude);



	return medianflow;


}

Opticalflow compute_opticalflowFB(IplImage *previous, IplImage *current){
    //Convert the input from RGB to Grayscale
    IplImage *imgA= cvCreateImage(cvGetSize(previous),IPL_DEPTH_8U,1);
    cvCvtColor(previous,imgA,CV_RGB2GRAY);

    CvSize s=cvGetSize(current);
    int height=s.height;
    int width=s.width;

    CvMat* gray = cvCreateMat(height, width, CV_8UC1);
    CvMat* prevgray = cvCreateMat(height, width, gray->type);

    cvCvtColor(current, gray, CV_BGR2GRAY);
    cvCvtColor(previous, prevgray, CV_BGR2GRAY);

    CvMat* flow = cvCreateMat(height, width, CV_32FC2);
    CvMat* cflow = cvCreateMat(height, width, CV_8UC3);

    cvCalcOpticalFlowFarneback(prevgray, gray, flow, 0.5, 3, 15, 3, 5, 1.2, 0);

    cvCvtColor(imgA, cflow, CV_GRAY2BGR);
    Opticalflow medianflow=drawOptFlowMap(flow, cflow, 16, 1.5, CV_RGB(0, 255, 0));
    //cvShowImage("OpticalFlowFarneback", cflow);
    //cvWaitKey(0);


	cvReleaseImage(&imgA);
	cvReleaseMat(&gray);
	cvReleaseMat(&prevgray);
	cvReleaseMat(&flow);
	cvReleaseMat(&cflow);

    return medianflow;
}

Opticalflow compute_opticalflow(IplImage *previous, IplImage *current, int xoff, int yoff){

	/* Read the video's frame size out of the AVI. */
    int MAX_CORNERS=50;
    int averageFlow=0;	//merge the optical flow vector based on the average value
    int medianFlow=1;	//pick up the median number of optical flow vector

    //Convert the input from RGB to Grayscale
    IplImage *imgA= cvCreateImage(cvGetSize(previous),IPL_DEPTH_8U,1);
    cvCvtColor(previous,imgA,CV_RGB2GRAY);

    IplImage *imgB= cvCreateImage(cvGetSize(current),IPL_DEPTH_8U,1);
    cvCvtColor(current,imgB,CV_RGB2GRAY);

    CvSize img_sz=cvGetSize(imgA);
    int win_size=3;

    //the first thing we need to do is to get the features we want to track
    IplImage* eig_image=cvCreateImage(img_sz,IPL_DEPTH_32F,1);
    IplImage* tmp_image=cvCreateImage(img_sz,IPL_DEPTH_32F,1);

    int corner_count=MAX_CORNERS;
    CvPoint2D32f cornersA[MAX_CORNERS];
    cvGoodFeaturesToTrack(imgA,eig_image,tmp_image,cornersA,&corner_count,0.01,5.0,0,3,0,0.04);

    cvFindCornerSubPix(imgA,cornersA,corner_count,cvSize(win_size,win_size),cvSize(-1,-1),cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS,20,0.03));
    //call Lucas Kanade algorithm
    char feature_found[MAX_CORNERS];
    float feature_errors[MAX_CORNERS];

    CvSize pyr_sz=cvSize(imgA->width +8,imgB->height /3);
    IplImage* pyrA=cvCreateImage(pyr_sz,IPL_DEPTH_32F,1);
    IplImage* pyrB=cvCreateImage(pyr_sz,IPL_DEPTH_32F,1);
    CvPoint2D32f cornersB[MAX_CORNERS];

    cvCalcOpticalFlowPyrLK(imgA,imgB,pyrA,pyrB,cornersA,cornersB,corner_count,cvSize(win_size,win_size),5,feature_found,feature_errors,cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS,20,0.3),0);



    int i;
    int sum_Ax=0;
    int sum_Ay=0;
    int sum_Bx=0;
    int sum_By=0;
    int sum_count=0;

    float degreeStore[MAX_CORNERS];
    int arrayEnd=0;
    for(i=0;i<MAX_CORNERS;++i){

      	if(feature_found[i]==0 || fabsf(feature_errors[i])>550 || fabsl(cornersA[i].x)>1200 || fabsl(cornersA[i].y)>1200 || fabsl(cornersB[i].x)>1200 || fabsl(cornersB[i].y)>1200 || isnan(cornersA[i].x) || isnan(cornersB[i].x) || isnan(cornersA[i].y) || isnan(cornersB[i].y)){
        //if(feature_found[i]==0 || fabsf(feature_errors[i])>550 || (cornersA[i].x)>1200 || (cornersA[i].x)<-1200 || fabsl(cornersA[i].y>1200) || fabsl(cornersB[i].x>1200) || fabsl(cornersB[i].y>1200) || isnan(cornersA[i].x) || isnan(cornersB[i].x) || isnan(cornersA[i].y) || isnan(cornersB[i].y)){

      		//.||cornersB[i].x<-1200 || cornersB[i].y<-1200
//      	printf("Error is %f\n",feature_errors[i]);
//    		printf("cornersA[i].x: %0.1f, %i\n", cornersA[i].x, isnan(cornersA[i].x));
//    		printf("cornersA[i].y: %0.1f, %i\n", cornersA[i].y, isnan(cornersA[i].y));
//    		printf("cornersB[i].x: %0.1f, %i\n", cornersB[i].x, isnan(cornersB[i].x));
//    		printf("cornersB[i].y: %0.1f, %i\n", cornersB[i].y, isnan(cornersB[i].y));
    		continue;
    	}



      	if(medianFlow){
        	float degree=(1.0*computeDegree(cvRound(cornersA[i].x), cvRound(cornersA[i].y), cvRound(cornersB[i].x), cvRound(cornersB[i].y)));
        	//printf("index: %i, degree: %0.1f\n", i, degree);
        	degreeStore[arrayEnd]=degree+i*0.01;
        	arrayEnd++;
      	}

      	if(averageFlow){
         	sum_Ax=cvRound(cornersA[i].x)+sum_Ax;
         	sum_Ay=cvRound(cornersA[i].y)+sum_Ay;
        	sum_Bx=cvRound(cornersB[i].x)+sum_Bx;
        	sum_By=cvRound(cornersB[i].y)+sum_By;
        	sum_count=sum_count+1;

      	}

    	//cvLine(imgB,p0,p1,CV_RGB(255,0,0),2,8,0);
    	//drawArrow(imgB, p0, p1, CV_RGB(255, 0, 0), 9, 1, 8, 0);

    }


    Opticalflow median;
    if(medianFlow){

    	//THESIS: only save the element with a smaller error, throw away with the one with high error
    	//In the arrayDegree, the first partition includes valid elements with correct degree value
    	//The second partition include the element that was not randomly initialized (Don't care)
    	//We only sort the first partition and take the median number of it.

        quickSort(degreeStore, 0, arrayEnd);

        int medianOfMedian=arrayEnd*0.75;

        float degreeStoreElement=degreeStore[medianOfMedian];
        int medianIndex=extractIndexFromFloat(degreeStoreElement);

        printf("\nMedian degree: %i, at index: %i\n", (int)degreeStoreElement, medianIndex);
        printArray(degreeStore, MAX_CORNERS-1);

        int median_sum_Ax=0;
        int median_sum_Ay=0;
        int median_sum_Bx=0;
        int median_sum_By=0;

        int sScope=3;

        int s;
        for(s=(-sScope/2);s<(sScope/2+1);s++){


        	float sIndex=degreeStore[medianOfMedian+s];
        	int sIndex2=extractIndexFromFloat(sIndex);
        	printf("sIndex2: %i ", sIndex2);

        	median_sum_Ax=cvRound(cornersA[sIndex2].x)+median_sum_Ax;
        	median_sum_Ay=cvRound(cornersA[sIndex2].y)+median_sum_Ay;
        	median_sum_Bx=cvRound(cornersB[sIndex2].x)+median_sum_Bx;
        	median_sum_By=cvRound(cornersB[sIndex2].y)+median_sum_By;

        }
        printf("\n");


    	CvPoint median_sum_p0=cvPoint(cvRound(median_sum_Ax/sScope),cvRound(median_sum_Ay/sScope));
    	CvPoint median_sum_p1=cvPoint(cvRound(median_sum_Bx/sScope),cvRound(median_sum_By/sScope));


    	int abs_0x=median_sum_p0.x+xoff;
    	int abs_0y=median_sum_p0.y+yoff;

    	int abs_1x=median_sum_p1.x+xoff;
    	int abs_1y=median_sum_p1.y+yoff;

    	CvPoint abs_p0=cvPoint(abs_0x,abs_0y);
    	CvPoint abs_p1=cvPoint(abs_1x,abs_1y);
    	median=create_opticalflow(median_sum_p0, median_sum_p1, abs_p0, abs_p1);

    }


    Opticalflow average;
    if(averageFlow){

    	CvPoint sum_p0=cvPoint(cvRound(sum_Ax/sum_count),cvRound(sum_Ay/sum_count));
    	CvPoint sum_p1=cvPoint(cvRound(sum_Bx/sum_count),cvRound(sum_By/sum_count));
    	//drawArrow(imgB, sum_p0, sum_p1, CV_RGB(0, 255, 255), 9, 1, 8, 0);

    	int abs_0x;
    	int abs_0y;
    	int abs_1x;
    	int abs_1y;
    	abs_0x=sum_p0.x+xoff;
    	abs_0y=sum_p0.y+yoff;

    	abs_1x=sum_p1.x+xoff;
    	abs_1y=sum_p1.y+yoff;

    	CvPoint abs_p0=cvPoint(abs_0x,abs_0y);
    	CvPoint abs_p1=cvPoint(abs_1x,abs_1y);

    	average=create_opticalflow(sum_p0, sum_p1, abs_p0, abs_p1);
    }



	cvReleaseImage(&imgA);
	cvReleaseImage(&imgB);
	cvReleaseImage(&eig_image);
	cvReleaseImage(&tmp_image);
	cvReleaseImage(&pyrA);
	cvReleaseImage(&pyrB);
	return median;
}

Opticalflow updateFlow(Opticalflow average_result, int preFlow, float bias){

	int upperLimit;
	int lowerLimit;
	int nowFlow=average_result.degree;
	float newFlow;

	if(preFlow<180){
		upperLimit=preFlow+180;
		if (nowFlow<upperLimit){
			newFlow=preFlow*bias+nowFlow*(1-bias);

		}

		else{
			newFlow=preFlow*bias+(nowFlow-360)*(1-bias);
			newFlow=newFlow<0?(newFlow+360):newFlow;

		}

	}

	else{
		lowerLimit=preFlow-180;
		if(nowFlow>lowerLimit){
			newFlow=preFlow*bias+nowFlow*(1-bias);
		}
		else{
			newFlow=(preFlow-360)*bias+nowFlow*(1-bias);
			newFlow=newFlow<0?(newFlow+360):newFlow;
		}

	}
	average_result.degree=newFlow;
	return average_result;

}

//ADDED: create optical flow vector
Opticalflow create_opticalflow(CvPoint sum_p0, CvPoint sum_p1, CvPoint abs_p0, CvPoint abs_p1)
{
	Opticalflow out;
    out.sum_p0 = sum_p0;
    out.sum_p1 = sum_p1;
    out.abs_p0 = abs_p0;
    out.abs_p1 = abs_p1;

    double sum_p0_x=(double)sum_p0.x;
    double sum_p0_y=(double)sum_p0.y;
    double sum_p1_x=(double)sum_p1.x;
    double sum_p1_y=(double)sum_p1.y;


    out.degree=computeDegree(sum_p0_x, sum_p0_y, sum_p1_x, sum_p1_y);
    out.magnitude=computeMagnitude(sum_p0_x, sum_p0_y, sum_p1_x, sum_p1_y);


    return out;
}

Opticalflow create_opticalflowFB(int degree, int magnitude)
{
	Opticalflow out;
	//Opticalflow* out = (Opticalflow*)malloc(sizeof(Opticalflow));

    out.degree=(double)degree;
    out.magnitude=(double)magnitude;

    return out;
}

int addDegree(int degree1, int degree2){
	float pi=3.1415926;
	if(degree1>180){
		degree1=-(360-degree1);

	}
	if(degree2>180){
		degree2=-(360-degree2);
	}


	float x1=1*cos(degree1*(pi/180));
	float y1=1*sin(degree1*(pi/180));

	float x2=1*cos(degree2*(pi/180));
	float y2=1*sin(degree2*(pi/180));

	float sumx=(x1+x2)/2;
	float sumy=(y1+y2)/2;


	if(fabs(sumx)<0.00000001 || fabs(sumy)<0.00000001){
		return 0;
	}
	int out;


	if (sumx>0 &&sumy>0){
		out=atan(fabs(sumy)/fabs(sumx))*(180/3.1415926);
	}

	else if (sumx<0 && sumy>0){
		out=atan(fabs(sumy)/fabs(sumx))*(180/3.1415926);
		out=180-out;
	}
	else if (sumx<0 && sumy<0){
		out=atan(fabs(sumy)/fabs(sumx))*(180/3.1415926);
		out=180+out;

	}

	else if (sumx>0 && sumy<0){
		out=atan(fabs(sumy)/fabs(sumx))*(180/3.1415926);
		out=360-out;

	}
	else {
		printf("Error!\n");
		out=100000;
	}

	return out;

}

int addMagnitude(int m1, int m2){
	int out=(m1+m2)/2;
	return out;

}

Opticalflow mergeMedian(double* degreeStore, int end, int range){

    int medianOfMedian=end*0.75;

    int s;
    int sScope=3;
    int mergedDegree=0;
    int mergedMagnitude=0;
    for(s=(-sScope/2);s<(sScope/2+1);s++){

    	double degreeStoreElement=degreeStore[medianOfMedian+s];
    	int medianMagnitude=extractIndexFromFloat(degreeStoreElement);
    	int medianDegree=(int)degreeStoreElement;
    	printf("medianDegree: %i, medianMagnitude: %i\n", medianDegree, medianMagnitude);
    	if(s==(-sScope/2)){
    		//if not, the first degree will be affected by degree of 0
    		mergedDegree=medianDegree;
    		mergedMagnitude=medianMagnitude;
    	}

    	mergedDegree=addDegree(mergedDegree, medianDegree);
    	mergedMagnitude=addMagnitude(mergedMagnitude, medianMagnitude);

    }

    printf("mergedDegree: %i, mergdeMagnitude: %i\n", mergedDegree, mergedMagnitude);
    Opticalflow medianflow=create_opticalflowFB(mergedDegree, mergedMagnitude);
    return medianflow;

}

