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

struct CamSettings{
    string CamName;
    int width;
    int height;
    int fps;
};

// ID0 = right, ID1 = Left, ID2 = World
struct CamInfo{
    string CamName;
    int vID;
    int pID;
//    int DevAddr;
//    int BusNum;
    string uid;
};

struct StreamingInfo{
    uvc_context_t *ctx;
    uvc_device_t *dev;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    uvc_stream_handle_t *strmh;
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
    int flag = 0;
    while (true) {
        cout << i << endl;
        dev = device_list[i];
        if (dev == NULL) {
            break;
        }
        uvc_get_device_descriptor(dev, &desc);
        if (desc->idVendor != 3141){
            printf("World Cam\n");
            flag = 1;
            i++;
            continue;
        }
        else{
            if (flag == 1){
                ci[i-1].CamName = desc->product;
                cout << desc->product << endl;
                ci[i-1].vID = desc->idVendor;
                cout << desc->idVendor << endl;
                ci[i-1].pID = desc->idProduct;
                cout << desc->idProduct << endl;
//            cout << "DA type: " << typeid(uvc_get_device_address(dev)).name() << endl;
//            ci[i].DevAddr = uvc_get_device_address(dev);
//            printf("Dev Addr: %d", uvc_get_device_address(dev));
//            ci[i].BusNum = uvc_get_bus_number(dev);
//            printf("Bus Num: %d", uvc_get_bus_number(dev));
                cout << to_string(uvc_get_device_address(dev))+":"+to_string(uvc_get_bus_number(dev)) << endl;
                ci[i-1].uid = to_string(uvc_get_device_address(dev))+":"+to_string(uvc_get_bus_number(dev));
            }
            else{
                ci[i].CamName = desc->product;
                cout << desc->product << endl;
                ci[i].vID = desc->idVendor;
                cout << desc->idVendor << endl;
                ci[i].pID = desc->idProduct;
                cout << desc->idProduct << endl;
//            cout << "DA type: " << typeid(uvc_get_device_address(dev)).name() << endl;
//            ci[i].DevAddr = uvc_get_device_address(dev);
//            printf("Dev Addr: %d", uvc_get_device_address(dev));
//            ci[i].BusNum = uvc_get_bus_number(dev);
//            printf("Bus Num: %d", uvc_get_bus_number(dev));
                cout << to_string(uvc_get_device_address(dev))+":"+to_string(uvc_get_bus_number(dev)) << endl;
                ci[i].uid = to_string(uvc_get_device_address(dev))+":"+to_string(uvc_get_bus_number(dev));
            }

        }
        uvc_free_device_descriptor(desc);
        i++;
    }
    uvc_exit(ctx);
}

void setUpStreams(struct StreamingInfo *si){

    uvc_error_t res;

    for(int i = 0; i<2;i++){
        res = uvc_init(&si->ctx, NULL);
        if (res < 0) {
            uvc_perror(res, "uvc_init");
        }
        else{
            printf("UVC %d open success\n", i);
        }
    }

}

int main() {
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

    CamInfo Cameras[0];
    getCamInfo(Cameras);

    for (int j = 0; j<2; j++){
        cout << "Cam " << j << ": " << Cameras[j].CamName << " " << Cameras[j].uid << " " << Cameras[j].vID << " " << Cameras[j].pID << endl;
    }

    StreamingInfo CamStreams[2];
    setUpStreams(CamStreams);



    std::cout << "Hello, World!" << std::endl;
    return 0;
}
