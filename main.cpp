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
Scalar col = Scalar(0, 255, 0); // green

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
        dev = device_list[i];
        if (dev == NULL) {
            break;
        }
        else{
            uvc_get_device_descriptor(dev, &desc);
            printf("Got desc\n");
            if(desc->idVendor != 3141){
                printf("World Cam\n");
                i++;
                continue;
            }
            else{
                string name = desc->product;
                if (name.substr(name.size()-1, 1) == "0"){
                    cout << "Right Eye Camera: " << name << endl;
                    dev_struct_count = 0;
                }
                else{
                    cout << "Left Eye Camera: " << name << endl;
                    dev_struct_count = 1;
                }
                ci[dev_struct_count].CamName = desc->product;
                ci[dev_struct_count].vID = desc->idVendor;
                ci[dev_struct_count].pID = desc->idProduct;
                ci[dev_struct_count].uid = to_string(uvc_get_device_address(dev))+":"+to_string(uvc_get_bus_number(dev));
                ci[dev_struct_count].cam_num = i;
            }
        }
        uvc_free_device_descriptor(desc);
        i++;
    }
    uvc_exit(ctx);
}

void streamTests(struct CamSettings *cs, struct CamInfo *ci, struct StreamingInfo *si){
    uvc_error_t res;
    uvc_device_t **devicelist;
    uvc_device_t *dev;
    uvc_device_descriptor_t *desc;

    for (int i = 0;i<2;++i) {
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
        for (int j = 0; j < 3; ++j){
            dev = devicelist[j];
            uvc_get_device_descriptor(dev, &desc);
            cout << "Dev " << j << ": " << dev << " Name: " << desc->product << endl;
        }
        res = uvc_open(devicelist[ci[i].cam_num], &si[i].devh, 1);
        if (res < 0) {
            uvc_perror(res, "uvc_find_device"); /* no devices found */
        }
        else{
            cout << "devh " << i << ": " << si[i].devh << endl;
        }
    }
}

void setUpStreams(struct CamSettings *cs, struct CamInfo *ci, struct StreamingInfo *si){
    uvc_error_t res;
    uvc_device_t **devicelist;
    uvc_device_t *dev;
    uvc_device_descriptor_t *desc;

    for(int i = 0; i<2;++i){
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
            for (int j = 0; j < 3; ++j){
                dev = devicelist[j];
                uvc_get_device_descriptor(dev, &desc);
                cout << "   Dev " << j << ": " << dev << " Name: " << desc->product << endl;
            }
        }

        res = uvc_open(devicelist[ci[i].cam_num], &si[i].devh, 1);
        if (res < 0) {
            uvc_perror(res, "uvc_find_device"); /* no devices found */
        }
        else{
            cout << "devh " << i << ": " << si[i].devh << endl;
            cout << "Name " << i << ": " << ci[i].CamName << endl;
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
        res = uvc_stream_start(si[i].strmh, nullptr, nullptr,1.6,0);
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
    uvc_frame_t *rgb;
    uvc_error_t res;

    Mat image;

    for (int i = 0; i<2;i++){
        res = uvc_stream_get_frame(si[i].strmh, &frame, 1 * pow(10,6));
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

        rgb = uvc_allocate_frame(frameW * frameH * 3);
        if (!rgb) {
            printf("unable to allocate bgr frame!\n");
            return;
        }
        uvc_error_t res = uvc_yuyv2rgb(frame, rgb);
        if (res < 0){
            printf("Unable to copy frame to bgr!\n");
        }

        // Iterate from CV_8UC1 to CV_8UC4 and see which one works
        Mat placeholder(rgb->height, rgb->width, CV_8UC3, rgb->data);
        placeholder.copyTo(image);


        Mat adjusted;
        flip(image, adjusted, 0);

        //Display image
        imshow(to_string(i),adjusted);
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
        uvc_free_frame(rgb);
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
        cout << "Cam " << j << ": " << Cameras[j].CamName << " " << Cameras[j].uid << " " << Cameras[j].vID << " " << Cameras[j].pID << " " << Cameras[j].cam_num << endl;
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

    for(int l = 0; l<2; ++l){
        cout << "Stream " << l << ":" << endl;
        cout << "   " << CamStreams[l].ctx << endl;
        cout << "   " << CamStreams[l].devh << endl;
        cout << "   " << &CamStreams[l].ctrl << endl;
        cout << "   " << CamStreams[l].strmh << endl;
        printf("\n\n");
    }

    for(int k = 0; k<2; k++){
        uvc_exit(CamStreams[k].ctx);
    }

    std::cout << "Hello, World!" << std::endl;
    return 0;
}
