
#include "image.h"
#include "utils.h"
#include "blas.h"
#include "cuda.h"
#include "linkedlist.h"
#include "linkedlist.c"
#include "sortAlgorithms.h"
#include "sortAlgorithms.c"


#include "kalmanbox.h"
#include "kalmanbox.c"

#include "objectbank.h"
#include "objectbank.c"

#include "hashtable.c"

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc_c.h"

#include <stdio.h>
#include <math.h>
#include <tgmath.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "opencv2/legacy/compat.hpp"
#include "opencv2/core/mat.hpp"

#define MISS -12345
static int debug_frame=3500;  //12
static int frame_num=0;  //ADDED: count for the frame number
static image pre_im;	 //ADDED: store the previous image
static int object_num=0; //ADDED: count for the number of objects in previous frame
static int object_prenum=0;
static int *idx_tempprestore; //ADDED: store the index of valid bounding boxes
static int saveDetection=1;
static int saveOpticalflow=1;
static int clock_Adfull_thres=5; //ADDED: after this number, the additional bounding boxes will be thrown away
static int objectIndex=0;
static Boxflow *box_tempfull; //ADDED: temporarily store the Boxflow vector, and copy it the box_full in the end
static Boxflow *box_Adfull;	//ADDED: additional storage place to store those previous objects for a certain frame duration
static int box_Adfull_size=30; //ADDED: the total number of objects that can be saved inside box_Adfull
static int *clock_Adfull;	//ADDED: when there is a element in box_Adfull initialized, the respective index will count down from 10
static snode* headconstant;
Opticalflow average_Ad; //ADDED: the optical flow vector computed about box_Adfull[i]
static int trajectoryID=1; //ADDED: track the corner ID

static int trajectory=1;  //ADDED: save the trajectory191

static int MOT=0; //ADDED: test MOT dataset
static int URBEN=0; //ADDED: test URBEN dataset


//TODO: Check static issue
static kalmanbox* temp_kalmanbox;
DataItem* hashArray[SIZE];

int windows = 0;


float colors[6][3] = { {1,0,1}, {0,0,1},{0,1,1},{0,1,0},{1,1,0},{1,0,0} };

float get_color(int c, int x, int max)
{
    float ratio = ((float)x/max)*5;
    int i = floor(ratio);
    int j = ceil(ratio);
    ratio -= i;
    float r = (1-ratio) * colors[i][c] + ratio*colors[j][c];
    //printf("%f\n", r);
    return r;
}

void composite_image(image source, image dest, int dx, int dy)
{
    int x,y,k;
    for(k = 0; k < source.c; ++k){
        for(y = 0; y < source.h; ++y){
            for(x = 0; x < source.w; ++x){
                float val = get_pixel(source, x, y, k);
                float val2 = get_pixel_extend(dest, dx+x, dy+y, k);
                set_pixel(dest, dx+x, dy+y, k, val * val2);
            }
        }
    }
}

image border_image(image a, int border)
{
    image b = make_image(a.w + 2*border, a.h + 2*border, a.c);
    int x,y,k;
    for(k = 0; k < b.c; ++k){
        for(y = 0; y < b.h; ++y){
            for(x = 0; x < b.w; ++x){
                float val = get_pixel_extend(a, x - border, y - border, k);
                if(x - border < 0 || x - border >= a.w || y - border < 0 || y - border >= a.h) val = 1;
                set_pixel(b, x, y, k, val);
            }
        }
    }
    return b;
}

image tile_images(image a, image b, int dx)
{
    if(a.w == 0) return copy_image(b);
    image c = make_image(a.w + b.w + dx, (a.h > b.h) ? a.h : b.h, (a.c > b.c) ? a.c : b.c);
    fill_cpu(c.w*c.h*c.c, 1, c.data, 1);
    embed_image(a, c, 0, 0); 
    composite_image(b, c, a.w + dx, 0);
    return c;
}

image get_label(image **characters, char *string, int size)
{
    if(size > 7) size = 7;
    image label = make_empty_image(0,0,0);
    while(*string){
        image l = characters[size][(int)*string];
        image n = tile_images(label, l, -size - 1 + (size+1)/2);
        free_image(label);
        label = n;
        ++string;
    }
    image b = border_image(label, label.h*.25);
    free_image(label);
    return b;
}

void draw_label(image a, int r, int c, image label, const float *rgb)
{
    int w = label.w;
    int h = label.h;
    if (r - h >= 0) r = r - h;

    int i, j, k;
    for(j = 0; j < h && j + r < a.h; ++j){
        for(i = 0; i < w && i + c < a.w; ++i){
            for(k = 0; k < label.c; ++k){
                float val = get_pixel(label, i, j, k);
                set_pixel(a, i+c, j+r, k, rgb[k] * val);
            }
        }
    }
}

void draw_box(image a, int x1, int y1, int x2, int y2, float r, float g, float b)
{
    //normalize_image(a);
    int i;
    if(x1 < 0) x1 = 0;
    if(x1 >= a.w) x1 = a.w-1;
    if(x2 < 0) x2 = 0;
    if(x2 >= a.w) x2 = a.w-1;

    if(y1 < 0) y1 = 0;
    if(y1 >= a.h) y1 = a.h-1;
    if(y2 < 0) y2 = 0;
    if(y2 >= a.h) y2 = a.h-1;

    for(i = x1; i <= x2; ++i){
        a.data[i + y1*a.w + 0*a.w*a.h] = r;
        a.data[i + y2*a.w + 0*a.w*a.h] = r;

        a.data[i + y1*a.w + 1*a.w*a.h] = g;
        a.data[i + y2*a.w + 1*a.w*a.h] = g;

        a.data[i + y1*a.w + 2*a.w*a.h] = b;
        a.data[i + y2*a.w + 2*a.w*a.h] = b;
    }
    for(i = y1; i <= y2; ++i){
        a.data[x1 + i*a.w + 0*a.w*a.h] = r;
        a.data[x2 + i*a.w + 0*a.w*a.h] = r;

        a.data[x1 + i*a.w + 1*a.w*a.h] = g;
        a.data[x2 + i*a.w + 1*a.w*a.h] = g;

        a.data[x1 + i*a.w + 2*a.w*a.h] = b;
        a.data[x2 + i*a.w + 2*a.w*a.h] = b;
    }
}

void draw_box_width(image a, int x1, int y1, int x2, int y2, int w, float r, float g, float b)
{
    int i;
    for(i = 0; i < w; ++i){
        draw_box(a, x1+i, y1+i, x2-i, y2-i, r, g, b);
    }
}

void draw_bbox(image a, box bbox, int w, float r, float g, float b)
{
    int left  = (bbox.x-bbox.w/2)*a.w;
    int right = (bbox.x+bbox.w/2)*a.w;
    int top   = (bbox.y-bbox.h/2)*a.h;
    int bot   = (bbox.y+bbox.h/2)*a.h;

    int i;
    for(i = 0; i < w; ++i){
        draw_box(a, left+i, top+i, right-i, bot-i, r, g, b);
    }
}

image **load_alphabet()
{
    int i, j;
    const int nsize = 8;
    image **alphabets = calloc(nsize, sizeof(image));
    for(j = 0; j < nsize; ++j){
        alphabets[j] = calloc(128, sizeof(image));
        for(i = 32; i < 127; ++i){
            char buff[256];
            sprintf(buff, "data/labels/%d_%d.png", i, j);
            alphabets[j][i] = load_image_color(buff, 0, 0);
        }
    }
    return alphabets;
}

int getframe_num(void *ptr)
{
	return frame_num;
}

void initialize_idx_prestore(int size, int totalcell2){
	idx_tempprestore=(int *)calloc(size, sizeof(int));
    box_tempfull=(Boxflow *)calloc(totalcell2, sizeof(Boxflow));
    box_Adfull=(Boxflow *)calloc(box_Adfull_size, sizeof(Boxflow));
    clock_Adfull=(int *)calloc(box_Adfull_size, sizeof(int));
    //clock_Adfull[3]=5;
	return;
}

double compareFlowVector(double preFlow, double nowFlow, double preMag, double nowMag, double thFlow, double thMag){


	double diffFlow=abs(preFlow-nowFlow);
	double diffMag=abs(preMag-nowMag)/preMag;

	if(diffMag<thMag){
		return diffMag;

	}
	else if (diffFlow<thFlow){
		return diffFlow;
	}
	else{
		int maxnumber;
		int minnumber;
		if(preFlow>nowFlow){
			maxnumber=preFlow;
			minnumber=nowFlow;
		}
		else{
			maxnumber=nowFlow;
			minnumber=preFlow;

		}

		diffFlow=minnumber+360-maxnumber;
		if(diffFlow<thFlow){
			return diffFlow;
		}

	}
	return MISS;
}


int objectMath45(int num, double nowFlow, double nowMag, int nowIndex, int nowClass, Boxflow* box_full){
	int classMatch=0;
	int totalcell=num/5;
	int totalrow=(int)sqrt(totalcell);
	int possibleIndex;
	int bestIndex=num+1;
	double bestResult=180;
	double currentResult;

	//Check if two objects belong to the same group
//	if((box_full[nowIndex].classtype==1 || box_full[nowIndex].classtype==4 || box_full[nowIndex].classtype==7) && (nowClass==1 || nowClass==4 || nowClass==7)){
//		classMatch=1;
//	}
//	else if((box_full[nowIndex].classtype==0 || box_full[nowIndex].classtype==2 || box_full[nowIndex].classtype==3 || box_full[nowIndex].classtype==5 || box_full[nowIndex].classtype==6) && (nowClass==0 || nowClass==2 || nowClass==3 || nowClass==5 || nowClass==6)){
//		classMatch=1;
//	}
//	else{
//		classMatch=0;;
//	}

	//Case 1: same index, check if the optical flow is the same
	//if(classMatch==1 && box_full[nowIndex].height!=0 && box_full[nowIndex].width!=0){
	if(box_full[nowIndex].classtype==nowClass && box_full[nowIndex].height!=0 && box_full[nowIndex].width!=0){
		currentResult=compareFlowVector(box_full[nowIndex].flow.degree, nowFlow, box_full[nowIndex].flow.magnitude, nowMag, 180, 0.5);
		printf("\t Pre: idx_prestore[p]: %i degree: %0.0f mag: %0.0f objectIndex: %i\n", nowIndex, box_full[nowIndex].flow.degree, box_full[nowIndex].flow.magnitude, box_full[nowIndex].objectIndex);

		if(currentResult!=MISS){
			bestIndex=nowIndex;
			return bestIndex;
		}

	}

	//Case 2: compare with the surronding 45 boxes
	int base=nowIndex%totalcell;
	int nn_level;
	for(nn_level=0;nn_level<5;nn_level++){
		int row;
		for (row=-1;row<2;row++){
			int col;
			for (col=-1;col<2;col++){
				possibleIndex=(base+col+totalrow*row)+totalcell*nn_level;
				if(possibleIndex==nowIndex){
					continue;
				}

				//Check if two objects belong to the same group
//				if((box_full[possibleIndex].classtype==1 || box_full[possibleIndex].classtype==4 || box_full[possibleIndex].classtype==7) && (nowClass==1 || nowClass==4 || nowClass==7)){
//					classMatch=1;
//				}
//				else if((box_full[possibleIndex].classtype==0 || box_full[possibleIndex].classtype==2 || box_full[possibleIndex].classtype==3 || box_full[possibleIndex].classtype==5 || box_full[possibleIndex].classtype==6) && (nowClass==0 || nowClass==2 || nowClass==3 || nowClass==5 || nowClass==6)){
//					classMatch=1;
//				}
//
//				else{
//					classMatch=0;
//				}


				if(box_full[possibleIndex].classtype==nowClass && box_full[possibleIndex].width!=0 && box_full[possibleIndex].height!=0){
					printf("\t Pre: idx_prestore[p]: %i degree: %0.0f mag: %0.0f objectIndex: %i\n", possibleIndex, box_full[possibleIndex].flow.degree, box_full[possibleIndex].flow.magnitude, box_full[possibleIndex].objectIndex);
					currentResult=compareFlowVector(box_full[possibleIndex].flow.degree, nowFlow, box_full[possibleIndex].flow.magnitude, nowMag, 90, 0.5);
					if(currentResult!=MISS && (currentResult<bestResult)){
						bestIndex=possibleIndex;
						bestResult=currentResult;

					}

				}

			}
		}
	}



	return bestIndex;
}
int matchIndex(int num, int preIndex, int nowIndex){
	int totalcell=num/5;
	int totalrow=(int)sqrt(totalcell);

	int base=preIndex%totalcell;
	int nn_level;
	for(nn_level=0;nn_level<5;nn_level++){
		int row;
		for (row=-1;row<2;row++){
			int i;
			for (i=-1;i<2;i++){
			int possibleIndex=(base+i+totalrow*row)+totalcell*nn_level;

			if(possibleIndex==nowIndex){
				return 1;
			}


			}
		}
	}

	return 0;


}

//int objectMatch(int num, int preFlow, int nowFlow, int preMag, int nowMag, int preClass, int nowClass, int preIndex, int nowIndex){
//	int totalcell=num/5;
//	int totalrow=(int)sqrt(totalcell);
//	if(preClass!=nowClass){
//		return 0;
//	}
//
//	//Case 1: same index, check if the optical flow is the same
//	else if(preIndex%totalcell==nowIndex%totalcell){
//		return compareFlowVector(preFlow, nowFlow, preMag,nowMag,6);
//	}
//
////	//Case 2: different index, and the previous optical flow is 0
////	else if(preMag==0 && (preIndex%totalcell!=nowIndex%totalcell)){
////		return 0;
////	}
//
//	//Case 3: optical flow is the same, check if the index is within the range of 3*3*5
//	else if(compareFlowVector(preFlow, nowFlow, preMag,nowMag,1)){
//		int base=preIndex%totalcell;
//		int nn_level;
//		for(nn_level=0;nn_level<5;nn_level++){
//			int row;
//			for (row=-1;row<2;row++){
//				int i;
//				for (i=-1;i<2;i++){
//					int possibleIndex=(base+i+totalrow*row)+totalcell*nn_level;
//
//					if(possibleIndex==nowIndex){
//						return 1;
//					}
//
//
//				}
//			}
//		}
//
//	}
//
//	return 0;
//
//}

int lookAround(float** probsLastFrame, float** probs, int num, int prevIndex, int classIndex, float prevProb, float thresh, double prevDegree){
	//prevIndex: location index from the previous frame
	//prevProb: Prob of being certain class in last frame
	prevProb=prevProb/100.0;
	int totalcell=num/5;
	int currentBase=prevIndex%totalcell;
	float bump=0;
	int maxCellIndex;

	float currentProb=probs[prevIndex][classIndex];
	float percentDiff=(prevProb-currentProb)/prevProb;
	printf("\t probs[%i] of class[%i] drops by %0.2f\n", prevIndex, classIndex, percentDiff);


	if(currentProb<thresh){
		maxCellIndex=searchWithDirection(probs, num, currentBase, classIndex, prevDegree);
		float maxProb=probs[maxCellIndex][classIndex];
		printf("\t maxCellIndex: %i of %0.3f\n", maxCellIndex, maxProb);

		float preAdjProb=probsLastFrame[maxCellIndex][classIndex];
		float percentAdjDiff=(maxProb-preAdjProb)/preAdjProb;
		printf("\t probs[%i] of class[%i] increases by %0.2f\n", maxCellIndex, classIndex, percentAdjDiff);


		probs[maxCellIndex][classIndex]=probs[maxCellIndex][classIndex]+bump;
		probs[maxCellIndex][80]=probs[maxCellIndex][classIndex];

	}


 	return 0;

}

void draw_tracking(IplImage *im_frame, int left, int top, int width, int height, int color1, int color2, int color3, int objectIndex){
	CvPoint lefttop=cvPoint(left, top);
	CvPoint rightbot=cvPoint(left+width, top+height);
	CvScalar color=CV_RGB(color1, color2, color3);
	cvRectangle(im_frame, lefttop, rightbot, color, 2, 8, 0 );
    char object[sizeof(objectIndex)];
    sprintf(object, "%d", objectIndex);
	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1, 1, 0, 3, 8);
	cvPutText(im_frame, object, cvPoint(left, (top+height)), &font, color);

    cvNamedWindow("tracking",CV_WINDOW_NORMAL);
    cvShowImage("tracking",im_frame);

	return;
}

void saveUnmatched(IplImage *im_frame, Boxflow in, int arraysize){

		printf("\n");
		printf("\t temporarily save object %i\n", in.objectIndex);

		kalmanbox* temp_kalmanbox=hashsearch(hashArray, in.objectIndex)->element;

		Boxflow temp=in;
		int width=temp.width;
		int height=temp.height;
		int left=temp_kalmanbox->y_k->data.fl[0]-width/2;
		int top=temp_kalmanbox->y_k->data.fl[1]-height/2;

 		printf("\t left: %i, top: %i, width: %i, height: %i\n", left, top, width, height);

		//printf("3. Kalman Filter Update: \n");
		CvPoint boxcenter=cvPoint(temp_kalmanbox->y_k->data.fl[0], temp_kalmanbox->y_k->data.fl[1]);
		CvPoint boxvelocity=cvPoint(in.flow.magnitude*cos(in.flow.degree*3.1415926/180), -(in.flow.magnitude*sin(in.flow.degree*3.1415926/180)));

		printf("\t magnitude: %0.0f, flow: %0.0f\n", in.flow.magnitude, in.flow.degree);

 		update_kalmanfilter(im_frame, temp_kalmanbox, boxcenter, boxvelocity, width, height);
		hashUpdate(hashArray, in.objectIndex, temp_kalmanbox);


		//If temp is previously saved in the box_Adfull, then find it. Else, save the temp into the first empty index
		int i;
		int j=MISS;
		int jlock=0;
		for(i=0;i<arraysize;i++){
			if(box_Adfull[i].objectIndex==in.objectIndex){
				printf("\t Find the object %i in box_Adfull\n", box_Adfull[i].objectIndex);
				j=i;
				clock_Adfull[j]=clock_Adfull[j]+1;
				printf("\t Increment temporary clock: %i\n", clock_Adfull[j]);
				break;
			}

			//ADDED: find the first empty index to save temp
			if(jlock==0 && box_Adfull[i].width==0 && box_Adfull[i].height==0){
				j=i;
				jlock=1;
				clock_Adfull[j]=0;
				printf("\t Initialize temporary clock: %i\n", clock_Adfull[i]);

			}
		}

		if(j==MISS){
			assert(0 && "box_Adfull is full!\n");
		}


		//Case 1: get out of the boundary
		if(left<0 || (left+width)>im_frame->width || top<0 || (top+height)>im_frame->height){
			printf("\t Discard object %i due to out of boundaries!\n", in.objectIndex);
			Boxflow nullflow=putNullInsideBox();
			box_Adfull[j]=nullflow;
			clock_Adfull[j]=0;

			cvReleaseKalman(&(temp_kalmanbox->kalmanfilter));
			hashdelete(hashArray, in.objectIndex);

			return;
		}

		temp.left=left;
		temp.top=top;
		temp.width=width;
		temp.height=height;

		box_Adfull[j]=temp;


		//Case 2: Time is up
		if(clock_Adfull[j]>clock_Adfull_thres){
			printf("\t Discard object %i due to out of time\n", in.objectIndex);
			Boxflow nullflow=putNullInsideBox();
			box_Adfull[j]=nullflow;
			clock_Adfull[j]=0;

			cvReleaseKalman(&(temp_kalmanbox->kalmanfilter));
			hashdelete(hashArray, in.objectIndex);
			return;

		}


		draw_tracking(im_frame, left, top, width, height, 0, 0, 52, box_Adfull[j].objectIndex);
		if(trajectory==1){
			trajectoryID=draw_trajectory2(box_Adfull[j].objectIndex, trajectoryID, frame_num-2, left, top, width, height);

		}
//		if(URBEN==1){
//    		Urben1(box_Adfull[j].classtype, box_Adfull[j].objectIndex, 20+frame_num-2, left, top, width, height);
//		}

        if(frame_num>=debug_frame){
        	cvWaitKey(0);
        }
        return;
}

void loopUnmatched(IplImage *im_frame, int arraysize){
	int i;

	for(i=0;i<arraysize;i++){
		if(box_Adfull[i].width==0 && box_Adfull[i].height==0){
			continue;
		}
		printf("\n");

		Boxflow in=box_Adfull[i];
		kalmanbox* temp_kalmanbox=hashsearch(hashArray, in.objectIndex)->element;

		Boxflow temp=box_Adfull[i];
		int width=temp.width;
		int height=temp.height;
		int left=temp_kalmanbox->y_k->data.fl[0]-width/2;
		int top=temp_kalmanbox->y_k->data.fl[1]-height/2;


		printf("\t objectIndex: %i, left: %i, top: %i, width: %i, height: %i\n", temp.objectIndex, left, top, width, height);

		CvPoint boxcenter=cvPoint(temp_kalmanbox->y_k->data.fl[0], temp_kalmanbox->y_k->data.fl[1]);
		CvPoint boxvelocity=cvPoint(in.flow.magnitude*cos(in.flow.degree*3.1415926/180), -(in.flow.magnitude*sin(in.flow.degree*3.1415926/180)));


		update_kalmanfilter(im_frame, temp_kalmanbox, boxcenter, boxvelocity, width, height);
		hashUpdate(hashArray, in.objectIndex, temp_kalmanbox);
		printf("\t magnitude: %0.0f, flow: %0.0f\n", in.flow.magnitude, in.flow.degree);


		//Case 1: get out of the boundary
		if(left<0 || (left+width)>im_frame->width || top<0 || (top+height)>im_frame->height){
			printf("\t Discard object %i due to out of boundaries!\n", in.objectIndex);
			Boxflow nullflow=putNullInsideBox();
			box_Adfull[i]=nullflow;
			clock_Adfull[i]=0;
			cvReleaseKalman(&(temp_kalmanbox->kalmanfilter));
			hashdelete(hashArray, in.objectIndex);
			continue;
		}

		temp.left=left;
		temp.top=top;
		temp.width=width;
		temp.height=height;

		box_Adfull[i]=temp;
		clock_Adfull[i]=clock_Adfull[i]+1;

		//Case 2: Time is up
		if(clock_Adfull[i]>clock_Adfull_thres){
			printf("\t Discard object %i due to out of time\n", in.objectIndex);
			Boxflow nullflow=putNullInsideBox();
			box_Adfull[i]=nullflow;
			clock_Adfull[i]=0;
			cvReleaseKalman(&(temp_kalmanbox->kalmanfilter));
			hashdelete(hashArray, in.objectIndex);
			continue;

		}


		draw_tracking(im_frame, left, top, width, height, 0, 0, 52, box_Adfull[i].objectIndex);
		if(trajectory==1){
			trajectoryID=draw_trajectory2(box_Adfull[i].objectIndex, trajectoryID, frame_num-2, left, top, width, height);

		}

//		if(URBEN==1){
//    		testAgainstUrben1(box_Adfull[i].classtype, box_Adfull[i].objectIndex, 20+frame_num-2, left, top, width, height);
//		}
        if(frame_num>=debug_frame){
        	cvWaitKey(0);
        }

	}
	return;
}

int calculateOverlapping(Boxflow unmatched, int* current){
	if(unmatched.height==0 && unmatched.width==0){
		return 0;
	}
	printf("\t Unmatched: box_Adfull[i], degree: %0.0f, mag: %0.0f objectIndex: %i\n", unmatched.flow.degree, unmatched.flow.magnitude, unmatched.objectIndex);
	int xleft=current[0];
	int xtop=current[1];
	int xright=current[0]+current[2];
	int xbottom=current[1]+current[3];

	int aleft=unmatched.left;
	int atop=unmatched.top;
	int aright=unmatched.width+unmatched.left;
	int abottom=unmatched.height+unmatched.top;

	if(aright<xleft || aleft>xright || abottom<xtop || atop>xbottom){
		return 0;
	}

	return 1;
}


int overLappingOneDirection(int x1, int w1, int x2, int w2){

	int l1 = x1;
	int l2 = x2;
	int left = l1 > l2 ? l1 : l2;
	int r1 = x1 + w1;
    int r2 = x2 + w2;
    int right = r1 < r2 ? r1 : r2;
    int value=right-left;
    if(value>0){
    	return value;
    }
    else{
    	return 0;
    }
}

int overLappingArea(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2){
	int w=overLappingOneDirection(x1, w1, x2, w2);
	int h=overLappingOneDirection(y1, h1, y2, h2);
	int area;
    if(w < 0 || h < 0) return 0;
    area = w*h;
    return area;
}

int calculateOverlappingRatio(int num, int nowIndex, int **box_para, Boxflow* box_full, Boxflow* box_tempfull, DataItem* hashArray, float threshold){


	int totalcell=num/5;
	int totalrow=(int)sqrt(totalcell);
	int possibleIndex;
	float overLapRatio;

	int base=nowIndex%totalcell;
	int nn_level;
	for(nn_level=0;nn_level<5;nn_level++){
		int row;
		for (row=-1;row<2;row++){
			int i;
			for (i=-1;i<2;i++){
				possibleIndex=(base+i+totalrow*row)+totalcell*nn_level;

				if(possibleIndex==nowIndex){
					continue;
				}
				int x1=box_para[possibleIndex][0];
				int y1=box_para[possibleIndex][1];
				int w1=box_para[possibleIndex][2];
				int h1=box_para[possibleIndex][3];

				int x2=box_para[nowIndex][0];
				int y2=box_para[nowIndex][1];
				int w2=box_para[nowIndex][2];
				int h2=box_para[nowIndex][3];


				int area=overLappingArea(x1, y1, w1, h1, x2, y2, w2, h2);
				overLapRatio=(float)area/(w2*h2);


				if(overLapRatio>threshold){
					int trajectoryID=0;
					int possibleObjectIndex=MISS; //first bounding box
					int nowObjectIndex=MISS;	//second bounding box
					int targetObjectIndex;
					int targetIndex;
					int testststs=box_full[191].objectIndex;
					if((box_full[possibleIndex].height!=0)&&box_full[possibleIndex].width!=0){
						possibleObjectIndex=box_full[possibleIndex].objectIndex;
					}

					else if((box_tempfull[possibleIndex].height!=0)&&box_tempfull[possibleIndex].width!=0){
						possibleObjectIndex=box_tempfull[possibleIndex].objectIndex;
					}

					if(box_full[nowIndex].height!=0&&box_full[nowIndex].width!=0){
						nowObjectIndex=box_full[nowIndex].objectIndex;
					}

					else if((box_tempfull[nowIndex].height!=0)&&box_tempfull[nowIndex].width!=0){
						nowObjectIndex=box_tempfull[nowIndex].objectIndex;
					}

					//Case 1: previously correct bbox matching, duplicated bounding boxes
					if(possibleObjectIndex!=MISS && nowObjectIndex==MISS){
						targetObjectIndex=possibleObjectIndex;
						targetIndex=possibleIndex;
					}

					//Case 2: previously wrong bbox matching, duplicated bounding boxes
					else if(possibleObjectIndex==MISS && nowObjectIndex!=MISS){
						targetObjectIndex=nowObjectIndex;
						targetIndex=nowIndex;
					}

					//Case 3: The class id of detection box changes, both detection bounding boxes are valid
					else if(possibleObjectIndex!=MISS && nowObjectIndex!=MISS){
						continue;

					}

					//Case 4: no previous bbox matching, duplcated bounding boxes
					else if(possibleObjectIndex==MISS && nowObjectIndex==MISS){
						//TODO: later
						//assert(0 && "Fix it!\n");
						continue;
					}

				    int clock=hashsearch(hashArray, targetObjectIndex)->element->clock;
				    if(clock>=5){
						printf("\t overlapping objectIndex: %i, possibelIndex: %i\n", targetObjectIndex, targetIndex);
					    printf("\t overlapping area: %i, ratio: %0.02f\n", area, overLapRatio);
				    	return 1;
				    }

				}


			}
		}
	}
	return 0;
}
void draw_trajectory1(int object, int class){
	FILE *f1 = fopen("table/objects.txt", "a");
	fprintf(f1, "%i, %i, %i\n", object, class, 1);
	fclose(f1);
}

int draw_trajectory2(int object, int trajectoryID, int frame_num_minus2, int topleftx, int toplefty, int width, int height){
	FILE *f2 = fopen("table/objects_features.txt", "a");
	FILE *f3 = fopen("table/Positions.txt", "a");


	fprintf(f2, "%i, %i\n", object, trajectoryID);
	fprintf(f3, "%i, %i, %i, %i\n",trajectoryID, frame_num_minus2, topleftx, toplefty);
	trajectoryID=trajectoryID+1;

	fprintf(f2, "%i, %i\n", object, trajectoryID);
	fprintf(f3, "%i, %i, %i, %i\n",trajectoryID, frame_num_minus2, topleftx+width, toplefty);
	trajectoryID=trajectoryID+1;

	fprintf(f2, "%i, %i\n", object, trajectoryID);
	fprintf(f3, "%i, %i, %i, %i\n",trajectoryID, frame_num_minus2, topleftx, toplefty+height);
	trajectoryID=trajectoryID+1;

	fprintf(f2, "%i, %i\n", object, trajectoryID);
	fprintf(f3, "%i, %i, %i, %i\n",trajectoryID, frame_num_minus2, topleftx+width, toplefty+height);
	trajectoryID=trajectoryID+1;

	fclose(f2);
	fclose(f3);
	return trajectoryID;
}

void testAgainstUrben1(int objectClass, int objectIndex22, int frame_num_minus2, int topleftx, int toplefty, int width, int height){
	//if(objectIndex22!=2 && objectIndex22!=8 && objectIndex22!=57 && objectIndex22!=65 && objectIndex22!=76 && objectIndex22!=78 && objectIndex22!=3 && objectIndex22!=16  && objectIndex22!=19  && objectIndex22!=33  && objectIndex22!=42  && objectIndex22!=52 && objectIndex22!=75 ){
	//if(objectIndex22!=75 && objectIndex22!=0 && objectIndex22!=2 && objectIndex22!=3 && objectIndex22!=4 && objectIndex22!=5 && objectIndex22!=13 && objectIndex22!=14 && objectIndex22!=22 && objectIndex22!=26 && objectIndex22!=27 && objectIndex22!=32 && objectIndex22!=34 && objectIndex22!=40 && objectIndex22!=43 && objectIndex22!=47 && objectIndex22!=51 && objectIndex22!=66 && objectIndex22!=70 && objectIndex22!=72 && objectIndex22!=77 && objectIndex22!=78 && objectIndex22!=79 && objectIndex22!=82 && objectIndex22!=83 && objectIndex22!=85 && objectIndex22!=92 && objectIndex22!=93 && objectIndex22!=100 && objectIndex22!=101){
		//if(objectIndex22==73 || objectIndex22==74 || objectIndex22==75){
	if(objectIndex22!=6 && objectIndex22!=79 && objectIndex22!=91 && objectIndex22!=96 && objectIndex22!=102 && objectIndex22!=106 && objectIndex22!=108 && objectIndex22!=110 && objectIndex22!=116 && objectIndex22!=124 && objectIndex22!=189  &&  objectIndex22!=2 && objectIndex22!=10 && objectIndex22!=16 && objectIndex22!=29 && objectIndex22!=32 && objectIndex22!=34 && objectIndex22!=37 && objectIndex22!=39 && objectIndex22!=42 && objectIndex22!=45 && objectIndex22!=47 && objectIndex22!=49 && objectIndex22!=53 && objectIndex22!=58 && objectIndex22!=70 && objectIndex22!=74 && objectIndex22!=76 && objectIndex22!=78 && objectIndex22!=82 && objectIndex22!=83 && objectIndex22!=88 && objectIndex22!=89 && objectIndex22!=95 && objectIndex22!=96 && objectIndex22!=102 && objectIndex22!=104 && objectIndex22!=106 && objectIndex22!=107 && objectIndex22!=108 && objectIndex22!=110 && objectIndex22!=114 && objectIndex22!=116 && objectIndex22!=123 && objectIndex22!=124 && objectIndex22!=130  && objectIndex22!=135 && objectIndex22!=136 && objectIndex22!=141 && objectIndex22!=149 && objectIndex22!=153 && objectIndex22!=158 && objectIndex22!=160 && objectIndex22!=166 && objectIndex22!=168 && objectIndex22!=171 && objectIndex22!=176 && objectIndex22!=177 && objectIndex22!=185 && objectIndex22!=188 && objectIndex22!=189){
	//if(objectIndex22!=2 && objectIndex22!=4 && objectIndex22!=11 && objectIndex22!=13){
		if(objectClass==7 || objectClass==3 || objectClass==2 || objectClass==0 || objectClass==1){
		//if(objectClass==3){
		//if(objectClass==7){
		//if(objectClass==1){

			FILE *f5=fopen("Urben/bounding_boxes.txt", "a");
			fprintf(f5, "%i,%i,%i,%i,%i,%i\n", objectIndex22, frame_num_minus2, topleftx, toplefty, topleftx+width, toplefty+height);
			fclose(f5);
		}
	}
	return;
}
void testAgainstUrben2(int objcetClass, int objectIndex22){
	//if(objectIndex22!=2 && objectIndex22!=8 && objectIndex22!=57 && objectIndex22!=65 && objectIndex22!=76 && objectIndex22!=78 && objectIndex22!=3 && objectIndex22!=16  && objectIndex22!=19  && objectIndex22!=33  && objectIndex22!=42  && objectIndex22!=52 && objectIndex22!=75 ){
	//if(objectIndex22!=75 && objectIndex22!=0 && objectIndex22!=2 && objectIndex22!=3 && objectIndex22!=4 && objectIndex22!=5 && objectIndex22!=13 && objectIndex22!=14 && objectIndex22!=22 && objectIndex22!=26 && objectIndex22!=27 && objectIndex22!=32 && objectIndex22!=34 && objectIndex22!=40 && objectIndex22!=43 && objectIndex22!=47 && objectIndex22!=51 && objectIndex22!=66 && objectIndex22!=70 && objectIndex22!=72 && objectIndex22!=77 && objectIndex22!=78 && objectIndex22!=79 && objectIndex22!=82 && objectIndex22!=83 && objectIndex22!=85 && objectIndex22!=92 && objectIndex22!=93 && objectIndex22!=100 && objectIndex22!=101){
		//if(objectIndex22==73 || objectIndex22==74 || objectIndex22==75){
	if(objectIndex22!=6 && objectIndex22!=79 && objectIndex22!=91 && objectIndex22!=96 && objectIndex22!=102 && objectIndex22!=106 && objectIndex22!=108 && objectIndex22!=110 && objectIndex22!=116 && objectIndex22!=124 && objectIndex22!=189  &&  objectIndex22!=2 && objectIndex22!=10 && objectIndex22!=16 && objectIndex22!=29 && objectIndex22!=32 && objectIndex22!=34 && objectIndex22!=37 && objectIndex22!=39 && objectIndex22!=42 && objectIndex22!=45 && objectIndex22!=47 && objectIndex22!=49 && objectIndex22!=53 && objectIndex22!=58 && objectIndex22!=70 && objectIndex22!=74 && objectIndex22!=76 && objectIndex22!=78 && objectIndex22!=82 && objectIndex22!=83 && objectIndex22!=88 && objectIndex22!=89 && objectIndex22!=95 && objectIndex22!=96 && objectIndex22!=102 && objectIndex22!=104 && objectIndex22!=106 && objectIndex22!=107 && objectIndex22!=108 && objectIndex22!=110 && objectIndex22!=114 && objectIndex22!=116 && objectIndex22!=123 && objectIndex22!=124 && objectIndex22!=130  && objectIndex22!=135 && objectIndex22!=136 && objectIndex22!=141 && objectIndex22!=149 && objectIndex22!=153 && objectIndex22!=158 && objectIndex22!=160 && objectIndex22!=166 && objectIndex22!=168 && objectIndex22!=171 && objectIndex22!=176 && objectIndex22!=177 && objectIndex22!=185 && objectIndex22!=188 && objectIndex22!=189){
	//if(objectIndex22!=2 && objectIndex22!=4 && objectIndex22!=11 && objectIndex22!=13){
		if(objcetClass==7){
			FILE *f6=fopen("Urben/objects.txt", "a");
			fprintf(f6, "%i,%i,%s\n", objectIndex22, 2, "person");
			fclose(f6);
		}

		else if(objcetClass==3){
			FILE *f6=fopen("Urben/objects.txt", "a");
			fprintf(f6, "%i,%i,%s\n", objectIndex22, 1, "car");
			fclose(f6);
		}

		else if(objcetClass==2){
			FILE *f6=fopen("Urben/objects.txt", "a");
			fprintf(f6, "%i,%i,%s\n", objectIndex22, 5, "bus");
			fclose(f6);
		}

		else if(objcetClass==0){
			FILE *f6=fopen("Urben/objects.txt", "a");
			fprintf(f6, "%i,%i,%s\n", objectIndex22, 6, "truck");
			fclose(f6);
		}
//
		else if(objcetClass==1){
			FILE *f6=fopen("Urben/objects.txt", "a");
			fprintf(f6, "%i,%i,%s\n", objectIndex22, 4, "bicycle");
			fclose(f6);
		}
	}

	return;
}

void draw_detections(image im, int num, float thresh, box *boxes, float **probs, char **names, image **alphabet, int classes, int **box_para, int *idx_store, Boxflow *box_full)
{
    int i;
    int idx_count=0;

    //clock_t time1=clock();
    image screenshot=copy_image(im);


    if(object_num>0){//frame_num=2 and above
    	int p;

    	//pre_im_frame is for displaying the detected bounding boxes
        IplImage *pre_im_frame=cvCreateImage(cvSize(pre_im.w,pre_im.h), IPL_DEPTH_8U, pre_im.c);
        pre_im_frame=image_convert_IplImage(pre_im, pre_im_frame);

        if(frame_num>=debug_frame){
            printf("Wake up!\n");
        }


     	for(p=0;p<object_num;p++){

            printf("\n1. Calculate Optical Flow for Current Objects: \n");
    		//TIME3:
    		//clock_t time3=clock();

    		//TODO: Need to optimize to speed up later
            IplImage *boxcrop=cvCreateImage(cvSize(im.w,im.h), IPL_DEPTH_8U, im.c);
            boxcrop=image_convert_IplImage(im, boxcrop);

            IplImage *pre_boxcrop=cvCreateImage(cvSize(pre_im.w,pre_im.h), IPL_DEPTH_8U, pre_im.c);
            pre_boxcrop=image_convert_IplImage(pre_im, pre_boxcrop);

            //TIME:
            //clock_t time6=clock();
        	//Opticalflow total_result=compute_opticalflowFB(pre_boxcrop, boxcrop, frame_num, debug_frame);
        	//printf("Complete Image Optical Flow Computation: %lf seconds\n", sec(clock()-time6));


            cvSetImageROI(pre_boxcrop, cvRect(box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3]));
            cvSetImageROI(boxcrop, cvRect(box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3]));


        	//Opticalflow average_result=compute_opticalflow(pre_boxcrop, boxcrop, box_para[idx_store[p]][0], box_para[idx_store[p]][1]);
        	Opticalflow average_result=compute_opticalflowFB(pre_boxcrop, boxcrop, frame_num, debug_frame);


        	int match=0; //current matches previous objects from box_full
        	int match2=0; //current rematches saved objects from box_Adfull
        	int overlap=0;

        	//printf("Optical Flow Computation: %lf seconds\n", sec(clock()-time3));

        	printf("\n2. Match and Update: \n");
        	//TIME:
        	//time3=clock();
    		printf("\t Current: idx_store[p]: %i degree: %0.0f mag: %0.0f\n", idx_store[p], average_result.degree, average_result.magnitude);
    		snode* headcount=headconstant;
    		int newIndex=idx_store[p];

    		int bestIndex=objectMath45(num, average_result.degree, average_result.magnitude, newIndex, box_para[idx_store[p]][4], box_full);
    		if(bestIndex!=num+1){
            	match=1;
            	box_para[idx_store[p]][9]=box_full[bestIndex].objectIndex;


            	Boxflow nullflow=putNullInsideBox();
            	printf("\t %i matches with %i, with objectIndex: %i\n", idx_store[p], bestIndex, box_full[bestIndex].objectIndex);

        		int preFlow=box_full[bestIndex].flow.degree;
        		int preMag=box_full[bestIndex].flow.magnitude;


        		//Remove the object which is matched from the last frame
            	box_full[bestIndex]=nullflow;

            	average_result=updateFlow(preFlow, preMag, average_result);
            	printf("\t degree updates from %i to %0.0f\n", preFlow, average_result.degree);
            	printf("\t magnitude updates from %i to %0.0f\n", preMag, average_result.magnitude);
            	object_prenum=object_prenum-1;

            	headcount=headconstant;
            	while(headcount!=NULL){

            		if(headcount->data==bestIndex){
            			headconstant=remove_any(headconstant,headcount);
            			break;
            		}
            		headcount=headcount->next;
            	}

    		}





        	//If there is no match, compare with the previously unmatched ones inside box_Adfull
    		//TODO: if there is nothing inside, don't compare
        	if(match==0){
            	//TODO: Optimize O(n)
        		double bestResult=180;
        		double currentResult;

            	int ii=box_Adfull_size+1;
            	for(ii=0;ii<box_Adfull_size;ii++){
                	overlap=calculateOverlapping(box_Adfull[ii], box_para[idx_store[p]]);
                	int fullIndex=(box_Adfull[ii].nn+1)*(num/5)+(box_Adfull[ii].row*(int)sqrt(num/5)+box_Adfull[ii].col);

                	if(overlap==1 && box_Adfull[ii].classtype==box_para[idx_store[p]][4]){

                		currentResult=compareFlowVector(box_Adfull[ii].flow.degree, average_result.degree, box_Adfull[ii].flow.magnitude, average_result.magnitude, 100, 0.5);
						if(currentResult!=MISS && currentResult<bestResult){
	                		bestIndex=ii;
	                		bestResult=currentResult;
	                		match2=1;
	                		break;
						}

                	}

            	}

            	if(overlap && match2){
            		printf("\t %i REMATCHES with box_Adfull[%i], with objectIndex: %i\n", idx_store[p], ii, box_Adfull[ii].objectIndex);
                	box_para[idx_store[p]][9]=box_Adfull[ii].objectIndex;
                	Boxflow nullflow=putNullInsideBox();
                	box_Adfull[ii]=nullflow;
                	clock_Adfull[ii]=0;
                	printf("\t degree stays the same\n");
            	}
        	}



    		//The +y means going down, -y means going up
    		CvPoint boxcenter=cvPoint(box_para[idx_store[p]][0]+(box_para[idx_store[p]][2]/2), box_para[idx_store[p]][1]+(box_para[idx_store[p]][3])/2);
    		CvPoint boxvelocity=cvPoint(average_result.magnitude*cos(average_result.degree*3.1415926/180), -(average_result.magnitude*sin(average_result.degree*3.1415926/180)));

    		//printf("Object Matching and Optical Flow Update: %lf seconds\n", sec(clock()-time3));

    		printf("\n3. Discrete Kalman Filter: \n");
    		//TIME:
    		//time3=clock();


        	if(match || (match2 && overlap)){
        		//ADDED: store the index of valid bounding boxes
        		//if matched, update the each kalman filter in the hashtable
        		printf("Kalman Filter Update: \n");
        		temp_kalmanbox=hashsearch(hashArray, box_para[idx_store[p]][9])->element;

        		update_kalmanfilter(pre_im_frame, temp_kalmanbox, boxcenter, boxvelocity, box_para[idx_store[p]][2], box_para[idx_store[p]][3]);
        		hashUpdate(hashArray, box_para[idx_store[p]][9], temp_kalmanbox);



//        		if(frame_num>debug_frame&&box_para[idx_store[p]][9]==5){
//
//        			printf("4. Prob Bumping: \n");
//           			printf("\t HERE! prob: %i index: %i \n", box_para[idx_store[p]][5], idx_store[p]);
//        			float** probsMore=getProbsMore(1);
//        			int ctbumping=lookAround(probsMore, probs, num, idx_store[p], box_para[idx_store[p]][4], box_para[idx_store[p]][5], thresh, average_result.degree);
//        			printf("\t Object %i is moving: %i\n",box_para[idx_store[p]][9], ctbumping);
//
//
//        		}
        		//Save the matched Boxflow into a temporary array
         		Boxflow temp_boxflow=putFlowInsideBox(average_result,box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3], box_para[idx_store[p]][4], box_para[idx_store[p]][5], box_para[idx_store[p]][6], box_para[idx_store[p]][7], box_para[idx_store[p]][8], box_para[idx_store[p]][9]);
        		box_tempfull[idx_store[p]]=temp_boxflow;

        	}
        	else{

        		//Added a condition to get rid of the duplicated bounding boxes
        		int discard=calculateOverlappingRatio(num, idx_store[p], box_para, box_full, box_tempfull, hashArray, 0.1);
        		if(discard==1){

                	cvReleaseImage(&pre_boxcrop);
                	cvReleaseImage(&boxcrop);
            		printf("\t Discard!\n");
        			continue;

        		}
        		//if not matched, put the new kalman filter inside the hashtable		snode* head=NULL;
        		box_tempfull[idx_store[p]]=putFlowInsideBox(average_result,box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3], box_para[idx_store[p]][4], box_para[idx_store[p]][5], box_para[idx_store[p]][6], box_para[idx_store[p]][7], box_para[idx_store[p]][8], objectIndex);
        		printf("\t new object: %i\n", objectIndex);

         		printf("Kalman Filter Initilization: \n");
        		temp_kalmanbox=create_kalmanfilter(boxcenter, boxvelocity);
        		hashinsert(hashArray, objectIndex, temp_kalmanbox);
        		objectIndex=objectIndex+1;

        		if(trajectory==1){
        			draw_trajectory1(box_tempfull[idx_store[p]].objectIndex, box_para[idx_store[p]][4]);
        		}

        		if(URBEN==1){
        			testAgainstUrben2(box_para[idx_store[p]][4], box_tempfull[idx_store[p]].objectIndex);
        		}

        	}

        	//printf("Discrete Kalman Filter: %lf seconds\n", sec(clock()-time3));



        	//drawArrow(im_frame, average_result.abs_p0, average_result.abs_p1, CV_RGB(box_para[idx_store[p]][10], box_para[idx_store[p]][11], box_para[idx_store[p]][12]), 10, 2, 9, 0);
        	draw_tracking(pre_im_frame, box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3], box_para[idx_store[p]][10], box_para[idx_store[p]][11], box_para[idx_store[p]][12], box_tempfull[idx_store[p]].objectIndex);

        	if(URBEN==1){
        		testAgainstUrben1(box_para[idx_store[p]][4], box_tempfull[idx_store[p]].objectIndex, 1000+frame_num-2, box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3]);
    		}



        	if(box_para[idx_store[p]][4]==0 && MOT==1){
            	FILE *f4=fopen("out.txt", "a");
            	fprintf(f4, "%i,%i,%i,%i,%i,%i,%i,%i,%i\n", frame_num-1, box_tempfull[idx_store[p]].objectIndex, box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3], box_para[idx_store[p]][5], box_para[idx_store[p]][4]+1, 1);
    			fclose(f4);

            	FILE *f5=fopen("out2.txt", "a");
            	fprintf(f5, "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i\n", frame_num-1, box_tempfull[idx_store[p]].objectIndex, box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3], box_para[idx_store[p]][5], -1, -1, -1, -1);
    			fclose(f5);

        	}



        	//Plot the tracking result in terms of tracjectories
        	if(trajectory==1){
        		trajectoryID=draw_trajectory2(box_tempfull[idx_store[p]].objectIndex, trajectoryID, frame_num-2, box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3]);
        	}

            if(frame_num>=debug_frame){
            	cvWaitKey(0);
            }

            idx_tempprestore[p]=idx_store[p];
        	cvReleaseImage(&pre_boxcrop);
        	cvReleaseImage(&boxcrop);
    	}


    	printf("\n4. Draw Previous Unmatched Objects\n");
    	//TIME:
    	//clock_t time4=clock();
    	hashdisplay(hashArray);
    	loopUnmatched(pre_im_frame, box_Adfull_size);
        //printf("Draw Previous Unmatched Objects in %lf seconds\n", sec(clock()-time4));


    	printf("\n5. Process Current Unmatched Objects: \n");
        //TIME:
        //time4=clock();

    	if (object_prenum!=0){
    		snode* headcount=headconstant->next;
    		kalmanbox* temptemp_kalmanbox;

    		while(headcount!=NULL){
        		int headnumber=headcount->data;

        		Boxflow nullflow=putNullInsideBox();
        		if(box_full[headnumber].objectIndex==0 && frame_num>=150){
        			printf("Problems!\n");
        			printf("headnumber: %i\n", headnumber);
        		}

        		else{
            		temptemp_kalmanbox=hashsearch(hashArray, box_full[headnumber].objectIndex)->element;

            		if((temptemp_kalmanbox->clock)>=3){
            			saveUnmatched(pre_im_frame, box_full[headnumber], box_Adfull_size);

            		}

            		else{
            			//box_full[headnumber].
            			printf("\t object: %i (cell: %i) does not have any match\n", box_full[headnumber].objectIndex, headnumber);
            			cvReleaseKalman(&(temptemp_kalmanbox->kalmanfilter));
            			hashdelete(hashArray, box_full[headnumber].objectIndex);

            		}


        		}


        		box_full[headnumber]=nullflow;
        		headcount=headcount->next;
    		}
    		dispose(headconstant);
    	}
    	printf("\t after processing unmatched\n");
    	hashdisplay(hashArray);
        //printf("Process Current Unmatched Objects in %lf seconds\n", sec(clock()-time4));



        printf("\n6. Temporary Variable Clean-up: \n");
        //time4=clock();
    	//TODO: Optimize later
    	printf("\t All Detection from idx_store: ");
    	int pp;
      	for(pp=0;pp<object_num;pp++){
      		printf("%i ",idx_store[pp]);
    		box_full[idx_store[pp]]=box_tempfull[idx_store[pp]];
    		Boxflow nullflow=putNullInsideBox();
    		box_tempfull[idx_store[pp]]=nullflow;
    	}
      	printf("\n");


		snode* head=NULL;
		head=prepend(head, MISS);
		headconstant=head;

		printf("\t Valid Detection from idx_tempprestore: ");
    	int xx;
    	for(xx=0;xx<40;xx++){
    		if(idx_tempprestore[xx]!=0){
    			printf("%i ",idx_tempprestore[xx]);
    			head=append(head,idx_tempprestore[xx]);
    		}
    		idx_store[xx]=0;
    		idx_tempprestore[xx]=0;

    	}
    	printf("\n");


    	int iic;
    	for(iic=0;iic<num;iic++){
    		memset(box_para[iic], 0, 13 * sizeof(int));
    	}

        //printf("Temporary Variable Clean-up in %lf seconds\n", sec(clock()-time4));




        if(saveOpticalflow){
        	CvSize size;{size.width = pre_im_frame->width, size.height = pre_im_frame->height;}
        	static CvVideoWriter* opticalflow_video = NULL;
        	if (opticalflow_video == NULL)
        	{
        		const char* output_name = "opticalflow.avi";
        		opticalflow_video = cvCreateVideoWriter(output_name, CV_FOURCC('D', 'I', 'V', 'X'), 5, size, 1);
        	}
        	cvWriteFrame(opticalflow_video, pre_im_frame);
        }
        cvReleaseImage(&pre_im_frame);
        object_prenum=object_num;
        object_num=0;

    }




    printf("\n7. Detection: \n");
    //TIME:
    //clock_t time5=clock();
//    int j;box_para[idx_store[p]][9]!=0 && box_para[idx_store[p]][9]!=2 && box_para[idx_store[p]][9]!=3
//    for(j=0; j<1; j++){
//
//    	//TODO: Need to check if it is empty
//        int left  = box_Adfull[j].left;
//        int top   = box_Adfull[j].top;
//        int right = box_Adfull[j].left+box_Adfull[j].width;
//        int bot   = box_Adfull[j].top+box_Adfull[j].height;
//        int width = im.h * .012*0.25;
//
//        draw_box_width(im, left, top, right, bot, width, 0.5, 0.5, 0.5);
//    }
    //int temprow=10;
    //int temprow=0;
    //int temprow=7;
    //int temprow=20; int tempcol1=20; int tempcol2=40;

    //int temprow=10; int temprow2=42; int tempcol1=10; int tempcol2=66;
    int colcol=sqrt(num/5);
    int objectIndex2=0;
    for(i = 0; i < num; ++i){
        int class = max_index(probs[i], classes);
        //printf("%0.2f, %i, %0.3f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f\n", probs[i][0], probs[i][1],probs[i][2],probs[i][3], probs[i][4], probs[i][5], probs[i][6], probs[i][7]);
        float prob = probs[i][class];

//        if((i>=0 && i<temprow*colcol)||(i>=num/5*1 && i<temprow*colcol+num/5*1)||(i>=num/5*2 && i<temprow*colcol+num/5*2)||(i>num/5*3 && i<temprow*colcol+num/5*3)||(i>=num/5*4 && i<temprow*colcol+num/5*4)){
//        	continue;
//        }

        //For drawing bboxes
//        if(frame_num==debug_frame){
//            int tempi;
//            int center=215;
//            //191
//            for(tempi=0;tempi<5;tempi++){
////
////            	probs[center-27+(676)*tempi][3]=1;
////            	probs[center-26+(676)*tempi][3]=1;
////            	probs[center-25+(676)*tempi][3]=1;
//
//            	//probs[center-1+(676)*tempi][3]=1;
//                //probs[center+(676)*tempi][3]=1;
////                probs[center+1+(676)*tempi][3]=1;
//
////                probs[center+25+(676)*tempi][3]=1;
////                probs[center+26+(676)*tempi][3]=1;
////                probs[center+27+(676)*tempi][3]=1;
//            }
//        }



        if(prob > thresh){

        	//width determines the thickness of the bounding boxes
            int width = im.h * .012*0.25;

            if(0){
                width = pow(prob, 1./2.)*10+1;
                alphabet = 0;
            }

            int offset = class*123457 % classes;
            float red = get_color(2,offset,classes);
            float green = get_color(1,offset,classes);
            float blue = get_color(0,offset,classes);
            float rgb[3];


            rgb[0] = red;
            rgb[1] = green;
            rgb[2] = blue;
            box b = boxes[i];

            int left  = (b.x-b.w/2.)*im.w;
            int right = (b.x+b.w/2.)*im.w;
            int top   = (b.y-b.h/2.)*im.h;
            int bot   = (b.y+b.h/2.)*im.h;

            if(left < 0) left = 0;
            if(right > im.w-1) right = im.w-1;
            if(top < 0) top = 0;
            if(bot > im.h-1) bot = im.h-1;


//			FILE *f = fopen("file.txt", "a");
//			fprintf(f, "%s, %.0f%, %d, %d, %d, %d \n", names[class], prob * 100, left, right, top, bot);
//			fclose(f);

            //Export the cell information to variables
//            float row=probs[i][81];
//            float col=probs[i][82];
//            float nn=probs[i][83];

            float row=probs[i][8+1];
            float col=probs[i][8+2];

//            if(col>=tempcol2 || col<=tempcol1 || row>temprow2){
//            	continue;
//            }
            float nn=probs[i][8+3];

            //ADDED: If the bounding box exceeds certain size, we discard it
            if((right-left)>(im.w*0.2) || (bot-top)>(im.h*0.2)){
            	printf("\t False Positive: Giant Detection, width: %i, height: %i\n", (right-left), (bot-top));
            	continue;
            }


            char fr[sizeof(frame_num)];
            sprintf(fr, "%d", frame_num);
            char rw[sizeof(row)];
            sprintf(rw, "%0.0f", row);
            char cl[sizeof(col)];
            sprintf(cl,"%0.0f", col);
            char n[sizeof(nn)];
            sprintf(n,"%0.0f", nn);
            char obj[sizeof(objectIndex2)];
            sprintf(obj,"%d", objectIndex2);


            char classtype[sizeof(names[class])];
            sprintf(classtype, "%s", names[class]);

            box_para[i][0]=left;
            box_para[i][1]=top;
            box_para[i][2]=(right-left);
            box_para[i][3]=(bot-top);
            box_para[i][4]=class;
            box_para[i][5]=prob*100;
            box_para[i][6]=(int)row;
            box_para[i][7]=(int)col;
            box_para[i][8]=(int)nn;
            box_para[i][9]=objectIndex2;

            int RED=cvRound(red*255);
            int GREEN=cvRound(green*255);
            int BLUE=cvRound(blue*255);

            box_para[i][10]=RED;
            box_para[i][11]=GREEN;
            box_para[i][12]=BLUE;

            idx_store[idx_count]=i;
            idx_count=idx_count+1;
            object_num=object_num+1;

            printf("\t Frame: %s Class: %s %0.f%%, index: %i, row: %0.0f, col: %0.0f, n:%0.0f objectIndex: %d\n", fr, names[class], prob*100, i, probs[i][9], probs[i][10], probs[i][11], objectIndex2);
            draw_box_width(im, left, top, right, bot, width, red, green, blue);
            if (alphabet) {
            	//Print the frame number, bbox number, object number to each label
            	char label_frame_bbox[sizeof(fr)+sizeof(names[class])+sizeof(row)+sizeof(cl)+sizeof(n)+sizeof(obj)];
            	//char label_frame_bbox[sizeof(fr)+sizeof(names[class])];
                strcpy( label_frame_bbox, names[class] );
                strcat( label_frame_bbox, "_" );
                strcat( label_frame_bbox, fr );
                strcat( label_frame_bbox, "_" );
                strcat( label_frame_bbox, rw );
                strcat( label_frame_bbox, "_" );
                strcat( label_frame_bbox, cl );
                strcat( label_frame_bbox, "_" );
                strcat( label_frame_bbox, n );
                strcat( label_frame_bbox, "_" );
                strcat( label_frame_bbox, obj );

                image label = get_label(alphabet, label_frame_bbox, (im.h*.03*0.5)/10);
                draw_label(im, top + width, left, label, rgb);
                free_image(label);
            }

        }
        	objectIndex2=objectIndex2+1;
    }

//	FILE *f = fopen("file.txt", "a");
//	fprintf(f, "next_frame\n");
//	fclose(f);

    free_image(pre_im);
    pre_im=copy_image(screenshot);
    free_image(screenshot);
    frame_num=frame_num+1;

	//printf("Detections: %lf seconds\n", sec(clock()-time5));


	//printf("Frame: %lf seconds\n", sec(clock()-time1));
	printf("Finish detecting\n");
}

void transpose_image(image im)
{
    assert(im.w == im.h);
    int n, m;
    int c;
    for(c = 0; c < im.c; ++c){
        for(n = 0; n < im.w-1; ++n){
            for(m = n + 1; m < im.w; ++m){
                float swap = im.data[m + im.w*(n + im.h*c)];
                im.data[m + im.w*(n + im.h*c)] = im.data[n + im.w*(m + im.h*c)];
                im.data[n + im.w*(m + im.h*c)] = swap;
            }
        }
    }
}

void rotate_image_cw(image im, int times)
{
    assert(im.w == im.h);
    times = (times + 400) % 4;
    int i, x, y, c;
    int n = im.w;
    for(i = 0; i < times; ++i){
        for(c = 0; c < im.c; ++c){
            for(x = 0; x < n/2; ++x){
                for(y = 0; y < (n-1)/2 + 1; ++y){
                    float temp = im.data[y + im.w*(x + im.h*c)];
                    im.data[y + im.w*(x + im.h*c)] = im.data[n-1-x + im.w*(y + im.h*c)];
                    im.data[n-1-x + im.w*(y + im.h*c)] = im.data[n-1-y + im.w*(n-1-x + im.h*c)];
                    im.data[n-1-y + im.w*(n-1-x + im.h*c)] = im.data[x + im.w*(n-1-y + im.h*c)];
                    im.data[x + im.w*(n-1-y + im.h*c)] = temp;
                }
            }
        }
    }
}

void flip_image(image a)
{
    int i,j,k;
    for(k = 0; k < a.c; ++k){
        for(i = 0; i < a.h; ++i){
            for(j = 0; j < a.w/2; ++j){
                int index = j + a.w*(i + a.h*(k));
                int flip = (a.w - j - 1) + a.w*(i + a.h*(k));
                float swap = a.data[flip];
                a.data[flip] = a.data[index];
                a.data[index] = swap;
            }
        }
    }
}

image image_distance(image a, image b)
{
    int i,j;
    image dist = make_image(a.w, a.h, 1);
    for(i = 0; i < a.c; ++i){
        for(j = 0; j < a.h*a.w; ++j){
            dist.data[j] += pow(a.data[i*a.h*a.w+j]-b.data[i*a.h*a.w+j],2);
        }
    }
    for(j = 0; j < a.h*a.w; ++j){
        dist.data[j] = sqrt(dist.data[j]);
    }
    return dist;
}

void ghost_image(image source, image dest, int dx, int dy)
{
    int x,y,k;
    float max_dist = sqrt((-source.w/2. + .5)*(-source.w/2. + .5));
    for(k = 0; k < source.c; ++k){
        for(y = 0; y < source.h; ++y){
            for(x = 0; x < source.w; ++x){
                float dist = sqrt((x - source.w/2. + .5)*(x - source.w/2. + .5) + (y - source.h/2. + .5)*(y - source.h/2. + .5));
                float alpha = (1 - dist/max_dist);
                if(alpha < 0) alpha = 0;
                float v1 = get_pixel(source, x,y,k);
                float v2 = get_pixel(dest, dx+x,dy+y,k);
                float val = alpha*v1 + (1-alpha)*v2;
                set_pixel(dest, dx+x, dy+y, k, val);
            }
        }
    }
}

void embed_image(image source, image dest, int dx, int dy)
{
    int x,y,k;
    for(k = 0; k < source.c; ++k){
        for(y = 0; y < source.h; ++y){
            for(x = 0; x < source.w; ++x){
                float val = get_pixel(source, x,y,k);
                set_pixel(dest, dx+x, dy+y, k, val);
            }
        }
    }
}

image collapse_image_layers(image source, int border)
{
    int h = source.h;
    h = (h+border)*source.c - border;
    image dest = make_image(source.w, h, 1);
    int i;
    for(i = 0; i < source.c; ++i){
        image layer = get_image_layer(source, i);
        int h_offset = i*(source.h+border);
        embed_image(layer, dest, 0, h_offset);
        free_image(layer);
    }
    return dest;
}

void constrain_image(image im)
{
    int i;
    for(i = 0; i < im.w*im.h*im.c; ++i){
        if(im.data[i] < 0) im.data[i] = 0;
        if(im.data[i] > 1) im.data[i] = 1;
    }
}

void normalize_image(image p)
{
    int i;
    float min = 9999999;
    float max = -999999;

    for(i = 0; i < p.h*p.w*p.c; ++i){
        float v = p.data[i];
        if(v < min) min = v;
        if(v > max) max = v;
    }
    if(max - min < .000000001){
        min = 0;
        max = 1;
    }
    for(i = 0; i < p.c*p.w*p.h; ++i){
        p.data[i] = (p.data[i] - min)/(max-min);
    }
}

void normalize_image2(image p)
{
    float *min = calloc(p.c, sizeof(float));
    float *max = calloc(p.c, sizeof(float));
    int i,j;
    for(i = 0; i < p.c; ++i) min[i] = max[i] = p.data[i*p.h*p.w];

    for(j = 0; j < p.c; ++j){
        for(i = 0; i < p.h*p.w; ++i){
            float v = p.data[i+j*p.h*p.w];
            if(v < min[j]) min[j] = v;
            if(v > max[j]) max[j] = v;
        }
    }
    for(i = 0; i < p.c; ++i){
        if(max[i] - min[i] < .000000001){
            min[i] = 0;
            max[i] = 1;
        }
    }
    for(j = 0; j < p.c; ++j){
        for(i = 0; i < p.w*p.h; ++i){
            p.data[i+j*p.h*p.w] = (p.data[i+j*p.h*p.w] - min[j])/(max[j]-min[j]);
        }
    }
    free(min);
    free(max);
}

void copy_image_into(image src, image dest)
{
    memcpy(dest.data, src.data, src.h*src.w*src.c*sizeof(float));
}

image copy_image(image p)
{
    image copy = p;
    copy.data = calloc(p.h*p.w*p.c, sizeof(float));
    memcpy(copy.data, p.data, p.h*p.w*p.c*sizeof(float));
    return copy;
}

void rgbgr_image(image im)
{
    int i;
    for(i = 0; i < im.w*im.h; ++i){
        float swap = im.data[i];
        im.data[i] = im.data[i+im.w*im.h*2];
        im.data[i+im.w*im.h*2] = swap;
    }
}

//ADDED: Draw arrow on the image REF: http://mlikihazar.blogspot.ca/2013/02/draw-arrow-opencv.html
void drawArrow(IplImage *image, CvPoint p, CvPoint q, CvScalar color, int arrowMagnitude, int thickness, int line_type, int shift)
{
    //Draw the principle line

    cvLine(image, p, q, color, thickness, line_type, shift);
    const double PI = 3.141592653;
    //compute the angle alpha
    double angle = atan2((double)p.y-q.y, (double)p.x-q.x);
    //compute the coordinates of the first segment
    p.x = (int) ( q.x +  arrowMagnitude * cos(angle + PI/8));
    p.y = (int) ( q.y +  arrowMagnitude * sin(angle + PI/8));
    //Draw the first segment
    cvLine(image, p, q, color, thickness, line_type, shift);
    //compute the coordinates of the second segment
    p.x = (int) ( q.x +  arrowMagnitude * cos(angle - PI/8));
    p.y = (int) ( q.y +  arrowMagnitude * sin(angle - PI/8));


    //Draw the second segment
    cvLine(image, p, q, color, thickness, line_type, shift);

}


//ADDED: Convert image object to IplImage object
IplImage* image_convert_IplImage(image p, IplImage *disp){
    int step = disp->widthStep;
    int x,y,k;
    for(y = 0; y < p.h; ++y){
        for(x = 0; x < p.w; ++x){
            for(k= 0; k < p.c; ++k){
                disp->imageData[y*step + x*p.c + k] = (unsigned char)(get_pixel(p,x,y,k)*255);

            }
        }
    }

    cvCvtColor(disp, disp, CV_RGB2BGR);
	return disp;

}


Boxflow putFlowInsideBox(Opticalflow vector, int left, int top, int width, int height, int classtype, float prob, int row, int col, int nn, int objectIndex){
	Boxflow out;
	out.flow=vector;
	out.left=left;
	out.top=top;
	out.width=width;
	out.height=height;
	out.classtype=classtype;
	out.prob=prob;
	out.row=row;
	out.col=col;
	out.nn=nn;
	out.objectIndex=objectIndex;

	return out;
}

Boxflow putNullInsideBox(){
	Boxflow out;
	Opticalflow vector=create_opticalflow(cvPoint(0,0), cvPoint(0,0), cvPoint(0,0), cvPoint(0,0));
	out.flow=vector;
	out.left=0;
	out.top=0;
	out.width=0;
	out.height=0;
	out.classtype=0;
	out.prob=0;
	out.row=0;
	out.col=0;
	out.nn=0;
	out.objectIndex=0;

	return out;
}


#ifdef OPENCV
void show_image_cv(image p, const char *name, IplImage *disp)
{
    int x,y,k;
    if(p.c == 3) rgbgr_image(p);
    //normalize_image(copy);

    char buff[256];
    //sprintf(buff, "%s (%d)", name, windows);
    sprintf(buff, "%s", name);

    int step = disp->widthStep;
    cvNamedWindow(buff, CV_WINDOW_NORMAL); 
    //cvMoveWindow(buff, 100*(windows%10) + 200*(windows/10), 100*(windows%10));
    ++windows;
    for(y = 0; y < p.h; ++y){
        for(x = 0; x < p.w; ++x){
            for(k= 0; k < p.c; ++k){
                disp->imageData[y*step + x*p.c + k] = (unsigned char)(get_pixel(p,x,y,k)*255);
            }
        }
    }
    if(0){
        int w = 448;
        int h = w*p.h/p.w;
        if(h > 1000){
            h = 1000;
            w = h*p.w/p.h;
        }
        IplImage *buffer = disp;
        disp = cvCreateImage(cvSize(w, h), buffer->depth, buffer->nChannels);
        cvResize(buffer, disp, CV_INTER_LINEAR);
        cvReleaseImage(&buffer);
    }

    cvShowImage(buff, disp);

    //ADDED: Save the demo input video to output
    if(saveDetection){
    	CvSize size;{size.width = disp->width, size.height = disp->height;}
    	static CvVideoWriter* output_video = NULL;
    	if (output_video == NULL)
    	{
    		const char* output_name = "detection.avi";
    		output_video = cvCreateVideoWriter(output_name, CV_FOURCC('D', 'I', 'V', 'X'), 25, size, 1);
    	}
    	cvWriteFrame(output_video, disp);
    }

}
#endif

void show_image(image p, const char *name)
{
#ifdef OPENCV
    IplImage *disp = cvCreateImage(cvSize(p.w,p.h), IPL_DEPTH_8U, p.c);
    image copy = copy_image(p);
    constrain_image(copy);
    show_image_cv(copy, name, disp);
    free_image(copy);
    cvReleaseImage(&disp);
#else
    fprintf(stderr, "Not compiled with OpenCV, saving to %s.png instead\n", name);
    save_image(p, name);
#endif
}

#ifdef OPENCV

void ipl_into_image(IplImage* src, image im)
{
    unsigned char *data = (unsigned char *)src->imageData;
    int h = src->height;
    int w = src->width;
    int c = src->nChannels;
    int step = src->widthStep;
    int i, j, k;

    for(i = 0; i < h; ++i){
        for(k= 0; k < c; ++k){
            for(j = 0; j < w; ++j){
                im.data[k*w*h + i*w + j] = data[i*step + j*c + k]/255.;
            }
        }
    }
}

image ipl_to_image(IplImage* src)
{
    int h = src->height;
    int w = src->width;
    int c = src->nChannels;
    image out = make_image(w, h, c);
    ipl_into_image(src, out);
    return out;
}

image load_image_cv(char *filename, int channels)
{
    IplImage* src = 0;
    int flag = -1;
    if (channels == 0) flag = -1;
    else if (channels == 1) flag = 0;
    else if (channels == 3) flag = 1;
    else {
        fprintf(stderr, "OpenCV can't force load with %d channels\n", channels);
    }

    if( (src = cvLoadImage(filename, flag)) == 0 )
    {
        fprintf(stderr, "Cannot load image \"%s\"\n", filename);
        char buff[256];
        sprintf(buff, "echo %s >> bad.list", filename);
        system(buff);
        return make_image(10,10,3);
        //exit(0);
    }
    image out = ipl_to_image(src);
    cvReleaseImage(&src);
    rgbgr_image(out);
    return out;
}

void flush_stream_buffer(CvCapture *cap, int n)
{
    int i;
    for(i = 0; i < n; ++i) {
        cvQueryFrame(cap);
    }
}

image get_image_from_stream(CvCapture *cap)
{
    IplImage* src = cvQueryFrame(cap);
    if (!src) return make_empty_image(0,0,0);
    image im = ipl_to_image(src);
    rgbgr_image(im);
    return im;
}

int fill_image_from_stream(CvCapture *cap, image im)
{
    IplImage* src = cvQueryFrame(cap);
    if (!src) return 0;
    ipl_into_image(src, im);
    rgbgr_image(im);
    return 1;
}

void save_image_jpg(image p, const char *name)
{
    image copy = copy_image(p);
    if(p.c == 3) rgbgr_image(copy);
    int x,y,k;

    char buff[256];
    sprintf(buff, "%s.jpg", name);

    IplImage *disp = cvCreateImage(cvSize(p.w,p.h), IPL_DEPTH_8U, p.c);
    int step = disp->widthStep;
    for(y = 0; y < p.h; ++y){
        for(x = 0; x < p.w; ++x){
            for(k= 0; k < p.c; ++k){
                disp->imageData[y*step + x*p.c + k] = (unsigned char)(get_pixel(copy,x,y,k)*255);
            }
        }
    }
    cvSaveImage(buff, disp,0);
    cvReleaseImage(&disp);
    free_image(copy);
}
#endif

void save_image_png(image im, const char *name)
{
    char buff[256];
    //sprintf(buff, "%s (%d)", name, windows);
    sprintf(buff, "%s.png", name);
    unsigned char *data = calloc(im.w*im.h*im.c, sizeof(char));
    int i,k;
    for(k = 0; k < im.c; ++k){
        for(i = 0; i < im.w*im.h; ++i){
            data[i*im.c+k] = (unsigned char) (255*im.data[i + k*im.w*im.h]);
        }
    }
    int success = stbi_write_png(buff, im.w, im.h, im.c, data, im.w*im.c);
    free(data);
    if(!success) fprintf(stderr, "Failed to write image %s\n", buff);
}

void save_image(image im, const char *name)
{
#ifdef OPENCV
    save_image_jpg(im, name);
#else
    save_image_png(im, name);
#endif
}


void show_image_layers(image p, char *name)
{
    int i;
    char buff[256];
    for(i = 0; i < p.c; ++i){
        sprintf(buff, "%s - Layer %d", name, i);
        image layer = get_image_layer(p, i);
        show_image(layer, buff);
        free_image(layer);
    }
}

void show_image_collapsed(image p, char *name)
{
    image c = collapse_image_layers(p, 1);
    show_image(c, name);
    free_image(c);
}

image make_empty_image(int w, int h, int c)
{
    image out;
    out.data = 0;
    out.h = h;
    out.w = w;
    out.c = c;
    return out;

}

int computeDegree(double sum_p0_x, double sum_p0_y, double sum_p1_x, double sum_p1_y){
	if(sum_p0_x==sum_p1_x && sum_p0_y==sum_p1_y){
		return 0;
	}
    int degree=atan((sum_p1_y-sum_p0_y)/(sum_p1_x-sum_p0_x))*(180.0/3.1415926);
    if (degree>0){
    	if (sum_p1_x>sum_p0_x){
    		degree=360-degree;
    	}
    	else if (sum_p1_x<sum_p0_x){
    		degree=180-degree;
    	}
    	else{
    		degree=270;
    	}
    }

    else if (degree<0){
    	if (sum_p1_y<sum_p0_y){
    		degree=-degree;
    	}
    	else if(sum_p1_x<sum_p0_x){
    		degree=180-degree;
    	}
    	else{
    		degree=90;
    	}

    }
    else{
    	if (sum_p1_x>sum_p0_x){
    		degree=0;
    	}
    	else if (sum_p1_x<sum_p0_x){
    		degree=180;

    	}
    }
    return degree;
}

int computeMagnitude(double sum_p0_x, double sum_p0_y, double sum_p1_x, double sum_p1_y){
	int magnitude=sqrt(pow(abs(sum_p0_x-sum_p1_x), 2)+pow(abs(sum_p0_y-sum_p1_y),2));
	return magnitude;
}




image make_image(int w, int h, int c)
{
    image out = make_empty_image(w,h,c);
    out.data = calloc(h*w*c, sizeof(float));
    return out;
}

image make_random_image(int w, int h, int c)
{
    image out = make_empty_image(w,h,c);
    out.data = calloc(h*w*c, sizeof(float));
    int i;
    for(i = 0; i < w*h*c; ++i){
        out.data[i] = (rand_normal() * .25) + .5;
    }
    return out;
}

image float_to_image(int w, int h, int c, float *data)
{
    image out = make_empty_image(w,h,c);
    out.data = data;
    return out;
}

void place_image(image im, int w, int h, int dx, int dy, image canvas)
{
    int x, y, c;
    for(c = 0; c < im.c; ++c){
        for(y = 0; y < h; ++y){
            for(x = 0; x < w; ++x){
                int rx = ((float)x / w) * im.w;
                int ry = ((float)y / h) * im.h;
                float val = bilinear_interpolate(im, rx, ry, c);
                set_pixel(canvas, x + dx, y + dy, c, val);
            }
        }
    }
}

image center_crop_image(image im, int w, int h)
{
    int m = (im.w < im.h) ? im.w : im.h;   
    image c = crop_image(im, (im.w - m) / 2, (im.h - m)/2, m, m);
    image r = resize_image(c, w, h);
    free_image(c);
    return r;
}

image rotate_crop_image(image im, float rad, float s, int w, int h, float dx, float dy, float aspect)
{
    int x, y, c;
    float cx = im.w/2.;
    float cy = im.h/2.;
    image rot = make_image(w, h, im.c);
    for(c = 0; c < im.c; ++c){
        for(y = 0; y < h; ++y){
            for(x = 0; x < w; ++x){
                float rx = cos(rad)*((x - w/2.)/s*aspect + dx/s*aspect) - sin(rad)*((y - h/2.)/s + dy/s) + cx;
                float ry = sin(rad)*((x - w/2.)/s*aspect + dx/s*aspect) + cos(rad)*((y - h/2.)/s + dy/s) + cy;
                float val = bilinear_interpolate(im, rx, ry, c);
                set_pixel(rot, x, y, c, val);
            }
        }
    }
    return rot;
}

image rotate_image(image im, float rad)
{
    int x, y, c;
    float cx = im.w/2.;
    float cy = im.h/2.;
    image rot = make_image(im.w, im.h, im.c);
    for(c = 0; c < im.c; ++c){
        for(y = 0; y < im.h; ++y){
            for(x = 0; x < im.w; ++x){
                float rx = cos(rad)*(x-cx) - sin(rad)*(y-cy) + cx;
                float ry = sin(rad)*(x-cx) + cos(rad)*(y-cy) + cy;
                float val = bilinear_interpolate(im, rx, ry, c);
                set_pixel(rot, x, y, c, val);
            }
        }
    }
    return rot;
}

void fill_image(image m, float s)
{
    int i;
    for(i = 0; i < m.h*m.w*m.c; ++i) m.data[i] = s;
}

void translate_image(image m, float s)
{
    int i;
    for(i = 0; i < m.h*m.w*m.c; ++i) m.data[i] += s;
}

void scale_image(image m, float s)
{
    int i;
    for(i = 0; i < m.h*m.w*m.c; ++i) m.data[i] *= s;
}

image crop_image(image im, int dx, int dy, int w, int h)
{
    image cropped = make_image(w, h, im.c);
    int i, j, k;
    for(k = 0; k < im.c; ++k){
        for(j = 0; j < h; ++j){
            for(i = 0; i < w; ++i){
                int r = j + dy;
                int c = i + dx;
                float val = 0;
                r = constrain_int(r, 0, im.h-1);
                c = constrain_int(c, 0, im.w-1);
                val = get_pixel(im, c, r, k);
                set_pixel(cropped, i, j, k, val);
            }
        }
    }
    return cropped;
}

int best_3d_shift_r(image a, image b, int min, int max)
{
    if(min == max) return min;
    int mid = floor((min + max) / 2.);
    image c1 = crop_image(b, 0, mid, b.w, b.h);
    image c2 = crop_image(b, 0, mid+1, b.w, b.h);
    float d1 = dist_array(c1.data, a.data, a.w*a.h*a.c, 10);
    float d2 = dist_array(c2.data, a.data, a.w*a.h*a.c, 10);
    free_image(c1);
    free_image(c2);
    if(d1 < d2) return best_3d_shift_r(a, b, min, mid);
    else return best_3d_shift_r(a, b, mid+1, max);
}

int best_3d_shift(image a, image b, int min, int max)
{
    int i;
    int best = 0;
    float best_distance = FLT_MAX;
    for(i = min; i <= max; i += 2){
        image c = crop_image(b, 0, i, b.w, b.h);
        float d = dist_array(c.data, a.data, a.w*a.h*a.c, 100);
        if(d < best_distance){
            best_distance = d;
            best = i;
        }
        printf("%d %f\n", i, d);
        free_image(c);
    }
    return best;
}

void composite_3d(char *f1, char *f2, char *out, int delta)
{
    if(!out) out = "out";
    image a = load_image(f1, 0,0,0);
    image b = load_image(f2, 0,0,0);
    int shift = best_3d_shift_r(a, b, -a.h/100, a.h/100);

    image c1 = crop_image(b, 10, shift, b.w, b.h);
    float d1 = dist_array(c1.data, a.data, a.w*a.h*a.c, 100);
    image c2 = crop_image(b, -10, shift, b.w, b.h);
    float d2 = dist_array(c2.data, a.data, a.w*a.h*a.c, 100);

    if(d2 < d1 && 0){
        image swap = a;
        a = b;
        b = swap;
        shift = -shift;
        printf("swapped, %d\n", shift);
    }
    else{
        printf("%d\n", shift);
    }

    image c = crop_image(b, delta, shift, a.w, a.h);
    int i;
    for(i = 0; i < c.w*c.h; ++i){
        c.data[i] = a.data[i];
    }
#ifdef OPENCV
    save_image_jpg(c, out);
#else
    save_image(c, out);
#endif
}

void letterbox_image_into(image im, int w, int h, image boxed)
{
    int new_w = im.w;
    int new_h = im.h;
    if (((float)w/im.w) < ((float)h/im.h)) {
        new_w = w;
        new_h = (im.h * w)/im.w;
    } else {
        new_h = h;
        new_w = (im.w * h)/im.h;
    }
    image resized = resize_image(im, new_w, new_h);
    embed_image(resized, boxed, (w-new_w)/2, (h-new_h)/2); 
    free_image(resized);
}

image letterbox_image(image im, int w, int h)
{
    int new_w = im.w;
    int new_h = im.h;
    if (((float)w/im.w) < ((float)h/im.h)) {
        new_w = w;
        new_h = (im.h * w)/im.w;
    } else {
        new_h = h;
        new_w = (im.w * h)/im.h;
    }
    image resized = resize_image(im, new_w, new_h);
    image boxed = make_image(w, h, im.c);
    fill_image(boxed, .5);
    //int i;
    //for(i = 0; i < boxed.w*boxed.h*boxed.c; ++i) boxed.data[i] = 0;
    embed_image(resized, boxed, (w-new_w)/2, (h-new_h)/2); 
    free_image(resized);
    return boxed;
}

image resize_max(image im, int max)
{
    int w = im.w;
    int h = im.h;
    if(w > h){
        h = (h * max) / w;
        w = max;
    } else {
        w = (w * max) / h;
        h = max;
    }
    if(w == im.w && h == im.h) return im;
    image resized = resize_image(im, w, h);
    return resized;
}

image resize_min(image im, int min)
{
    int w = im.w;
    int h = im.h;
    if(w < h){
        h = (h * min) / w;
        w = min;
    } else {
        w = (w * min) / h;
        h = min;
    }
    if(w == im.w && h == im.h) return im;
    image resized = resize_image(im, w, h);
    return resized;
}

image random_crop_image(image im, int w, int h)
{
    int dx = rand_int(0, im.w - w);
    int dy = rand_int(0, im.h - h);
    image crop = crop_image(im, dx, dy, w, h);
    return crop;
}

image random_augment_image(image im, float angle, float aspect, int low, int high, int size)
{
    aspect = rand_scale(aspect);
    int r = rand_int(low, high);
    int min = (im.h < im.w*aspect) ? im.h : im.w*aspect;
    float scale = (float)r / min;

    float rad = rand_uniform(-angle, angle) * TWO_PI / 360.;

    float dx = (im.w*scale/aspect - size) / 2.;
    float dy = (im.h*scale - size) / 2.;
    if(dx < 0) dx = 0;
    if(dy < 0) dy = 0;
    dx = rand_uniform(-dx, dx);
    dy = rand_uniform(-dy, dy);

    image crop = rotate_crop_image(im, rad, scale, size, size, dx, dy, aspect);

    return crop;
}

float three_way_max(float a, float b, float c)
{
    return (a > b) ? ( (a > c) ? a : c) : ( (b > c) ? b : c) ;
}

float three_way_min(float a, float b, float c)
{
    return (a < b) ? ( (a < c) ? a : c) : ( (b < c) ? b : c) ;
}

void yuv_to_rgb(image im)
{
    assert(im.c == 3);
    int i, j;
    float r, g, b;
    float y, u, v;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            y = get_pixel(im, i , j, 0);
            u = get_pixel(im, i , j, 1);
            v = get_pixel(im, i , j, 2);

            r = y + 1.13983*v;
            g = y + -.39465*u + -.58060*v;
            b = y + 2.03211*u;

            set_pixel(im, i, j, 0, r);
            set_pixel(im, i, j, 1, g);
            set_pixel(im, i, j, 2, b);
        }
    }
}

void rgb_to_yuv(image im)
{
    assert(im.c == 3);
    int i, j;
    float r, g, b;
    float y, u, v;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            r = get_pixel(im, i , j, 0);
            g = get_pixel(im, i , j, 1);
            b = get_pixel(im, i , j, 2);

            y = .299*r + .587*g + .114*b;
            u = -.14713*r + -.28886*g + .436*b;
            v = .615*r + -.51499*g + -.10001*b;

            set_pixel(im, i, j, 0, y);
            set_pixel(im, i, j, 1, u);
            set_pixel(im, i, j, 2, v);
        }
    }
}

// http://www.cs.rit.edu/~ncs/color/t_convert.html
void rgb_to_hsv(image im)
{
    assert(im.c == 3);
    int i, j;
    float r, g, b;
    float h, s, v;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            r = get_pixel(im, i , j, 0);
            g = get_pixel(im, i , j, 1);
            b = get_pixel(im, i , j, 2);
            float max = three_way_max(r,g,b);
            float min = three_way_min(r,g,b);
            float delta = max - min;
            v = max;
            if(max == 0){
                s = 0;
                h = 0;
            }else{
                s = delta/max;
                if(r == max){
                    h = (g - b) / delta;
                } else if (g == max) {
                    h = 2 + (b - r) / delta;
                } else {
                    h = 4 + (r - g) / delta;
                }
                if (h < 0) h += 6;
                h = h/6.;
            }
            set_pixel(im, i, j, 0, h);
            set_pixel(im, i, j, 1, s);
            set_pixel(im, i, j, 2, v);
        }
    }
}

void hsv_to_rgb(image im)
{
    assert(im.c == 3);
    int i, j;
    float r, g, b;
    float h, s, v;
    float f, p, q, t;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            h = 6 * get_pixel(im, i , j, 0);
            s = get_pixel(im, i , j, 1);
            v = get_pixel(im, i , j, 2);
            if (s == 0) {
                r = g = b = v;
            } else {
                int index = floor(h);
                f = h - index;
                p = v*(1-s);
                q = v*(1-s*f);
                t = v*(1-s*(1-f));
                if(index == 0){
                    r = v; g = t; b = p;
                } else if(index == 1){
                    r = q; g = v; b = p;
                } else if(index == 2){
                    r = p; g = v; b = t;
                } else if(index == 3){
                    r = p; g = q; b = v;
                } else if(index == 4){
                    r = t; g = p; b = v;
                } else {
                    r = v; g = p; b = q;
                }
            }
            set_pixel(im, i, j, 0, r);
            set_pixel(im, i, j, 1, g);
            set_pixel(im, i, j, 2, b);
        }
    }
}

void grayscale_image_3c(image im)
{
    assert(im.c == 3);
    int i, j, k;
    float scale[] = {0.299, 0.587, 0.114};
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            float val = 0;
            for(k = 0; k < 3; ++k){
                val += scale[k]*get_pixel(im, i, j, k);
            }
            im.data[0*im.h*im.w + im.w*j + i] = val;
            im.data[1*im.h*im.w + im.w*j + i] = val;
            im.data[2*im.h*im.w + im.w*j + i] = val;
        }
    }
}

image grayscale_image(image im)
{
    assert(im.c == 3);
    int i, j, k;
    image gray = make_image(im.w, im.h, 1);
    float scale[] = {0.299, 0.587, 0.114};
    for(k = 0; k < im.c; ++k){
        for(j = 0; j < im.h; ++j){
            for(i = 0; i < im.w; ++i){
                gray.data[i+im.w*j] += scale[k]*get_pixel(im, i, j, k);
            }
        }
    }
    return gray;
}

image threshold_image(image im, float thresh)
{
    int i;
    image t = make_image(im.w, im.h, im.c);
    for(i = 0; i < im.w*im.h*im.c; ++i){
        t.data[i] = im.data[i]>thresh ? 1 : 0;
    }
    return t;
}

image blend_image(image fore, image back, float alpha)
{
    assert(fore.w == back.w && fore.h == back.h && fore.c == back.c);
    image blend = make_image(fore.w, fore.h, fore.c);
    int i, j, k;
    for(k = 0; k < fore.c; ++k){
        for(j = 0; j < fore.h; ++j){
            for(i = 0; i < fore.w; ++i){
                float val = alpha * get_pixel(fore, i, j, k) + 
                    (1 - alpha)* get_pixel(back, i, j, k);
                set_pixel(blend, i, j, k, val);
            }
        }
    }
    return blend;
}

void scale_image_channel(image im, int c, float v)
{
    int i, j;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            float pix = get_pixel(im, i, j, c);
            pix = pix*v;
            set_pixel(im, i, j, c, pix);
        }
    }
}

void translate_image_channel(image im, int c, float v)
{
    int i, j;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            float pix = get_pixel(im, i, j, c);
            pix = pix+v;
            set_pixel(im, i, j, c, pix);
        }
    }
}

image binarize_image(image im)
{
    image c = copy_image(im);
    int i;
    for(i = 0; i < im.w * im.h * im.c; ++i){
        if(c.data[i] > .5) c.data[i] = 1;
        else c.data[i] = 0;
    }
    return c;
}

void saturate_image(image im, float sat)
{
    rgb_to_hsv(im);
    scale_image_channel(im, 1, sat);
    hsv_to_rgb(im);
    constrain_image(im);
}

void hue_image(image im, float hue)
{
    rgb_to_hsv(im);
    int i;
    for(i = 0; i < im.w*im.h; ++i){
        im.data[i] = im.data[i] + hue;
        if (im.data[i] > 1) im.data[i] -= 1;
        if (im.data[i] < 0) im.data[i] += 1;
    }
    hsv_to_rgb(im);
    constrain_image(im);
}

void exposure_image(image im, float sat)
{
    rgb_to_hsv(im);
    scale_image_channel(im, 2, sat);
    hsv_to_rgb(im);
    constrain_image(im);
}

void distort_image(image im, float hue, float sat, float val)
{
    rgb_to_hsv(im);
    scale_image_channel(im, 1, sat);
    scale_image_channel(im, 2, val);
    int i;
    for(i = 0; i < im.w*im.h; ++i){
        im.data[i] = im.data[i] + hue;
        if (im.data[i] > 1) im.data[i] -= 1;
        if (im.data[i] < 0) im.data[i] += 1;
    }
    hsv_to_rgb(im);
    constrain_image(im);
}

void random_distort_image(image im, float hue, float saturation, float exposure)
{
    float dhue = rand_uniform(-hue, hue);
    float dsat = rand_scale(saturation);
    float dexp = rand_scale(exposure);
    distort_image(im, dhue, dsat, dexp);
}

void saturate_exposure_image(image im, float sat, float exposure)
{
    rgb_to_hsv(im);
    scale_image_channel(im, 1, sat);
    scale_image_channel(im, 2, exposure);
    hsv_to_rgb(im);
    constrain_image(im);
}

float bilinear_interpolate(image im, float x, float y, int c)
{
    int ix = (int) floorf(x);
    int iy = (int) floorf(y);

    float dx = x - ix;
    float dy = y - iy;

    float val = (1-dy) * (1-dx) * get_pixel_extend(im, ix, iy, c) + 
        dy     * (1-dx) * get_pixel_extend(im, ix, iy+1, c) + 
        (1-dy) *   dx   * get_pixel_extend(im, ix+1, iy, c) +
        dy     *   dx   * get_pixel_extend(im, ix+1, iy+1, c);
    return val;
}

image resize_image(image im, int w, int h)
{
    image resized = make_image(w, h, im.c);   
    image part = make_image(w, im.h, im.c);
    int r, c, k;
    float w_scale = (float)(im.w - 1) / (w - 1);
    float h_scale = (float)(im.h - 1) / (h - 1);
    for(k = 0; k < im.c; ++k){
        for(r = 0; r < im.h; ++r){
            for(c = 0; c < w; ++c){
                float val = 0;
                if(c == w-1 || im.w == 1){
                    val = get_pixel(im, im.w-1, r, k);
                } else {
                    float sx = c*w_scale;
                    int ix = (int) sx;
                    float dx = sx - ix;
                    val = (1 - dx) * get_pixel(im, ix, r, k) + dx * get_pixel(im, ix+1, r, k);
                }
                set_pixel(part, c, r, k, val);
            }
        }
    }
    for(k = 0; k < im.c; ++k){
        for(r = 0; r < h; ++r){
            float sy = r*h_scale;
            int iy = (int) sy;
            float dy = sy - iy;
            for(c = 0; c < w; ++c){
                float val = (1-dy) * get_pixel(part, c, iy, k);
                set_pixel(resized, c, r, k, val);
            }
            if(r == h-1 || im.h == 1) continue;
            for(c = 0; c < w; ++c){
                float val = dy * get_pixel(part, c, iy+1, k);
                add_pixel(resized, c, r, k, val);
            }
        }
    }

    free_image(part);
    return resized;
}


void test_resize(char *filename)
{
    image im = load_image(filename, 0,0, 3);
    float mag = mag_array(im.data, im.w*im.h*im.c);
    printf("L2 Norm: %f\n", mag);
    image gray = grayscale_image(im);

    image c1 = copy_image(im);
    image c2 = copy_image(im);
    image c3 = copy_image(im);
    image c4 = copy_image(im);
    distort_image(c1, .1, 1.5, 1.5);
    distort_image(c2, -.1, .66666, .66666);
    distort_image(c3, .1, 1.5, .66666);
    distort_image(c4, .1, .66666, 1.5);


    show_image(im,   "Original");
    show_image(gray, "Gray");
    show_image(c1, "C1");
    show_image(c2, "C2");
    show_image(c3, "C3");
    show_image(c4, "C4");
#ifdef OPENCV
    while(1){
        image aug = random_augment_image(im, 0, .75, 320, 448, 320);
        show_image(aug, "aug");
        free_image(aug);


        float exposure = 1.15;
        float saturation = 1.15;
        float hue = .05;

        image c = copy_image(im);

        float dexp = rand_scale(exposure);
        float dsat = rand_scale(saturation);
        float dhue = rand_uniform(-hue, hue);

        distort_image(c, dhue, dsat, dexp);
        show_image(c, "rand");
        printf("%f %f %f\n", dhue, dsat, dexp);
        free_image(c);
        cvWaitKey(0);
    }
#endif
}


image load_image_stb(char *filename, int channels)
{
    int w, h, c;
    unsigned char *data = stbi_load(filename, &w, &h, &c, channels);
    if (!data) {
        fprintf(stderr, "Cannot load image \"%s\"\nSTB Reason: %s\n", filename, stbi_failure_reason());
        exit(0);
    }
    if(channels) c = channels;
    int i,j,k;
    image im = make_image(w, h, c);
    for(k = 0; k < c; ++k){
        for(j = 0; j < h; ++j){
            for(i = 0; i < w; ++i){
                int dst_index = i + w*j + w*h*k;
                int src_index = k + c*i + c*w*j;
                im.data[dst_index] = (float)data[src_index]/255.;
            }
        }
    }
    free(data);
    return im;
}

image load_image(char *filename, int w, int h, int c)
{
#ifdef OPENCV
    image out = load_image_cv(filename, c);
#else
    image out = load_image_stb(filename, c);
#endif

    if((h && w) && (h != out.h || w != out.w)){
        image resized = resize_image(out, w, h);
        free_image(out);
        out = resized;
    }
    return out;
}

image load_image_color(char *filename, int w, int h)
{
    return load_image(filename, w, h, 3);
}

image get_image_layer(image m, int l)
{
    image out = make_image(m.w, m.h, 1);
    int i;
    for(i = 0; i < m.h*m.w; ++i){
        out.data[i] = m.data[i+l*m.h*m.w];
    }
    return out;
}

float get_pixel(image m, int x, int y, int c)
{
    assert(x < m.w && y < m.h && c < m.c);
    return m.data[c*m.h*m.w + y*m.w + x];
}
float get_pixel_extend(image m, int x, int y, int c)
{
    if(x < 0) x = 0;
    if(x >= m.w) x = m.w-1;
    if(y < 0) y = 0;
    if(y >= m.h) y = m.h-1;
    if(c < 0 || c >= m.c) return 0;
    return get_pixel(m, x, y, c);
}
void set_pixel(image m, int x, int y, int c, float val)
{
    if (x < 0 || y < 0 || c < 0 || x >= m.w || y >= m.h || c >= m.c) return;
    assert(x < m.w && y < m.h && c < m.c);
    m.data[c*m.h*m.w + y*m.w + x] = val;
}
void add_pixel(image m, int x, int y, int c, float val)
{
    assert(x < m.w && y < m.h && c < m.c);
    m.data[c*m.h*m.w + y*m.w + x] += val;
}

void print_image(image m)
{
    int i, j, k;
    for(i =0 ; i < m.c; ++i){
        for(j =0 ; j < m.h; ++j){
            for(k = 0; k < m.w; ++k){
                printf("%.2lf, ", m.data[i*m.h*m.w + j*m.w + k]);
                if(k > 30) break;
            }
            printf("\n");
            if(j > 30) break;
        }
        printf("\n");
    }
    printf("\n");
}

image collapse_images_vert(image *ims, int n)
{
    int color = 1;
    int border = 1;
    int h,w,c;
    w = ims[0].w;
    h = (ims[0].h + border) * n - border;
    c = ims[0].c;
    if(c != 3 || !color){
        w = (w+border)*c - border;
        c = 1;
    }

    image filters = make_image(w, h, c);
    int i,j;
    for(i = 0; i < n; ++i){
        int h_offset = i*(ims[0].h+border);
        image copy = copy_image(ims[i]);
        //normalize_image(copy);
        if(c == 3 && color){
            embed_image(copy, filters, 0, h_offset);
        }
        else{
            for(j = 0; j < copy.c; ++j){
                int w_offset = j*(ims[0].w+border);
                image layer = get_image_layer(copy, j);
                embed_image(layer, filters, w_offset, h_offset);
                free_image(layer);
            }
        }
        free_image(copy);
    }
    return filters;
} 

image collapse_images_horz(image *ims, int n)
{
    int color = 1;
    int border = 1;
    int h,w,c;
    int size = ims[0].h;
    h = size;
    w = (ims[0].w + border) * n - border;
    c = ims[0].c;
    if(c != 3 || !color){
        h = (h+border)*c - border;
        c = 1;
    }

    image filters = make_image(w, h, c);
    int i,j;
    for(i = 0; i < n; ++i){
        int w_offset = i*(size+border);
        image copy = copy_image(ims[i]);
        //normalize_image(copy);
        if(c == 3 && color){
            embed_image(copy, filters, w_offset, 0);
        }
        else{
            for(j = 0; j < copy.c; ++j){
                int h_offset = j*(size+border);
                image layer = get_image_layer(copy, j);
                embed_image(layer, filters, w_offset, h_offset);
                free_image(layer);
            }
        }
        free_image(copy);
    }
    return filters;
} 

void show_image_normalized(image im, const char *name)
{
    image c = copy_image(im);
    normalize_image(c);
    show_image(c, name);
    free_image(c);
}

void show_images(image *ims, int n, char *window)
{
    image m = collapse_images_vert(ims, n);
    /*
       int w = 448;
       int h = ((float)m.h/m.w) * 448;
       if(h > 896){
       h = 896;
       w = ((float)m.w/m.h) * 896;
       }
       image sized = resize_image(m, w, h);
     */
    normalize_image(m);
    save_image(m, window);
    show_image(m, window);
    free_image(m);
}

void free_image(image m)
{
    if(m.data){
        free(m.data);
    }
}
