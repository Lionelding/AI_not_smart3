#ifndef SRC_KALMANBOX_H
#define SRC_KALMANBOX_H

#include "opencv2/video/tracking.hpp"

typedef struct kalmanbox{

	CvKalman* kalmanfilter;
	CvMat* x_k;
	CvMat* w_k;
	CvMat* z_k;
}kalmanbox;


kalmanbox* create_kalmanfilter(CvPoint boxcenter, CvPoint boxvelocity);
CvMat* update_kalmanfilter(IplImage *im_frame, kalmanbox* kalmanbox_out, CvPoint observedPt, CvPoint observedV, int width, int height);


#endif /* SRC_KALMANBOX_H_ */
