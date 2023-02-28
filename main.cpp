#include <iostream>
#include "stdio.h"
#include "unistd.h"
#include "string"
#include "libuvc/libuvc.h"

// Computer Vision
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace cv;
using namespace std;

int run_count = 0;

Mat RedGreenBlue;

void cb(uvc_frame_t *frame, void *ptr) {
    uvc_frame_t *bgr;
    uvc_error_t ret;
    enum uvc_frame_format *frame_format = (enum uvc_frame_format *)ptr;
    FILE *fp;
    static int jpeg_count = 0;
    static const char *H264_FILE = "iOSDevLog.h264";
    static const char *MJPEG_FILE = ".jpeg";
    char filename[16];

    /* We'll convert the image from YUV/JPEG to BGR, so allocate space */
    bgr = uvc_allocate_frame(frame->width * frame->height * 3);
    if (!bgr) {
        printf("unable to allocate bgr frame!\n");
        return;
    }

    printf("callback! frame_format = %d, width = %d, height = %d, length = %lu\n",frame->frame_format, frame->width, frame->height, frame->data_bytes);



    cout << "Frame num: " << frame->sequence << endl;
    //cout << "bRows: " << bgr->width << " bCols: " << bgr->width << endl;
    cout << "fRows: " << frame->width << " fCols: " << frame->width << endl;
    Mat image(frame->width, frame->height, CV_8UC2, frame->data);
    cout << "Mat type: " << image.type() << endl;
//    for (int i = 0; i < 9; i++){
//        cout << "0," << to_string(i) << " :" << image.at<double>(0,i) << endl;
//    }
    cout << "iRows: " << image.rows << " iCols: " << image.cols << endl;

    cvtColor(image,RedGreenBlue,COLOR_YUV2RGBA_YUY2);
    namedWindow("Test", WINDOW_AUTOSIZE);
    printf("create win\n");
    imshow("Test",RedGreenBlue);
    printf("img shown\n");
    waitKey(1);
    printf("waitkeyed\n");
    image.release();
    RedGreenBlue.release();
    printf("img released\n");

    run_count = run_count + 1;
    uvc_free_frame(bgr);
}

int main() {
    uvc_context_t *ctx;
    uvc_device_t *dev;
    uvc_device_t **listd;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    uvc_error_t res;

    res = uvc_init(&ctx, NULL);

    if (res < 0) {
        uvc_perror(res, "uvc_init");
        return res;
    }

    res = uvc_find_device(ctx, &dev, 0, 0, NULL); /* filter devices: vendor_id, product_id, "serial_num" */

    if (res < 0) {
        uvc_perror(res, "uvc_find_device"); /* no devices found */
    }
    else {
        puts("Device found");
    }

    /* Try to open the device: requires exclusive access */
    res = uvc_open(dev, &devh);

    if (res < 0) {
        uvc_perror(res, "uvc_open"); /* unable to open device */
    }
    else {
        puts("Device opened");
    }

//    uvc_get_device_list(ctx, &listd); //Figure out how to cycle through the cameras

    /* Print out a message containing all the information that libuvc
      * knows about the device */

    uvc_print_diag(devh, stderr);

    const uvc_format_desc_t *format_desc;
    format_desc = uvc_get_format_descs(devh)->next;
    cout << "fd " << format_desc->frame_descs << endl;
    const uvc_frame_desc_t *frame_desc;
    frame_desc = format_desc->frame_descs->next->next->next->next->next->next;
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

    /* Try to negotiate first stream profile */ /* result stored in ctrl */  /* width, height, fps */
    res = uvc_get_stream_ctrl_format_size(devh, &ctrl,frame_format, width, height, fps);

    /* Print out the result */
    uvc_print_stream_ctrl(&ctrl, stderr);

    if (res < 0) {
        uvc_perror(res, "get_mode"); /* device doesn't provide a matching stream */
    }
    else {
        /* Start the video stream. The library will call user function cb:
         *   cb(frame, (void *) 12345)
         */
        res = uvc_start_streaming(devh, &ctrl, cb, (void *) 12345, 0);

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

            sleep(10); /* stream for 10 seconds */

            /* End the stream. Blocks until last callback is serviced */
            uvc_stop_streaming(devh);
            puts("Done streaming.");
        }

        /* Release our handle on the device */
        uvc_close(devh);
        puts("Device closed");
    }

    cout << "# of runs: " << run_count;
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
