#ifndef SRC_OBJECTBANK_H_
#define SRC_OBJECTBANK_H_

#include "opencv2/core/types_c.h"
#include "opencv2/core/core_c.h"

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
Opticalflow compute_opticalflow(IplImage *imgA, IplImage *imgB, int xoff, int yoff);
Opticalflow compute_opticalflowFB(IplImage *previous, IplImage *current);
Opticalflow updateFlow(Opticalflow average_result, int preFlow, float bias);
Opticalflow create_opticalflow(CvPoint sum_p0, CvPoint sum_p1, CvPoint abs_p0, CvPoint abs_p1);
Opticalflow create_opticalflowFB(int degree, int magnitude);
int addDegree(int degree1, int degree2, double bias);
int addMagnitude(int m1, int m2);
Opticalflow degreeMedian(double* degreeStore, int end, int range);
double componentMedian(double* valueStore, int end, int sScope);
Opticalflow drawOptFlowMap(CvMat* flow, CvMat *cflowmap, int step, double scale, CvScalar color);



#endif /* SRC_OBJECTBANK_H_ */
