# Called from native code by name through JNI GetMethodID. R8 would otherwise
# rename/remove them and the JNI lookup would silently fail at runtime (the
# caller swallows the lookup failure), breaking the options sheet and vibration.
#   showOptionsDialogUi -> MREmu/MREmu.cpp (request_options_dialog)
#   triggerVibration, hasVibrator -> MREmu/jni/Vibration.cpp
-keepclassmembers class com.ximikboda.mremu.MainActivity {
    void showOptionsDialogUi();
    void triggerVibration(long);
    boolean hasVibrator();
}