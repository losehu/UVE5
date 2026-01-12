

#include <opencv2/opencv.hpp>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

 bool getScreenSize(int& width, int& height) ;
 void drawTest(cv::Mat& img) ;




