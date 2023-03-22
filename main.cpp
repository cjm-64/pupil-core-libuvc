#include <iostream>
#include "stdio.h"
#include "unistd.h"
#include "string"
#include "libuvc/libuvc.h"
#include <turbojpeg.h>

// Computer Vision
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace cv;
using namespace std;

int run_count = 0;

Mat BlueGreenRed, image;

tjhandle _jpegDecompressor = tjInitDecompress();

void cb(uvc_frame_t *frame, void *ptr) {
    uvc_frame_t *bgr;
    uvc_error_t ret;
    enum uvc_frame_format *frame_format = (enum uvc_frame_format *)ptr;
    FILE *fp;

    static int jpeg_count = 0;
    static const char *H264_FILE = "iOSDevLog.h264";
    static const char *MJPEG_FILE = ".jpeg";
    char filename[16];

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

    namedWindow("Test", WINDOW_AUTOSIZE);
    printf("create win\n");
    imshow("Test",image);
    printf("img shown\n");
    waitKey(1);
    printf("waitkeyed\n");

    run_count = run_count + 1;
}

int main() {
    uvc_context_t *ctx;
    uvc_device_t **dev;
    uvc_device_t **listd;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    uvc_stream_handle *strmh;
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
    res = uvc_open(dev[0], &devh, 1);

    if (res < 0) {
        uvc_perror(res, "uvc_open"); /* unable to open device */
    }
    else {
        puts("Device opened");
    }

    //uvc_print_diag(devh, stderr);

    const uvc_format_desc_t *format_desc;
    format_desc = uvc_get_format_descs(devh);
    const uvc_frame_desc_t *frame_desc;
    frame_desc = format_desc->frame_descs;
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


    if (res < 0) {
        uvc_perror(res, "get_mode"); /* device doesn't provide a matching stream */
    }
    else {
        /* Start the video stream. The library will call user function cb:
         *   cb(frame, (void *) 12345)
         */
        res = uvc_start_streaming(devh, &ctrl, cb, (void *) 12345, 0, 1);

        if (res < 0) {
            uvc_perror(res, "start_streaming"); /* unable to start stream */
        }
        else {
            puts("Streaming...");

            /* enable auto exposure - see uvc_set_ae_mode documentation */
            puts("Enabling auto exposure ...");
            const uint8_t UVC_AUTO_EXPOSURE_MODE_AUTO = 2;
            res = uvc_set_ae_mode(devh, UVC_AUTO_EXPOSURE_MODE_AUTO);
            if (res == UVC_SUCCESS) {
                puts(" ... enabled auto exposure");
            }
            else if (res == UVC_ERROR_PIPE) {
                /* this error indicates that the camera does not support the full AE mode;
                 * try again, using aperture priority mode (fixed aperture, variable exposure time) */
                puts(" ... full AE not supported, trying aperture priority mode");
                const uint8_t UVC_AUTO_EXPOSURE_MODE_APERTURE_PRIORITY = 8;
                res = uvc_set_ae_mode(devh, UVC_AUTO_EXPOSURE_MODE_APERTURE_PRIORITY);
                if (res < 0) {
                    uvc_perror(res, " ... uvc_set_ae_mode failed to enable aperture priority mode");
                }
                else {
                    puts(" ... enabled aperture priority auto exposure mode");
                }
            }
            else {
                uvc_perror(res, " ... uvc_set_ae_mode failed to enable auto exposure mode");
            }

            int16_t *brightness;
            printf("Brightness created\n");
            res = uvc_get_brightness(devh, brightness, uvc_req_code(0x00));
            printf("Attempt get Brightness\n");
            if (res < 0){
                cout << "res: " << res << endl;
                cout << "error" << endl;
                uvc_strerror(res);
            }
            else{
                cout << "res: " << res << endl;
                cout << "Brightness is: " << brightness << endl;
            }


            sleep(3); /* stream for 10 seconds */

            /* End the stream. Blocks until last callback is serviced */
            uvc_stop_streaming(devh);
            puts("Done streaming.");
        }

        /* Release our handle on the device */
        uvc_close(devh);
        puts("Device closed");
        tjDestroy(_jpegDecompressor);
    }

    cout << "# of runs: " << run_count << endl;
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
