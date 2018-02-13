float trans_data[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

float measure_data[2]={0,0};
float state_pre_data[4]={100,200,0,0};


CvMat trans;
cvInitMatHeader(&trans, 4, 4, CV_32FC1, trans_data, CV_AUTOSTEP);
d->transition_matrix=&trans;

float good1=CV_MAT_ELEM(trans, float, 0, 3);
float good2=CV_MAT_ELEM(*(d->transition_matrix), float, 3, 0);

printf("float %0.0f\n",good1);
printf("float %0.0f\n",good2);


CvMat measure;
cvInitMatHeader(&measure, 2, 1, CV_32FC1, measure_data, CV_AUTOSTEP);


CvMat state_pre_mat;
cvInitMatHeader(&measure, 4, 1, CV_32FC1, state_pre_data, CV_AUTOSTEP);


d->state_pre=&state_pre_mat;


//TODO: confirm if these number being used are correct or not
cvSetIdentity(d->measurement_matrix, cvRealScalar(1));
cvSetIdentity(d->process_noise_cov, cvRealScalar(0.0001));
cvSetIdentity(d->measurement_noise_cov, cvRealScalar(0.1));
cvSetIdentity(d->error_cov_post, cvRealScalar(0.1));