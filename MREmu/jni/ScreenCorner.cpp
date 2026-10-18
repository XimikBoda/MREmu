#include "ScreenCorner.h"

#ifndef ANDROID
int ScreenCorner::getRadius() const { 
    return 0; 
}

#else

ScreenCorner::ScreenCorner() : vm_(nullptr), activityClass_(nullptr), getRadiusMethod_(nullptr) {
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

    getRadiusMethod_ = env->GetMethodID(activityClass_, "getScreenCornerRadius", "()I");

    if (needDetach)
        vm_->DetachCurrentThread();
}

ScreenCorner::~ScreenCorner() {
    if (vm_ && activityClass_) {
        JNIEnv* env = nullptr;
        bool needDetach = false;
        
        if (vm_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
            vm_->AttachCurrentThread(&env, nullptr);
            needDetach = true;
        }

        env->DeleteGlobalRef(activityClass_);

        if (needDetach)
            vm_->DetachCurrentThread();
    }
}

int ScreenCorner::getRadius() const {
    if (!vm_ || !activityClass_ || !getRadiusMethod_) return 0;

    JNIEnv* env = nullptr;
    bool needDetach = false;

    if (vm_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        vm_->AttachCurrentThread(&env, nullptr);
        needDetach = true;
    }

    ANativeActivity* activity = sf::getNativeActivity();
    
    jint radius = env->CallIntMethod(activity->clazz, getRadiusMethod_);

    if (needDetach)
        vm_->DetachCurrentThread();

    return (int)radius;
}
#endif