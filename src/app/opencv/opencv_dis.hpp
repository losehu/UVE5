#pragma once

#include <GLFW/glfw3.h>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

bool getScreenSize(int& width, int& height);
int OPENCV_PollKey(void);


