#include "network.h"
#include "detection_layer.h"
#include "region_layer.h"
#include "cost_layer.h"
#include "utils.h"
#include "parser.h"
#include "box.h"
#include "image.h"
#include "demo.h"
#include <sys/time.h>
#include "objectbank.h"

//#include "stdafx.h"
#include "cv.h"
#include "cxcore.h"
#include "highgui.h"

#include "opencv2/legacy/compat.hpp"
#include "opencv2/core/mat.hpp"

#define DEMO 1

#ifdef OPENCV

static char **demo_names;
static image **demo_alphabet;
static int demo_classes;

static float **probs;
//static float **probsMore;
static box *boxes;
static network net;
static image buff [3];
static image buff_letter[3];
static image previous;
static int **box_para; //ADDED: store the information of each bounding box
static int *idx_store; //ADDED: store the index of bouding boxes
static Boxflow *box_full;

static int buff_index = 0;
static CvCapture * cap;
static IplImage  * ipl;

static float fps = 0;
static float demo_thresh = 0;
static float demo_hier = .5;
static int running = 0;

static int demo_delay = 0;
static int demo_frame = 5;
static int demo_detections = 0;
static float **predictions;
static int demo_index = 0;
static int demo_done = 0;
static float *last_avg2;
static float *last_avg;
static float *avg;
double demo_time;

static int opticalflow_entireframe=0;

double get_wall_time()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

//float** getProbsMore(int number1){
//	return probsMore;
//}


void *detect_in_thread(void *ptr)
{
    running = 1;
    float nms = 0.4;
    int temp;

    layer l = net.layers[net.n-1];
    float *X = buff_letter[(buff_index+2)%3].data;
    float *prediction = network_predict(net, X);

    memcpy(predictions[demo_index], prediction, l.outputs*sizeof(float));
    mean_arrays(predictions, demo_frame, l.outputs, avg);
    l.output = last_avg2;
    if(demo_delay == 0) {
    	l.output = avg;}
    if(l.type == DETECTION){
        get_detection_boxes(l, 1, 1, demo_thresh, probs, boxes, 0);
    } else if (l.type == REGION){
         get_region_boxes(l, buff[0].w, buff[0].h, net.w, net.h, 0, probs, boxes, 0, 0, demo_hier, 1);
    } else {
        error("Last layer must produce detections\n");
    }


	temp=getframe_num();
    printf("\nFrame %i\n", temp);
    printf("FPS:%.1f\n",fps);
    printf("Objects:\n\n");


    if(temp>=9){
    	//copy the 84 last elements in each column of probs to probsMore
    	printf("\n");

    }

    if (nms > 0) {
    	do_nms_obj(boxes, probs, l.w*l.h*l.n, l.classes, nms);
    }


    image display = buff[(buff_index+2) % 3];


    if(opticalflow_entireframe){
    //ADDED: Otherwise, the previous and current will change when display changes
    	image display_timestamp;
    	image current;

    	display_timestamp=copy_image(display);


    	if(temp==0){
    		previous=display_timestamp;
    		printf("\nSkip!\n");

    	}
    	else{
    		current=display_timestamp;
    		IplImage *disp_previous=cvCreateImage(cvSize(buff[0].w,buff[0].h), IPL_DEPTH_8U, buff[0].c);
    		IplImage *disp_current=cvCreateImage(cvSize(buff[0].w,buff[0].h), IPL_DEPTH_8U, buff[0].c);
    		disp_previous=image_convert_IplImage(previous, disp_previous);
    		disp_current=image_convert_IplImage(current, disp_current);


    		cvNamedWindow("disp_previous",CV_WINDOW_NORMAL);
    		cvShowImage("disp_previous",disp_previous);

    		cvNamedWindow("disp_current",CV_WINDOW_NORMAL);
    		cvShowImage("disp_current",disp_current);

    		//compute_opticalflow(disp_previous, disp_current);
    		cvReleaseImage(&disp_previous);
    		cvReleaseImage(&disp_current);

    		free(previous.data);
    		previous=display_timestamp;
    	}
    }

    draw_detections(display, demo_detections, demo_thresh, boxes, probs, demo_names, demo_alphabet, demo_classes, box_para, idx_store, box_full);

//    int ii;
//    for(ii=0;ii<l.w*l.h*l.n;ii++){
//    	//memcpy(probsMore[ii], &probs[ii][0], 84*sizeof(float));
//    	memcpy(probsMore[ii], &probs[ii][0], 12*sizeof(float));
//    }

    demo_index = (demo_index + 1)%demo_frame;
    running = 0;
    return 0;
}



void *fetch_in_thread(void *ptr)
{
    int status = fill_image_from_stream(cap, buff[buff_index]);
    letterbox_image_into(buff[buff_index], net.w, net.h, buff_letter[buff_index]);
    if(status == 0) demo_done = 1;
    return 0;
}

void *display_in_thread(void *ptr)
{
    show_image_cv(buff[(buff_index + 1)%3], "demo", ipl);
    int c = cvWaitKey(1);
    if (c != -1) c = c%256;
    if (c == 10){
        if(demo_delay == 0) demo_delay = 60;
        else if(demo_delay == 5) demo_delay = 0;
        else if(demo_delay == 60) demo_delay = 5;
        else demo_delay = 0;
    } else if (c == 27) {
        demo_done = 1;
        return 0;
    } else if (c == 82) {
        demo_thresh += .02;
    } else if (c == 84) {
        demo_thresh -= .02;
        if(demo_thresh <= .02) demo_thresh = .02;
    } else if (c == 83) {
        demo_hier += .02;
    } else if (c == 81) {
        demo_hier -= .02;
        if(demo_hier <= .0) demo_hier = .0;
    }
    return 0;
}

void *display_loop(void *ptr)
{
    while(1){
        display_in_thread(0);
    }
}

void *detect_loop(void *ptr)
{
    while(1){
        detect_in_thread(0);
    }
}


void demo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int delay, char *prefix, int avg_frames, float hier, int w, int h, int frames, int fullscreen)
{
    demo_delay = delay;
    demo_frame = avg_frames;
    predictions = calloc(demo_frame, sizeof(float*));
    image **alphabet = load_alphabet();
    demo_names = names;
    demo_alphabet = alphabet;
    demo_classes = classes;
    demo_thresh = thresh;
    demo_hier = hier;
    printf("Demo\n");
    net = parse_network_cfg(cfgfile);
    if(weightfile){
        load_weights(&net, weightfile);
    }
    set_batch_network(&net, 1);
    pthread_t detect_thread;
    pthread_t fetch_thread;

    srand(2222222);

    if(filename){
        printf("video file: %s\n", filename);
        cap = cvCaptureFromFile(filename);
    }else{
        cap = cvCaptureFromCAM(cam_index);

        if(w){
            cvSetCaptureProperty(cap, CV_CAP_PROP_FRAME_WIDTH, w);
        }
        if(h){
            cvSetCaptureProperty(cap, CV_CAP_PROP_FRAME_HEIGHT, h);
        }
        if(frames){
            cvSetCaptureProperty(cap, CV_CAP_PROP_FPS, frames);
        }
    }

    if(!cap) error("Couldn't connect to webcam.\n");

    layer l = net.layers[net.n-1];
    demo_detections = l.n*l.w*l.h;
    int j;

    avg = (float *) calloc(l.outputs, sizeof(float));
    last_avg  = (float *) calloc(l.outputs, sizeof(float));
    last_avg2 = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < demo_frame; ++j) predictions[j] = (float *) calloc(l.outputs, sizeof(float));

    boxes = (box *)calloc(l.w*l.h*l.n, sizeof(box));
    probs = (float **)calloc(l.w*l.h*l.n, sizeof(float *));
    //ADDED: 3 more addresses to probs
    for(j = 0; j < l.w*l.h*l.n; ++j) probs[j] = (float *)calloc(l.classes+1+3, sizeof(float));


//    probsMore = (float **)calloc(l.w*l.h*l.n, sizeof(float *));
//    //ADDED: 3 more addresses to probs
//    for(j = 0; j < l.w*l.h*l.n; ++j) probsMore[j] = (float *)calloc(l.classes+1+3, sizeof(float));



    //ADDED: box_para to store the left, top, height, width, class,
    box_para = (int **)calloc(l.w*l.h*l.n, sizeof(int *));
    for(j = 0; j < l.w*l.h*l.n; ++j) box_para[j] = (int *)calloc(10+3, sizeof(int));

    //ADDED: idx_store to store the index number that contains an object
    int size=40;
    idx_store = (int *)calloc(size, sizeof(int));
    initialize_idx_prestore(size, l.w*l.h*l.n);
    //initialize_kalmanfilter(1);
    //ADDED: store the bounding box and optical flow vector from one frame together
    box_full=(Boxflow *)calloc(l.w*l.h*l.n, sizeof(Boxflow));
    //for(j = 0; j < (845); ++j) box_full[j] = (Boxflow *)calloc(1, sizeof(Boxflow));


    buff[0] = get_image_from_stream(cap);
    buff[1] = copy_image(buff[0]);
    buff[2] = copy_image(buff[0]);
    buff_letter[0] = letterbox_image(buff[0], net.w, net.h);
    buff_letter[1] = letterbox_image(buff[0], net.w, net.h);
    buff_letter[2] = letterbox_image(buff[0], net.w, net.h);
    ipl = cvCreateImage(cvSize(buff[0].w,buff[0].h), IPL_DEPTH_8U, buff[0].c);
    initializePython();


    int count = 0;
    demo_time = get_wall_time();
    
    while(!demo_done){
        buff_index = (buff_index + 1) %3;

        if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
        if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");
        if(!prefix){
            if(count % (demo_delay+1) == 0){
                fps = 1./(get_wall_time() - demo_time);
                demo_time = get_wall_time();
                float *swap = last_avg;
                last_avg  = last_avg2;
                last_avg2 = swap;
                memcpy(last_avg, avg, l.outputs*sizeof(float));
            }
            display_in_thread(0);
        }else{
            char name[256];
            sprintf(name, "%s_%08d", prefix, count);
            save_image(buff[(buff_index + 1)%3], name);
        }
        pthread_join(fetch_thread, 0);
        pthread_join(detect_thread, 0);
        ++count;
    }
}
#else
void demo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int delay, char *prefix, int avg, float hier, int w, int h, int frames, int fullscreen)
{
    fprintf(stderr, "Demo needs OpenCV for webcam images.\n");
}
#endif

