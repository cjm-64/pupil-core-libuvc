#include <iostream>
#include "stdio.h"
#include "unistd.h"
#include "string"
#include "libuvc/libuvc.h"
#include <turbojpeg.h>
#include <math.h>
#include <chrono>

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


    res = uvc_stream_get_frame(hand, &frame, 1*pow(10,6));
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
    convertScaleAbs(image, adjusted, 1, 100);
    namedWindow(winname, WINDOW_AUTOSIZE);
    printf("create win\n");
    imshow(winname,image);
    printf("img shown\n");
    waitKey(1);
    printf("waitkeyed\n");
    adjusted.release();

}

uvc_stream_handle_t * initCam(uvc_device_t *device, uvc_stream_handle_t *stream_handle, string eye){
    uvc_device_handle_t *device_handle;
    uvc_stream_ctrl_t control;
    uvc_error_t result;

    printf("\n\n");
    result = uvc_open(device, &device_handle, 1);
    if (result < 0) {
        cout << eye << " ";
        uvc_perror(result, "uvc_open"); /* unable to open device */
    }
    else{
        cout << eye << " opened" << endl;
//	    uvc_print_diag(device_handle, stderr);
    }
    

    int width, height, fps;

    const uvc_format_desc_t *format_desc = uvc_get_format_descs(device_handle);
    const uvc_frame_desc_t *frame_desc = format_desc->frame_descs->next;
    enum uvc_frame_format frame_format = UVC_FRAME_FORMAT_ANY;
    if (frame_desc) {
        width = frame_desc->wWidth;
        height = frame_desc->wHeight;
        fps = 10000000 / frame_desc->intervals[2];
    }
    cout << eye << ": ";
    printf("(%4s) %dx%d %dfps\n", format_desc->fourccFormat, width, height, fps);
    
    result = uvc_get_stream_ctrl_format_size(device_handle, &control, frame_format, width, height, fps, 1);
    if (result < 0){
        cout << eye << " ";
        uvc_perror(result, "start_streaming");
    }
    else{
        cout << eye << " get stream control success" << endl;
//        printf("\n\nStream controls\n");
//        uvc_print_stream_ctrl(&control, stderr);
    }

    result = uvc_stream_open_ctrl(device_handle, &stream_handle, &control, 1);
    if (result < 0){
        cout << eye << " ";
        uvc_perror(result, "start_streaming");
    }
    else{
        cout << eye << " stream open ctrl success" << endl;
    }

    return stream_handle;
}

int main(int argc, char* argv[]) {
    uvc_context_t *right_context, *left_context;
    uvc_device_t **device_list;
    uvc_stream_handle_t *right_strmh, *left_strmh;
    uvc_error_t res;

    // For some reason the cameras aren't always in the same order when they are plugged in
    // So this gets the locations of the cameras and puts them in device_iterators
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

    res = uvc_init(&right_context, NULL);

    if (res < 0) {
        uvc_perror(res, "uvc_init");
        return res;
    }
    else{
        printf("right ctx initialized\n");
        res = uvc_init(&left_context, NULL);
        if (res < 0) {
                uvc_perror(res, "uvc_init");
                return res;
            }
            else{
            printf("left ctx initialized\n");
        }
    }

    res = uvc_find_devices(right_context, &device_list, 0, 0, NULL); /* filter devices: vendor_id, product_id, "serial_num" */

    if (res < 0) {
        uvc_perror(res, "uvc_find_device"); /* no devices found */
    }
    else {
        puts("Devices found");
    }

    right_strmh = initCam(device_list[device_iterators[0]], right_strmh, "Right");
    left_strmh = initCam(device_list[device_iterators[1]], left_strmh, "Left");

    res = uvc_stream_start(right_strmh, nullptr, nullptr,2.0,1);
    if (res < 0){
        cout << "Right :";
        uvc_perror(res, "start_streaming");
    }
    else{
        cout << "Right stream start success" << endl;
        res = uvc_stream_start(left_strmh, nullptr, nullptr,2.0,1);
        if (res < 0){
            cout << "Left :";
            uvc_perror(res, "start_streaming");
        }
        else{
            cout << "Left stream start success\n" << endl;

        }
    }

    if (atoi(argv[1]) < 1){
        printf("No run cam\n");
    }
    else{
        cout << "Running for " << argv[1] << " seconds" << endl;
        auto start = chrono::system_clock::now();
        auto curr_time = chrono::system_clock::now();
        chrono::duration<double> elapsed_seconds = curr_time-start;
        while (elapsed_seconds.count() < stoi(argv[1])){
            callback(right_strmh, "Right Eye");
            callback(left_strmh, "Left Eye");
            curr_time = chrono::system_clock::now();
            elapsed_seconds = curr_time-start;
            run_count = run_count + 1;
        }
        cout << "Total time: " << elapsed_seconds.count() << "s" << endl;
        cout << "Total runs: " << run_count << endl;
    }
    printf("Run end \n");
    uvc_exit(right_context);
    uvc_exit(left_context);
    printf("Hello World\n");
}
