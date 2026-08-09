#pragma once
#include <SFML/System/Time.hpp>

#ifndef ANDROID
class Vibration {
public:
	Vibration() {}
	bool isAvailable() const;

	void vibrate(sf::Time);
};

#else
#include <android/native_activity.h>
#include <jni.h>
#include <SFML/System/NativeActivity.hpp>

class Vibration {
public:
	Vibration();

	~Vibration();

	bool isAvailable() const;
	void vibrate(sf::Time duration);
private:
    JavaVM* vm_;
    jclass activityClass_;
    jmethodID vibrateMethod_;
    int isAvailable_;
};

#endif