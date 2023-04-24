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

    cout << "dev: " << device << endl;
    result = uvc_open(device, &device_handle, 1);
    if (result < 0) {
        uvc_perror(result, "uvc_open"); /* unable to open device */
    }
    else{
        printf("Device Opened\n");
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
    cout << "\n" << eye << ": " << endl;
    printf("(%4s) %dx%d %dfps\n", format_desc->fourccFormat, width, height, fps);
    
    result = uvc_get_stream_ctrl_format_size(device_handle, &control, frame_format, width, height, fps, 1);
    if (result < 0){
        uvc_perror(result, "start_streaming");
    }
    else{
        printf("\n\nStream controls\n");
        uvc_print_stream_ctrl(&control, stderr);
    }

    result = uvc_stream_open_ctrl(device_handle, &stream_handle, &control, 1);
    if (result < 0){
        uvc_perror(result, "start_streaming");
    }
    else{
	    printf("UVC Stream open ctrl success\n\n");
    }

    result = uvc_stream_start(stream_handle, nullptr, nullptr,2.0,1);
    if (result < 0){
        uvc_perror(result, "start_streaming");
    }
    else{
        printf("UVC stream start success \n");
    }

    return stream_handle;
}

int main(int argc, char* argv[]) {
    uvc_context_t *right_context, *left_context;
    uvc_device_t **device_list;
    uvc_stream_handle_t *right_strmh, *left_strmh;
    uvc_error_t res;


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

    right_strmh = initCam(device_list[0], right_strmh, "Right");
    left_strmh = initCam(device_list[2], left_strmh, "Left");

    if (atoi(argv[1]) < 1){
        printf("No run cam\n");
    }
    else{
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
}
