#include "Vibration.h"

#ifndef ANDROID
bool Vibration::isAvailable() const { return false; }

void Vibration::vibrate(sf::Time) {}

#else

namespace {
// The SFML render thread is a plain pthread, not a JVM thread, so every JNI call
// from it has to attach first. Attaching and then detaching costs a full ART
// thread setup and teardown, and vibrate() runs once per key press, so once a
// thread has attached it stays attached for good. A leaked attachment when the
// thread finally exits is harmless: the VM reclaims it as the thread dies.
bool ensure_env(JNIEnv*& env, JavaVM* vm) {
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_EDETACHED)
        return env != nullptr;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
        return false;
    return env != nullptr;
}
} // namespace

Vibration::Vibration() : vm_(nullptr), activityClass_(nullptr), activityObject_(nullptr), vibrateMethod_(nullptr), isAvailable_(-1) {
    ANativeActivity* activity = sf::getNativeActivity();
    if (!activity || !activity->vm) return;

    vm_ = activity->vm;
    JNIEnv* env = nullptr;
    if (!ensure_env(env, vm_)) {
        vm_ = nullptr;
        return;
    }

    jclass localClass = env->GetObjectClass(activity->clazz);
    if (!localClass) {
        env->ExceptionClear();
        vm_ = nullptr;
        return;
    }
    activityClass_ = (jclass)env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);
    if (!activityClass_) {
        env->ExceptionClear();
        vm_ = nullptr;
        return;
    }

    // Cache the activity itself too. sf::getNativeActivity() is a module global
    // that SFML resets to null in onDestroy, and activity->clazz is only
    // guaranteed valid on the ANativeActivity thread, whereas vibrate() runs on
    // the render thread.
    activityObject_ = env->NewGlobalRef(activity->clazz);
    if (!activityObject_) {
        env->ExceptionClear();
        isAvailable_ = 0;
        return;
    }

    vibrateMethod_ = env->GetMethodID(activityClass_, "triggerVibration", "(J)V");
    if (!vibrateMethod_) {
        // Must clear before touching JNI again: a pending NoSuchMethodError
        // followed by another JNI call is undefined and aborts the process.
        // Checked even though proguard-rules.pro keeps the member, because the
        // keep rule protects the name, it does not guarantee the lookup works.
        env->ExceptionClear();
        isAvailable_ = 0;
        return;
    }

    jmethodID checkMethod = env->GetMethodID(activityClass_, "hasVibrator", "()Z");
    if (checkMethod) {
        bool hasVib = env->CallBooleanMethod(activityObject_, checkMethod);
        if (env->ExceptionCheck())
            env->ExceptionClear();
        else
            isAvailable_ = hasVib ? 1 : 0;
    }
}

Vibration::~Vibration() {
    if (!vm_) return;

    JNIEnv* env = nullptr;
    if (!ensure_env(env, vm_)) return;

    if (activityObject_) env->DeleteGlobalRef(activityObject_);
    if (activityClass_) env->DeleteGlobalRef(activityClass_);
}

bool Vibration::isAvailable() const {
    return isAvailable_ == 1;
}

void Vibration::vibrate(sf::Time duration) {
    if (isAvailable_ != 1 || !vm_ || !activityClass_ || !activityObject_ || !vibrateMethod_) return;

    JNIEnv* env = nullptr;
    if (!ensure_env(env, vm_)) return;

    env->CallVoidMethod(activityObject_, vibrateMethod_, (jlong)duration.asMilliseconds());

    // MainActivity.triggerVibration does unchecked casts on getSystemService(), so
    // this can throw. Clearing it here is what stops the next key press from
    // making JNI calls with an exception still pending.
    if (env->ExceptionCheck())
        env->ExceptionClear();
}
#endif