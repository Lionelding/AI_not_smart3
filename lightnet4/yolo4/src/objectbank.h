#ifndef SRC_OBJECTBANK_H_
#define SRC_OBJECTBANK_H_

#include "opencv2/core/types_c.h"
#include "opencv2/core/core_c.h"
#include "/usr/include/python2.7/Python.h"

//ADDED: Opticalflow type to store the points
typedef struct {
	CvPoint sum_p0;
	CvPoint sum_p1;
	CvPoint abs_p0;
	CvPoint abs_p1;
	double magnitude;
	double degree;
}Opticalflow;


// Opticalflow functions
void computHistorgram(int Array[]);
void initializePython();
Opticalflow compute_opticalflow(IplImage *imgA, IplImage *imgB, int xoff, int yoff);
Opticalflow compute_opticalflowFB(IplImage *previous, IplImage *current, int frame_nu, int debug_frame);
Opticalflow updateFlow(int preFlow, int preMag, Opticalflow average_result);
Opticalflow create_opticalflow(CvPoint sum_p0, CvPoint sum_p1, CvPoint abs_p0, CvPoint abs_p1);
Opticalflow create_opticalflowFB(int degree, int magnitude);
int addDegree(int degree1, int degree2, double bias);
int addMagnitude(int m1, int m2, float bias);
Opticalflow degreeMedian(double* degreeStore, int end, int range);
double componentMedian(double* valueStore, int end, int sScope);
Opticalflow drawOptFlowMap(CvMat* flow, CvMat *cflowmap, int step, double scale, CvScalar color);



#endif /* SRC_OBJECTBANK_H_ */
