#pragma once
#include <SFML/System/Time.hpp>

#ifndef ANDROID
class Vibration {
public:
	Vibration() {}
	bool isAvailable() const;

	void vibrate(sf::Time);
	void stop();
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
	void stop();
private:
    void init();

    JavaVM* vm_;
    jclass activityClass_;
    jobject activityObject_;
    jmethodID vibrateMethod_;
    int isAvailable_;
};

#endif

void mremu_vibrate(sf::Time duration);
void mremu_vibrate_stop();