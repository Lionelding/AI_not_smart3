#include "opencv2/core/types_c.h"
#include "opencv2/video/tracking.hpp"
#include "opencv2/legacy/compat.hpp"
#include "opencv2/core/mat.hpp"
#include "kalmanbox.h"

#define CVX_YELLOW	CV_RGB(0xff,0xff,0x00)
#define CVX_WHITE	CV_RGB(0xff,0xff,0xff)
#define CVX_RED		CV_RGB(0xff,0x00,0x00)


kalmanbox* create_kalmanfilter(CvPoint boxcenter, CvPoint boxvelocity){

    float state[4]={boxcenter.x, boxcenter.y,boxvelocity.x,boxvelocity.y};
    float state2[2]={boxcenter.x, boxcenter.y};


    //Kalmanfilter Initialization
	kalmanbox* kalmanbox_out = (kalmanbox*)malloc(sizeof(kalmanbox));
	kalmanbox_out->kalmanfilter=cvCreateKalman(4,4,0);

	//Initialize clock
	kalmanbox_out->clock=1;
	printf("\t clock: %i\n", kalmanbox_out->clock);

	//Create variable to save the initial state
	kalmanbox_out->x_k = cvCreateMat(4, 1, CV_32FC1 );
	//kalmanbox_out->x_k = kalmanbox_out->kalmanfilter->PriorState;

	memcpy(kalmanbox_out->x_k->data.fl, state, sizeof(state));
    CvRandState rng;
    cvRandInit( &rng, 0, 1, -1, CV_RAND_UNI );
    cvRandSetRange( &rng, 0, 0, 0 );
    rng.disttype = CV_RAND_NORMAL;


    //Create variable to save the Measurement state
    kalmanbox_out->z_k = cvCreateMat( 4, 1, CV_32FC1 );
    cvZero(kalmanbox_out->z_k );
    memcpy(kalmanbox_out->z_k->data.fl, state, sizeof(state));
    printf("\t Measurement State z_k x: %0.0f, y: %0.0f\n", kalmanbox_out->z_k->data.fl[0], kalmanbox_out->z_k->data.fl[1]);


    //Initialization of other variables
    float dt=2;
    const float trans_data[16]={1,0,dt,0,0,1,0,dt,0,0,1,0,0,0,0,1};
    memcpy( kalmanbox_out->kalmanfilter->transition_matrix->data.fl, trans_data, sizeof(trans_data));
    cvSetIdentity( kalmanbox_out->kalmanfilter->measurement_matrix, cvRealScalar(1) );
    cvSetIdentity( kalmanbox_out->kalmanfilter->process_noise_cov, cvRealScalar(1e-4) ); //Q --> 'process noise covariance matrix
    cvSetIdentity( kalmanbox_out->kalmanfilter->measurement_noise_cov, cvRealScalar(1e-3) ); //R --> measurement noise covariance matrix
    cvSetIdentity( kalmanbox_out->kalmanfilter->error_cov_post, cvRealScalar(0.1));
    cvRand(&rng, kalmanbox_out->kalmanfilter->state_post);


    //Postprior State from "previous frame"
    memcpy(kalmanbox_out->kalmanfilter->state_post->data.fl, state, sizeof(state));
    printf("\t Initialization x: %0.0f, y: %0.0f, vx: %0.0f, vy: %0.0f\n", kalmanbox_out->kalmanfilter->PriorState[0], kalmanbox_out->kalmanfilter->PriorState[1], kalmanbox_out->kalmanfilter->PriorState[2], kalmanbox_out->kalmanfilter->PriorState[3]);

    //state_pre is the Prediction
    kalmanbox_out->y_k= cvKalmanPredict(kalmanbox_out->kalmanfilter, 0 );
    printf("\t state_pre x: %0.0f, y: %0.0f, vx: %0.0f, vy: %0.0f\n", kalmanbox_out->kalmanfilter->state_pre->data.fl[0], kalmanbox_out->kalmanfilter->state_pre->data.fl[1], kalmanbox_out->kalmanfilter->state_pre->data.fl[2], kalmanbox_out->kalmanfilter->state_pre->data.fl[3]);


    return kalmanbox_out;
}

void update_kalmanfilter(IplImage *im_frame, kalmanbox* kalmanbox_out, CvPoint observedPt, CvPoint observedV, int width, int height){

	//Update clock
	kalmanbox_out->clock=1+kalmanbox_out->clock;
	printf("\t Kalman filter clock: %i\n", kalmanbox_out->clock);

 	printf("\t Bounding Box Measured Center x: %i, y: %i, vx: %i, vy: %i\n", observedPt.x, observedPt.y, observedV.x, observedV.y);
    printf("\t state_pre x: %0.0f, y: %0.0f, vx: %0.0f, vy: %0.0f\n", kalmanbox_out->kalmanfilter->state_pre->data.fl[0], kalmanbox_out->kalmanfilter->state_pre->data.fl[1], kalmanbox_out->kalmanfilter->state_pre->data.fl[2], kalmanbox_out->kalmanfilter->state_pre->data.fl[3]);

    float modified_observed_position[4]={observedPt.x, observedPt.y, (observedV.x+kalmanbox_out->kalmanfilter->state_pre->data.fl[2])/2, (observedV.y+kalmanbox_out->kalmanfilter->state_pre->data.fl[3])/2};

//    if((kalmanbox_out->kalmanfilter->state_pre->data.fl[2]=!0 || kalmanbox_out->kalmanfilter->state_pre->data.fl[3]!=0) && (observedV.x==0 && observedV.y==0)){
//    	//Add some offset from previous state_prediction
//    	modified_observed_position[0]=observedPt.x;
//    	modified_observed_position[1]=observedPt.y;
//    	modified_observed_position[2]=(observedV.x+kalmanbox_out->kalmanfilter->state_pre->data.fl[2])/2;
//    	modified_observed_position[3]=(observedV.y+kalmanbox_out->kalmanfilter->state_pre->data.fl[3])/2;
//    	printf("\t Add offsets to observed\n");
//
//    }
//    else{
//    	modified_observed_position[0]=observedPt.x;
//    	modified_observed_position[1]=observedPt.y;
//    	modified_observed_position[2]=observedV.x;
//    	modified_observed_position[3]=observedV.y;
//    }


 	printf("\t Modified Measurement x: %0.1f, y: %0.1f, vx: %0.1f, vy: %0.1f\n", modified_observed_position[0], modified_observed_position[1], modified_observed_position[2], modified_observed_position[3]);
 	memcpy(kalmanbox_out->z_k->data.fl, modified_observed_position, sizeof(modified_observed_position));


	kalmanbox_out->x_k=cvKalmanCorrect(kalmanbox_out->kalmanfilter, kalmanbox_out->z_k );
	printf("\t state_post x: %0.0f, y: %0.0f, vx: %0.0f, vy: %0.0f\n", kalmanbox_out->kalmanfilter->state_post->data.fl[0], kalmanbox_out->kalmanfilter->state_post->data.fl[1], kalmanbox_out->kalmanfilter->state_post->data.fl[2], kalmanbox_out->kalmanfilter->state_post->data.fl[3]);


    //draw Red bounding boxes indicating the postprior state with current measurement
	float rx=CV_MAT_ELEM(*(kalmanbox_out->x_k), float, 0, 0);
	float ry=CV_MAT_ELEM(*(kalmanbox_out->x_k), float, 1, 0);
	CvPoint reallefttop=cvPoint(rx-width/2, ry-height/2);
	CvPoint realrightbot=cvPoint(rx+width/2, ry+height/2);
	cvRectangle(im_frame, reallefttop, realrightbot, CVX_RED, 3, 8, 0 );


	kalmanbox_out->y_k = cvKalmanPredict(kalmanbox_out->kalmanfilter, 0 );


	//draw white bounding boxes indicating the predicted state before current measurement
	float px=CV_MAT_ELEM(*(kalmanbox_out->kalmanfilter->state_pre), float, 0, 0);
	float py=CV_MAT_ELEM(*(kalmanbox_out->kalmanfilter->state_pre), float, 1, 0);
	CvPoint predictedlefttop=cvPoint(px-width/2, py-height/2);
	CvPoint predictedrightbot=cvPoint(px+width/2, py+height/2);
	cvRectangle(im_frame, predictedlefttop, predictedrightbot, CVX_WHITE, 3, 8, 0 );
    printf("\t state_pre x: %0.0f, y: %0.0f, vx: %0.0f, vy: %0.0f\n", kalmanbox_out->kalmanfilter->state_pre->data.fl[0], kalmanbox_out->kalmanfilter->state_pre->data.fl[1], kalmanbox_out->kalmanfilter->state_pre->data.fl[2], kalmanbox_out->kalmanfilter->state_pre->data.fl[3]);


	return;
}
