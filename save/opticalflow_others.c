    // Your input images.
    IplImage* A = cvCreateImage(cvSize(100,100),8,1);
    IplImage* B = cvCreateImage(cvSize(100,100),8,1);

    // Size of the grid for feature points A, explained a little more later.
    int gridSize = 4;

    // Number of points to track.
    int count2 = gridSize*gridSize;

    // C-code. In case of c++ use new/delete instead of malloc/free
    CvPoint2D32f * featuresA = (CvPoint2D32f*) malloc(count2*sizeof(CvPoint2D32f));
    CvPoint2D32f * featuresB = (CvPoint2D32f*) malloc(count2*sizeof(CvPoint2D32f));

    // Size of the search window
    CvSize winSize = cvSize(2,2);

    // Number of levels in the pyramid
    int level = 2;

    // Status and error
    char * status = (char*) malloc(count2*sizeof(char));
    float * err   = (float*) malloc(count2*sizeof(float));

    // Set the termination criteria.
    int type = CV_TERMCRIT_ITER|CV_TERMCRIT_EPS;
    double eps = 0.01;
    int iter = 10;
    CvTermCriteria crit = cvTermCriteria(type,iter,eps);

    // Set additional flags, such as CV_LKFLOW_PYR_A_READY,CV_LKFLOW_PYR_B_READY
    // or CV_LKFLOW_INITIAL_GUESS, here I choice not to use any of them.
    int internal_flags = 0;

    // Now we have all variables, just one thing missing..
    // We need some good features to track, ie put something in featuresA.
    // here you can use the cvGoodFeaturesToTrack function, but
    // I will instead just use a simple grid.
    float spacing[2]= {(float) A->width/(float) gridSize, (float)A->height/(float)gridSize};
    float x; float y;
    int i=0;
    for (i = 0; i < count2; ++i){
            x = (i-gridSize*(i/gridSize))*spacing[0]+spacing[0]/2;
            y = (i/gridSize)*spacing[1]+spacing[1]/2;
            featuresA[i] = cvPoint2D32f(x,y);
    }

    // Track the points
    cvCalcOpticalFlowPyrLK(A,B,NULL,NULL,featuresA,featuresB,count2,winSize,level,status,err,crit,internal_flags);

    // Clean up
    cvReleaseImage(&A);
    cvReleaseImage(&B);
    free(featuresA);
    free(featuresB);
    free(status);
    free(err);