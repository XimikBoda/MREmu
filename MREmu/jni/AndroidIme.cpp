#include "AndroidIme.h"

#ifdef ANDROID
#include <mutex>

namespace {
    std::mutex g_cbMutex;

    struct JniScope {
        JavaVM* vm;
        JNIEnv* env = nullptr;
        bool needDetach = false;

        explicit JniScope(JavaVM* vm_) : vm(vm_) {
            if (!vm) return;
            if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
                vm->AttachCurrentThread(&env, nullptr);
                needDetach = true;
            }
        }

        ~JniScope() {
            if (vm && needDetach) {
                vm->DetachCurrentThread();
            }
        }
    };
}

AndroidIme::AndroidIme()
    : vm_(nullptr), activityClass_(nullptr),
    openMethod_(nullptr), closeMethod_(nullptr),
    setSelectionMethod_(nullptr), setTextMethod_(nullptr)
{
    ANativeActivity* activity = sf::getNativeActivity();
    if (!activity || !activity->vm) return;

    vm_ = activity->vm;
    JniScope jni(vm_);
    if (!jni.env) return;

    jclass localClass = jni.env->GetObjectClass(activity->clazz);
    activityClass_ = static_cast<jclass>(jni.env->NewGlobalRef(localClass));
    jni.env->DeleteLocalRef(localClass);

    openMethod_ = jni.env->GetMethodID(activityClass_, "openIme", "(Ljava/lang/String;II)V");
    closeMethod_ = jni.env->GetMethodID(activityClass_, "closeIme", "()V");
    setSelectionMethod_ = jni.env->GetMethodID(activityClass_, "setImeSelection", "(I)V");
    setTextMethod_ = jni.env->GetMethodID(activityClass_, "setImeText", "(Ljava/lang/String;I)V");
}

AndroidIme::~AndroidIme() {
    if (vm_ && activityClass_) {
        JniScope jni(vm_);
        if (jni.env) {
            jni.env->DeleteGlobalRef(activityClass_);
        }
    }
}

void AndroidIme::open(const std::u16string& initialText, int maxSize, int inputTypeFlags) {
    if (!vm_ || !openMethod_) return;
    JniScope jni(vm_);
    if (!jni.env) return;

    jstring jStr = jni.env->NewString(reinterpret_cast<const jchar*>(initialText.data()),
        static_cast<jsize>(initialText.size()));
    jni.env->CallVoidMethod(sf::getNativeActivity()->clazz, openMethod_, jStr, (jint)maxSize, (jint)inputTypeFlags);
    jni.env->DeleteLocalRef(jStr);
}

void AndroidIme::close() {
    if (!vm_ || !closeMethod_) return;
    JniScope jni(vm_);
    if (!jni.env) return;

    jni.env->CallVoidMethod(sf::getNativeActivity()->clazz, closeMethod_);
}

void AndroidIme::setSelection(int pos) {
    if (!vm_ || !setSelectionMethod_) return;
    JniScope jni(vm_);
    if (!jni.env) return;

    jni.env->CallVoidMethod(sf::getNativeActivity()->clazz, setSelectionMethod_, (jint)pos);
}

void AndroidIme::setText(const std::u16string& newText, int cursorPos) {
    if (!vm_ || !setTextMethod_) return;
    JniScope jni(vm_);
    if (!jni.env) return;

    jstring jStr = jni.env->NewString(reinterpret_cast<const jchar*>(newText.data()),
        static_cast<jsize>(newText.size()));
    jni.env->CallVoidMethod(sf::getNativeActivity()->clazz, setTextMethod_, jStr, (jint)cursorPos);
    jni.env->DeleteLocalRef(jStr);
}
#endif