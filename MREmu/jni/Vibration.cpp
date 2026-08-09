#include "Vibration.h"

#ifndef ANDROID
bool Vibration::isAvailable() const { return false; }

void Vibration::vibrate(sf::Time) {}

#else

Vibration::Vibration() : vm_(nullptr), activityClass_(nullptr), vibrateMethod_(nullptr), isAvailable_(-1) {
    ANativeActivity* activity = sf::getNativeActivity();
    if (!activity || !activity->vm) return;

    vm_ = activity->vm;
    JNIEnv* env = nullptr;
    bool needDetach = false;

    if (vm_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        vm_->AttachCurrentThread(&env, nullptr);
        needDetach = true;
    }

    jclass localClass = env->GetObjectClass(activity->clazz);
    activityClass_ = (jclass)env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);

    vibrateMethod_ = env->GetMethodID(activityClass_, "triggerVibration", "(J)V");

    jmethodID checkMethod = env->GetMethodID(activityClass_, "hasVibrator", "()Z");
    if (checkMethod) {
        bool hasVib = env->CallBooleanMethod(activity->clazz, checkMethod);
        isAvailable_ = hasVib ? 1 : 0;
    }

    if (needDetach) {
        vm_->DetachCurrentThread();
    }
}

Vibration::~Vibration() {
    if (vm_ && activityClass_) {
        JNIEnv* env = nullptr;
        bool needDetach = false;
        if (vm_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
            vm_->AttachCurrentThread(&env, nullptr);
            needDetach = true;
        }

        env->DeleteGlobalRef(activityClass_);

        if (needDetach) {
            vm_->DetachCurrentThread();
        }
    }
}

bool Vibration::isAvailable() const {
    return isAvailable_ == 1;
}

void Vibration::vibrate(sf::Time duration) {
    if (isAvailable_ != 1 || !vm_ || !activityClass_ || !vibrateMethod_) return;

    JNIEnv* env = nullptr;
    bool needDetach = false;

    if (vm_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        vm_->AttachCurrentThread(&env, nullptr);
        needDetach = true;
    }

    ANativeActivity* activity = sf::getNativeActivity();
    env->CallVoidMethod(activity->clazz, vibrateMethod_, (jlong)duration.asMilliseconds());

    if (needDetach) {
        vm_->DetachCurrentThread();
    }
}
#endif