#pragma once

#ifndef ANDROID
class ScreenCorner {
public:
    ScreenCorner() {}
    int getRadius() const;
};

#else
#include <android/native_activity.h>
#include <jni.h>
#include <SFML/System/NativeActivity.hpp>

class ScreenCorner {
public:
    ScreenCorner();
    ~ScreenCorner();

    int getRadius() const;
private:
    JavaVM* vm_;
    jclass activityClass_;
    jmethodID getRadiusMethod_;
};

#endif