
cvNamedWindow("Kalman", 1 );
CvRandState rng;
cvRandInit( &rng, 0, 1, -1, CV_RAND_UNI );
IplImage* img = cvCreateImage( cvSize(500,500), 8, 3 );
CvKalman* kalman = cvCreateKalman( 2, 1, 0 );

CvMat* x_k = cvCreateMat( 2, 1, CV_32FC1 );
cvRandSetRange( &rng, 0, 0.1, 0 );
rng.disttype = CV_RAND_NORMAL;
cvRand( &rng, x_k );

CvMat* w_k = cvCreateMat( 2, 1, CV_32FC1 );

CvMat* z_k = cvCreateMat( 1, 1, CV_32FC1 );
cvZero( z_k );

const float F[] = { 1, 1, 0, 1 };
memcpy( kalman->transition_matrix->data.fl, F, sizeof(F));

cvSetIdentity( kalman->measurement_matrix, cvRealScalar(1) );
cvSetIdentity( kalman->process_noise_cov, cvRealScalar(1e-5) );
cvSetIdentity( kalman->measurement_noise_cov, cvRealScalar(1e-1) );
cvSetIdentity( kalman->error_cov_post, cvRealScalar(1));

while( 1 ) {
	const CvMat* y_k = cvKalmanPredict( kalman, 0 );

	cvRandSetRange(&rng,0,sqrt(kalman->measurement_noise_cov->data.fl[0]),0);
	cvRand( &rng, z_k );
	cvMatMulAdd( kalman->measurement_matrix, x_k, z_k, z_k );

	cvZero( img );
	cvCircle( img, phi2xy(z_k), 4, CVX_YELLOW, 2,8,0 ); // observed state
	cvCircle( img, phi2xy(y_k), 4, CVX_WHITE, 2,8,0 ); // “predicted” state
	cvCircle( img, phi2xy(x_k), 4, CVX_RED, 2,8,0 );
	// real state
	cvShowImage( "Kalman", img );

	cvKalmanCorrect( kalman, z_k );

	cvRandSetRange(&rng,0,sqrt(kalman->process_noise_cov->data.fl[0]),0);
	cvRand( &rng, w_k );
	cvMatMulAdd( kalman->transition_matrix, x_k, w_k, x_k );
	if( cvWaitKey( 100 ) == 27 ) break;
}