#pragma once
#include <string>
#include <functional>

#ifndef ANDROID
class AndroidIme {
public:
    AndroidIme() = default;
    ~AndroidIme() = default;

    void open(const std::u16string&, int, int = 0) {}
    void close() {}
    void setSelection(int) {}
    void setText(const std::u16string&, int) {}
    void setVisible(bool) {}
};
#else
#include <android/native_activity.h>
#include <jni.h>
#include <SFML/System/NativeActivity.hpp>

class AndroidIme {
public:
    AndroidIme();
    ~AndroidIme();

    void open(const std::u16string& initialText, int maxSize, int inputTypeFlags = 0);
    void close();
    void setSelection(int pos);
    void setText(const std::u16string& newText, int cursorPos);
    void setVisible(bool visible);

private:
    JavaVM* vm_;
    jclass activityClass_;
    jmethodID openMethod_;
    jmethodID closeMethod_;
    jmethodID setSelectionMethod_;
    jmethodID setTextMethod_;
    jmethodID setVisibleMethod_;
};
#endif