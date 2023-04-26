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

void getNames(char* devnames, uvc_device_t **device_list){
    uvc_device_t * dev;
    uvc_device_descriptor_t *desc;
    char dummy[1];
    int i = 0;
    while (true){
        dev = device_list[i];
        if (dev == NULL){
            break;
        }
        else if (uvc_get_device_descriptor(dev, &desc) == UVC_SUCCESS){
            cout << "Product: " << desc->product << endl;
            devnames[i] = *desc->product;
        }
        uvc_free_device_descriptor(desc);
        i++;
    }
}

int main() {
    uvc_context_t *ctx;
    uvc_device_t **device_list;
    uvc_device_t * dev;
    uvc_error_t res;


    res = uvc_init(&ctx,NULL);
    if(res < 0){
        uvc_perror(res, "uvc_init");
    }

    res = uvc_get_device_list(ctx,&device_list);
    if (res < 0) {
        uvc_perror(res, "uvc_find_device");
    }
    int i = 0;

    while (true){
        dev = device_list[i];
        if (dev == NULL){
            break;
        }
        i++;
    }

    char device_names[i];
    getNames(device_names, device_list);

    for (int j = 0; j < sizeof(device_names); j++){
        cout << "Name: " << device_names[j][] << endl;
    }


    std::cout << "Hello, World!" << std::endl;
    return 0;
}
