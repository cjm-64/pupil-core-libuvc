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

using namespace cv;
using namespace std;

Mat image;

int run_count = 0;
tjhandle _jpegDecompressor = tjInitDecompress();

int getCamInfo (int i) {
    uvc_context_t *ctx;
    uvc_device_t **device_list;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    uvc_error_t res;

    printf("\n\n\n");
    res = uvc_init(&ctx, NULL);
    if (res < 0) {
        uvc_perror(res, "uvc_init");
    } else {
        printf("ctx initialized\n");
    }

    res = uvc_find_devices(ctx, &device_list, 0, 0, NULL);
    if (res < 0) {
        uvc_perror(res, "uvc_find_device"); /* no devices found */
    } else {
        puts("Devices found");
    }

    res = uvc_open(device_list[i], &devh, 1);
    if (res < 0) {
        uvc_perror(res, "uvc_open"); /* unable to open device */
        cout << i << " failed to open" << endl;
    } else {
        cout << i << " opened" << endl;
//        uvc_print_diag(devh, stderr);
    }
    int width, height, fps;

    const uvc_format_desc_t *format_desc = uvc_get_format_descs(devh);
    const uvc_frame_desc_t *frame_desc = format_desc->frame_descs->next;
    enum uvc_frame_format frame_format = UVC_FRAME_FORMAT_ANY;
    if (frame_desc) {
        width = frame_desc->wWidth;
        height = frame_desc->wHeight;
        fps = 10000000 / frame_desc->intervals[2];
    }
    cout << i << ": ";
    printf("(%4s) %dx%d %dfps\n", format_desc->fourccFormat, width, height, fps);

    bool retval;
    cout << "fps: " << fps << endl;
    if (fps == 120) {
        retval = true;
    } else {
        retval = false;
    }

    uvc_close(devh);
    uvc_exit(ctx);

    return retval;
}


void callback(uvc_stream_handle_t *hand, string winname){
    uvc_frame_t *frame;
    uvc_frame_t *bgr;
    uvc_error_t res;

    res = uvc_stream_get_frame(hand, &frame, 0);
    if(res < 0){
        uvc_perror(res, "Failed to get frame");
    }
    else{
        printf("got frame");
    }

    //Allocate buffers for conversions
    int frameW = frame->width;
    int frameH = frame->height;
    long unsigned int frameBytes = frame->data_bytes;

    printf("callback! frame_format = %d, width = %d, height = %d, length = %lu\n",frame->frame_format, frameW, frameH, frameBytes);

    if (frame->frame_format == 3){
        // yuyv format
        cout << "Format: YUYV" << endl;
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
        placeholder.release();
        uvc_free_frame(bgr);
    }
    else if (frame->frame_format == 7){
        //jpeg format
        cout << "Format: JPEG" << endl;
        int status, j_width, j_height, jpegSubsamp, header_ok;
        int colorComponents = 3;
        unsigned char decompBuffer[frameW*frameH*colorComponents];
        tjDecompressHeader2(_jpegDecompressor, (unsigned char *)frame->data, frameBytes,  &j_width, &j_height, &jpegSubsamp);
        tjDecompress2(_jpegDecompressor, (unsigned char *)frame->data, frameBytes, decompBuffer, frameW, 0, frameH, TJPF_RGB, TJFLAG_FASTDCT);
        Mat placeholder(Size(frameW,frameH), CV_8UC3, decompBuffer);
        placeholder.copyTo(image);
        cout << "dRows: " << image.rows << " dCols: " << image.cols << endl;
        cout << "test " << decompBuffer[0] << endl;
        placeholder.release();
    }
    else {
        //Not one of the 2 formats, not sure how we got here tbh
    }

    Mat adjusted;
    flip(image, adjusted, 0);
//    namedWindow(winname, WINDOW_AUTOSIZE);
    printf("create win\n");
    imshow(winname,adjusted);
    printf("img shown\n");
    waitKey(1);
    printf("waitkeyed\n");
    adjusted.release();

}

int main(int argc, char* argv[]) {
    uvc_context_t *Rightctx, *Leftctx;
    uvc_device_t *dev;
    uvc_device_t **devicelist;
    uvc_device_descriptor_t *desc;
    uvc_device_handle_t *righteyecam, *lefteyecam;
    uvc_stream_ctrl_t righteyectrl, lefteyectrl;
    uvc_stream_handle_t *righteyestrmh, *lefteyestrmh;
    uvc_error_t res;

    int width, height, fps;

    int loc = 0;
    int device_iterators[2] = {0};
    for (int i = 0; i<3; i++){
        if (getCamInfo(i) > 0){
            device_iterators[loc] = i;
            loc++;
        }
        else{
            printf("World cam; ignoring\n");
        }
    }
    for (int i = 0; i<2; i++){
        cout << "DI: " << device_iterators[i] << endl;
    }
    printf("\n\n\n");

    res = uvc_init(&Rightctx, NULL);
    if (res < 0) {
        uvc_perror(res, "uvc_init");
        return res;
    }
    else{
        printf("Right init\n");
        res = uvc_find_devices(Rightctx, &devicelist, 0, 0, NULL);
        if (res < 0) {
            uvc_perror(res, "uvc_find_device"); /* no devices found */
        }
        else {
            printf("Right found\n");
            res = uvc_open(devicelist[device_iterators[0]], &righteyecam, 0);
            if (res < 0) {
                uvc_perror(res, "uvc_open"); /* unable to open device */
            }
            else {
                printf("Right opened\n");
                const uvc_format_desc_t *right_format_desc = uvc_get_format_descs(righteyecam);
                const uvc_frame_desc_t *right_frame_desc = right_format_desc->frame_descs->next;
                enum uvc_frame_format right_frame_format = UVC_FRAME_FORMAT_ANY;
                if (right_frame_desc) {
                    width = right_frame_desc->wWidth;
                    height = right_frame_desc->wHeight;
                    fps = 10000000 / right_frame_desc->intervals[2];
                }
                printf("\nRight eye format: (%4s) %dx%d %dfps\n", right_format_desc->fourccFormat, width, height, fps);
                res = uvc_get_stream_ctrl_format_size(righteyecam, &righteyectrl,right_frame_format, width, height, fps, 0);
                if (res < 0){
                    uvc_perror(res, "start_streaming");
                }
                else{
                    printf("Right stream ctrl formatted\n");
                    printf("\n\nRight eye stream controls\n");
                    uvc_print_stream_ctrl(&righteyectrl, stderr);
                    res = uvc_stream_open_ctrl(righteyecam, &righteyestrmh, &righteyectrl, 0);
                    if (res < 0){
                        uvc_perror(res, "start_streaming");
                    }
                    else{
                        printf("Right stream open ctrl\n");
                        res = uvc_stream_start(righteyestrmh, nullptr, nullptr,2.0,0);
                        if (res < 0){
                            uvc_perror(res, "start_streaming");
                        }
                        else{
                            printf("Right stream start\n");
                            res = uvc_stream_start(righteyestrmh, nullptr, nullptr,2.0,0);
                            if (res < 0){
                                printf("Right Eye Stream Start Fail\n\n\n");
                            }
                            else{
                                printf("Right Eye Stream Started\n\n\n");
                            }
                        }
                    }
                }
            }
        }
    }

    res = uvc_init(&Leftctx, NULL);
    if (res < 0) {
        uvc_perror(res, "uvc_init");
        return res;
    }
    else{
        printf("Left init\n");
        res = uvc_find_devices(Leftctx, &devicelist, 0, 0, NULL);
        if (res < 0) {
            uvc_perror(res, "uvc_find_device"); /* no devices found */
        }
        else {
            printf("Left found\n");
            res = uvc_open(devicelist[device_iterators[1]], &lefteyecam, 0);
            if (res < 0) {
                uvc_perror(res, "uvc_open"); /* unable to open device */
            }
            else {
                printf("Left opened\n");
                const uvc_format_desc_t *left_format_desc = uvc_get_format_descs(lefteyecam);
                const uvc_frame_desc_t *left_frame_desc = left_format_desc->frame_descs->next;
                enum uvc_frame_format left_frame_format = UVC_FRAME_FORMAT_ANY;
                if (left_frame_desc) {
                    width = left_frame_desc->wWidth;
                    height = left_frame_desc->wHeight;
                    fps = 10000000 / left_frame_desc->intervals[2];
                }
                printf("\nLeft eye format: (%4s) %dx%d %dfps\n", left_format_desc->fourccFormat, width, height, fps);
                res = uvc_get_stream_ctrl_format_size(lefteyecam, &lefteyectrl,left_frame_format, width, height, fps, 0);
                if (res < 0){
                    uvc_perror(res, "start_streaming");
                }
                else{
                    printf("Left stream ctrl formatted\n");
                    printf("\n\nLeft eye stream controls\n");
                    uvc_print_stream_ctrl(&lefteyectrl, stderr);
                    res = uvc_stream_open_ctrl(lefteyecam, &lefteyestrmh, &lefteyectrl, 0);
                    if (res < 0){
                        uvc_perror(res, "start_streaming");
                    }
                    else{
                        printf("Left stream open ctrl\n");
                        res = uvc_stream_start(lefteyestrmh, nullptr, nullptr,2.0,0);
                        if (res < 0){
                            uvc_perror(res, "start_streaming");
                        }
                        else{
                            printf("Left stream start\n");
                            res = uvc_stream_start(lefteyestrmh, nullptr, nullptr,2.0,0);
                            if (res < 0){
                                printf("Left Eye Stream Start Fail\n\n\n");
                            }
                            else{
                                printf("Left Eye Stream Started\n\n\n");
                            }
                        }
                    }
                }
            }
        }
    }


    printf("Right\n");
    uvc_print_diag(lefteyecam, stderr);
    printf("Left\n");
    uvc_print_diag(lefteyecam, stderr);


    if (atoi(argv[1]) < 1){
        printf("No run cam\n");
    }
    else{
        auto start = chrono::system_clock::now();
        auto curr_time = chrono::system_clock::now();
        chrono::duration<double> elapsed_seconds = curr_time-start;
        while (elapsed_seconds.count() < atoi(argv[1])){
            callback(righteyestrmh, "Right Eye");
            callback(lefteyestrmh, "Left Eye");
            curr_time = chrono::system_clock::now();
            elapsed_seconds = curr_time-start;
            run_count = run_count + 1;
        }
        cout << "Total time: " << elapsed_seconds.count() << "s" << endl;
        cout << "Total runs: " << run_count << endl;
    }

    uvc_exit(Rightctx);
    uvc_exit(Leftctx);

    std::cout << "Hello, World!" << std::endl;
    return 0;
}
