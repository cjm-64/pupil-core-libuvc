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

void callback(uvc_stream_handle_t *hand, int timeout){
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
    namedWindow("Test", WINDOW_AUTOSIZE);
    printf("create win\n");
    imshow("Test",image);
    printf("img shown\n");
    waitKey(1);
    printf("waitkeyed\n");
    adjusted.release();

}

int main(int argc, char* argv[]) {
    uvc_context_t *ctx;
    uvc_device_t **dev;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    uvc_stream_handle_t *strmh;
    uvc_error_t res;


    res = uvc_init(&ctx, NULL);

    if (res < 0) {
        uvc_perror(res, "uvc_init");
        return res;
    }
    else{
        printf("uvc initialized\n");
    }

    res = uvc_find_devices(ctx, &dev, 0, 0, NULL); /* filter devices: vendor_id, product_id, "serial_num" */

    if (res < 0) {
        uvc_perror(res, "uvc_find_device"); /* no devices found */
    }
    else {
        puts("Device found");
    }

    /* Try to open the device: requires exclusive access */
    res = uvc_open(dev[1], &devh, 1);

    if (res < 0) {
        uvc_perror(res, "uvc_open"); /* unable to open device */
    }
    else {
        puts("Device opened");
    }

    uvc_print_diag(devh, stderr);

    const uvc_format_desc_t *format_desc;
    format_desc = uvc_get_format_descs(devh);
    const uvc_frame_desc_t *frame_desc;
    frame_desc = format_desc->frame_descs->next;
    enum uvc_frame_format frame_format;
    int width = 640;
    int height = 480;
    int fps = 30;

    switch (format_desc->bDescriptorSubtype) {
        case UVC_VS_FORMAT_MJPEG:
            printf("Color format MJPEG\n");
            frame_format = UVC_COLOR_FORMAT_MJPEG;
            break;
        case UVC_VS_FORMAT_FRAME_BASED:
            printf("Color format H264\n");
            frame_format = UVC_FRAME_FORMAT_H264;
            break;
        default:
            printf("Color format any\n");
            frame_format = UVC_FRAME_FORMAT_ANY;
            break;
    }

    if (frame_desc) {
        width = frame_desc->wWidth;
        height = frame_desc->wHeight;
        fps = 10000000 / frame_desc->dwDefaultFrameInterval;
    }

    printf("\nFirst format: (%4s) %dx%d %dfps\n", format_desc->fourccFormat, width, height, fps);


    res = uvc_get_stream_ctrl_format_size(devh, &ctrl,frame_format, width, height, fps, 1);


    /* Print out the result */
    uvc_print_stream_ctrl(&ctrl, stderr);

    if (res < 0){
        uvc_perror(res, "start_streaming");
    }
    else{
        res = uvc_stream_open_ctrl(devh, &strmh, &ctrl, 0);
    }

    if (res < 0){
        uvc_perror(res, "start_streaming");
    }
    else{
        res = uvc_stream_start(strmh, nullptr, nullptr,2.0,0);
    }

//    int16_t brightness = 100;
//    printf("Brightness created\n");
//    res = uvc_set_brightness(devh, brightness);
//    printf("Attempt get Brightness\n");
//    if (res < 0){
//        cout << "res: " << res << endl;
//        cout << "error" << endl;
//        uvc_strerror(res);
//    }
//    else{
//        cout << "res: " << res << endl;
//        cout << "Brightness is: " << brightness << endl;
//    }

    int timeout = 3.3*pow(10,4);

    auto start = chrono::system_clock::now();
    auto end = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = end-start;
    while (elapsed_seconds.count() < atoi(argv[1])){
        callback(strmh, timeout);
        end = chrono::system_clock::now();
        elapsed_seconds = end-start;
        run_count = run_count + 1;
    }
    cout << "Total time: " << elapsed_seconds.count() << "s" << endl;
    cout << "Total runs: " << run_count << endl;



    std::cout << "Hello, World!" << std::endl;
    return 0;
}
