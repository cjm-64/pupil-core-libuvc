# pupil-core-libuvc
In progress: Open Pupil Core video stream with c++

Using OpenCV 4.6.0 & the pupil labs version of libuvc

Compile in command line using 
```
g++ main.cpp -o out `pkg-config --cflags --libs opencv4` -luvc 
```
