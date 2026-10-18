# Called from native code by name through JNI GetMethodID. R8 would otherwise
# rename/remove them and the JNI lookup would silently fail at runtime (the
# caller swallows the lookup failure), breaking the options sheet and vibration.
#   showOptionsDialogUi -> MREmu/MREmu.cpp (request_options_dialog)
#   showErrorDialogUi -> MREmu/MREmu.cpp (request_error_dialog)
#   triggerHapticFeedback -> MREmu/MREmu.cpp (trigger_options_haptic)
#   triggerVibration, hasVibrator -> MREmu/jni/Vibration.cpp
-keepclassmembers class com.ximikboda.mremu.MainActivity {
    void showOptionsDialogUi();
    void showErrorDialogUi(java.lang.String);
    void triggerHapticFeedback(int);
    void triggerVibration(long);
    boolean hasVibrator();
}