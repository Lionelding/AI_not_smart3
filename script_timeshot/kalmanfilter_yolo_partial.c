 void initialize_kalmanfilter(int filternum){

    kalmanfilter=cvCreateKalman(4,2,0);

    float temp[4]={1043,155,0,0};
    //Exact location 1  float temp[4]={1043,155,0,0};
    //EXact location 17 float temp[4]={1674,1045,0,0};
    x_k = cvCreateMat(4, 1, CV_32FC1 );

    CvRandState rng;
    cvRandInit( &rng, 0, 1, -1, CV_RAND_UNI );
    cvRandSetRange( &rng, 0, 0, 0 );
    rng.disttype = CV_RAND_NORMAL;
    //cvRand( &rng, x_k );

    memcpy( x_k->data.fl, temp, sizeof(temp));

    w_k = cvCreateMat( 4, 1, CV_32FC1 );
    z_k = cvCreateMat( 2, 1, CV_32FC1 );
    cvZero( z_k );
    //TODO:modify the time
    int dt=1;
    const float trans_data[16]={1,0,dt,0,0,1,0,dt,0,0,1,0,0,0,0,1};
    memcpy( kalmanfilter->transition_matrix->data.fl, trans_data, sizeof(trans_data));


    kalmanfilter->PosterState=temp;
    cvSetIdentity( kalmanfilter->measurement_matrix, cvRealScalar(1) );
    cvSetIdentity( kalmanfilter->process_noise_cov, cvRealScalar(1) ); //Q --> 'process noise covariance matrix
    cvSetIdentity( kalmanfilter->measurement_noise_cov, cvRealScalar(1e-5) ); //R --> measurement noise covariance matrix
    cvSetIdentity( kalmanfilter->error_cov_post, cvRealScalar(0.1));

    cvRand(&rng, kalmanfilter->state_post);
    return;
}       	



    if(box_para[idx_store[p]][9]==debug_object_index){
		CvPoint boxcenter=cvPoint(box_para[idx_store[p]][0]+(box_para[idx_store[p]][2]/2), box_para[idx_store[p]][1]+(box_para[idx_store[p]][3])/2);
		float vx=average_result.magnitude*cos(average_result.degree*3.1415926/180);
		float vy=average_result.magnitude*sin(average_result.degree*3.1415926/180);

		CvPoint speed=cvPoint(cvRound(vx), cvRound(vy));
		printf("Box Center x: %i, y: %i, vx: %i, vy: %i\n", boxcenter.x, boxcenter.y, speed.x, speed.y);
		cvCircle( im_frame, boxcenter, 4, CVX_YELLOW, 2,8,0 );

		const CvMat* y_k = cvKalmanPredict(kalmanfilter, 0 );
		printf("Predicted Center x: %0.0f, y: %0.0f \n", y_k->data.fl[0],y_k->data.fl[1]);
		CvRandState rng;

  		float observed_position[2]={boxcenter.x, boxcenter.y};
		memcpy(z_k->data.fl, observed_position, sizeof(observed_position));

		float px=CV_MAT_ELEM(*y_k, float, 0, 0);
		float py=CV_MAT_ELEM(*y_k, float, 1, 0);

		CvPoint predictedlefttop=cvPoint(px-box_para[idx_store[p]][2]/2, py-box_para[idx_store[p]][3]/2);
		CvPoint predictedrightbot=cvPoint(px+box_para[idx_store[p]][2]/2, py+box_para[idx_store[p]][3]/2);


		cvRectangle(im_frame, predictedlefttop, predictedrightbot, CVX_WHITE, 3, 8, 0 );


		cvKalmanCorrect(kalmanfilter, z_k );
		cvRandSetRange(&rng,0,sqrt(kalmanfilter->process_noise_cov->data.fl[0]),0);
		cvRand( &rng, w_k );


		printf("before x: %0.0f, before y: %0.0f\n", CV_MAT_ELEM(*x_k, float, 0, 0), CV_MAT_ELEM(*x_k, float, 1, 0));

		cvMatMulAdd( kalmanfilter->transition_matrix, x_k, w_k, x_k );

		printf("after x: %0.0f, after y: %0.0f\n", CV_MAT_ELEM(*x_k, float, 0, 0), CV_MAT_ELEM(*x_k, float, 1, 0));

 		float rx=CV_MAT_ELEM(*x_k, float, 0, 0);
 		float ry=CV_MAT_ELEM(*x_k, float, 1, 0);

 		CvPoint reallefttop=cvPoint(rx-box_para[idx_store[p]][2]/2, ry-box_para[idx_store[p]][3]/2);
		CvPoint realrightbot=cvPoint(rx+box_para[idx_store[p]][2]/2, ry+box_para[idx_store[p]][3]/2);

		cvRectangle(im_frame, reallefttop, realrightbot, CVX_RED, 3, 8, 0 );

	}