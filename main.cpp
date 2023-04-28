#include <iostream>
#include "stdio.h"
#include "unistd.h"
#include "string"
#include "libuvc/libuvc.h"
#include <turbojpeg.h>
#include <math.h>
#include <chrono>
#include <list>

// Computer Vision
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

//Circle
int thresh_val = 50; //Threshold Value
int max_rad = 50; //Radius value
int thresh_max_val = 100; //Max threshold value
int thresh_type = 1; //Type of threshold, read OCV documentation
int CED = 1; //For Circle tracking, read OCV documentation
int Cent_D = 1; //For Circle tracking, read OCV documentation
Scalar col = Scalar(0, 255, 0); // green

int X_Point = 0; //X of center of tracked circle (pupil)
int Y_Point = 0; //Y of center of tracked circle (pupil)
int Radius = 0; //Radius of tracked circle (pupil)

struct CamSettings{
    string CamName;
    int width;
    int height;
    int fps;
};

// ID0 = right, ID1 = Left, ID2 = World
struct CamInfo{
    string CamName;
    uint8_t vID;
    uint8_t pID;
    string uid;
    int cam_num;
};

struct StreamingInfo{
    uvc_context_t *ctx;
    uvc_device_t *dev;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    uvc_stream_handle_t *strmh;
    const uvc_format_desc_t *format_desc;
    uvc_frame_desc_t *frame_desc;
    enum uvc_frame_format frame_format;
};

void getCamInfo(struct CamInfo *ci){
    uvc_context_t *ctx;
    uvc_device_t **device_list;
    uvc_device_t *dev;
    uvc_device_descriptor_t *desc;
    uvc_error_t res;

    res = uvc_init(&ctx,NULL);
    if(res < 0) {
        uvc_perror(res, "uvc_init");
    }

    res = uvc_get_device_list(ctx,&device_list);
    if (res < 0) {
        uvc_perror(res, "uvc_find_device");
    }
    int i = 0;
    int dev_struct_count = 0;
    while (true) {
        cout << i << endl;
        dev = device_list[i];
        if (dev == NULL) {
            break;
        }
        else{
            uvc_get_device_descriptor(dev, &desc);
            if(desc->idVendor != 3141){
                printf("World Cam\n");
                i++;
                continue;
            }
            else{
                ci[dev_struct_count].CamName = desc->product;
                ci[dev_struct_count].vID = desc->idVendor;
                ci[dev_struct_count].pID = desc->idProduct;
                ci[dev_struct_count].uid = to_string(uvc_get_device_address(dev))+":"+to_string(uvc_get_bus_number(dev));
                ci[dev_struct_count].cam_num = i;
                dev_struct_count++;
            }
            printf("Got desc\n");
        }
        uvc_free_device_descriptor(desc);
        i++;
    }
    uvc_exit(ctx);
}

void setUpStreams(struct CamSettings *cs, struct CamInfo *ci, struct StreamingInfo *si){
    uvc_error_t res;
    uvc_device_t **devicelist;

    for(int i = 0; i<2;i++){
        res = uvc_init(&si[i].ctx, NULL);
        if (res < 0) {
            uvc_perror(res, "uvc_init");
        }
        else{
            printf("UVC %d open success\n", i);
        }
        res = uvc_find_devices(si[i].ctx, &devicelist, 0, 0, NULL);
        if (res < 0) {
            uvc_perror(res, "uvc_init");
        }
        else{
            cout << "Dev " << i << ": " << si[i].dev << endl;
        }
        res = uvc_open(devicelist[ci[i].cam_num], &si[i].devh, 1);
        if (res < 0) {
            uvc_perror(res, "uvc_find_device"); /* no devices found */
        }
        else{
            cout << "devh " << i << ": " << si[i].devh << endl;
        }
        si[i].format_desc = uvc_get_format_descs(si[i].devh);
        si[i].frame_desc = si[i].format_desc->frame_descs->next;
        si[i].frame_format = UVC_FRAME_FORMAT_ANY;
        if(si[i].frame_desc->wWidth != NULL){
            cs[i].width = si[i].frame_desc->wWidth;
            cs[i].height = si[i].frame_desc->wHeight;
            cs[i].fps = 10000000 / si[i].frame_desc->intervals[2];
        }
        printf("\nEye %d format: (%4s) %dx%d %dfps\n", i, si[i].format_desc->fourccFormat, cs[i].width, cs[i].height, cs[i].fps);

        res = uvc_get_stream_ctrl_format_size(si[i].devh, &si[i].ctrl, si[i].frame_format, cs[i].width, cs[i].height, cs[i].fps, 1);
        if (res < 0){
            uvc_perror(res, "start_streaming");
        }
        else{
            printf("Eye %d stream control formatted\n", i);
            uvc_print_stream_ctrl(&si[i].ctrl, stderr);
        }

        res = uvc_stream_open_ctrl(si[i].devh, &si[i].strmh, &si[i].ctrl,1);
        if (res < 0){
            uvc_perror(res, "start_streaming");
        }
        else{
            printf("Eye %d stream opened\n", i);
        }

        res = uvc_stream_start(si[i].strmh, nullptr, nullptr,2.0,0);
        if (res < 0){
            uvc_perror(res, "start_streaming");
        }
        else{
            printf("Eye %d stream started\n", i);
        }
        printf("\n\n\n");
    }
}

void callback(struct StreamingInfo *si){
    uvc_frame_t *frame;
    uvc_frame_t *bgr;
    uvc_error_t res;

    Mat image;

    for (int i = 0; i<2;i++){
        res = uvc_stream_get_frame(si[i].strmh, &frame, 1* pow(10,6));
        if(res < 0){
            uvc_perror(res, "Failed to get frame");
            continue;
        }
        else{
            printf("got frame");
        }

        //Allocate buffers for conversions
        int frameW = frame->width;
        int frameH = frame->height;
        long unsigned int frameBytes = frame->data_bytes;

        printf("Eye %d: frame_format = %d, width = %d, height = %d, length = %lu\n", i, frame->frame_format, frameW, frameH, frameBytes);

        bgr = uvc_allocate_frame(frameW * frameH * 3);
        if (!bgr) {
            printf("unable to allocate bgr frame!\n");
            return;
        }
        uvc_error_t res = uvc_yuyv2bgr(frame, bgr);
        if (res < 0){
            printf("Unable to copy frame to bgr!\n");
        }
        Mat placeholder(bgr->height, bgr->width, CV_8UC3, bgr->data);
        placeholder.copyTo(image);

        Mat adjusted;
        flip(image, adjusted, 0);

        Mat grayIMG, binaryIMG, bpcIMG;
        cvtColor(adjusted, grayIMG, COLOR_BGR2GRAY);
        threshold(grayIMG, binaryIMG, thresh_val, thresh_max_val, thresh_type); //gray to binary
        cvtColor(binaryIMG, bpcIMG, COLOR_GRAY2RGB);

        vector<Vec3f> circles;
        HoughCircles(binaryIMG, circles, HOUGH_GRADIENT, 1, 1000, CED, Cent_D, max_rad-2, max_rad);
        Vec3i c;
        for( size_t i = 0; i < circles.size(); i++ ){
            c = circles[i];
        }

        X_Point = c[0];
        Y_Point = c[1];
        Radius = c[2];
        cout << "X: " << X_Point << " Y: " << Y_Point << " Rad: " << Radius << endl;

        //Draw Circles and Box On Black and White
        circle(bpcIMG, Point(X_Point, Y_Point), 1, col,1,LINE_8);
        circle(bpcIMG, Point(X_Point, Y_Point), Radius, col,1,LINE_8);


        //Display image
        imshow(to_string(i),bpcIMG);
        printf("img shown\n");
        if(i == 0){
            moveWindow(to_string(i), 500, 200);
            //Move right eye to right
        }
        else{
            moveWindow(to_string(i), 200, 200);
            //Move left eye to left
        }
        waitKey(1);
        printf("waitkeyed\n");

        //Free memory
        adjusted.release();
        image.release();
        placeholder.release();
        grayIMG.release();
        binaryIMG.release();
        bpcIMG.release();
        uvc_free_frame(bgr);
    }
}

int main(int argc, char* argv[]) {
    CamSettings camSets[2];
    for(int i = 0; i<2;i++){
        if(i == 0){
            camSets[i].CamName = "Pupil Cam2 ID0";
        }
        else{
            camSets[i].CamName = "Pupil Cam2 ID1";
        }
        camSets[i].width = 192;
        camSets[i].height = 192;
        camSets[i].fps = 120;
    }

    CamInfo Cameras[2];
    getCamInfo(Cameras);
    for (int j = 0; j<2; j++){
        cout << "Cam " << j << ": " << Cameras[j].CamName << " " << Cameras[j].uid << " " << Cameras[j].vID << " " << Cameras[j].pID << endl;
    }

    StreamingInfo CamStreams[2];
    setUpStreams(camSets, Cameras, CamStreams);

    if (atoi(argv[1]) < 1){
        printf("No run cam\n");
    }
    else{
        int run_count = 0;
        auto start = chrono::system_clock::now();
        auto curr_time = chrono::system_clock::now();
        chrono::duration<double> elapsed_seconds = curr_time-start;
        while (elapsed_seconds.count() < atoi(argv[1])){
            callback(CamStreams);
            curr_time = chrono::system_clock::now();
            elapsed_seconds = curr_time-start;
            run_count = run_count + 1;
        }
        cout << "Total time: " << elapsed_seconds.count() << "s" << endl;
        cout << "Total runs: " << run_count << endl;
        cout << "Avg fps: " << run_count/atoi(argv[1]) << endl;
    }

    for(int k = 0; k<2; k++){
        uvc_exit(CamStreams[k].ctx);
    }

    std::cout << "Hello, World!" << std::endl;
    return 0;
}
