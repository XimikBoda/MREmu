#include "Android.h"

#ifdef ANDROID
#include <jni.h>
#include "AppContext.h"
#include "AppManagerEvents.h"
#include <atomic>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

static std::atomic<int> storage_permission_state{ 0 };

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_notifyPermissionState(JNIEnv* env, jobject thiz, jboolean granted) {
    storage_permission_state = granted ? 1 : -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_nativeLoadVxpFile(JNIEnv* env, jobject thiz, jint fd, jstring j_path) {
    const char* path_cstr = env->GetStringUTFChars(j_path, nullptr);

    while (!AppContext::get_app_manager())
        sf::sleep(sf::seconds(0.2));

    AppContext::push_event(Event::launch{ Event::fd_app{fd, path_cstr} });

    env->ReleaseStringUTFChars(j_path, path_cstr);
}

void Android::wait_storage_permission(sf::RenderWindow &win) {
	while (win.isOpen()) {
		sf::Event event;
		while (win.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
                win.close();
			}
		}

		int perm_state = storage_permission_state.load();

		if (perm_state == 1)
			break;
		else if (perm_state == 0)
            win.clear(sf::Color::Black);
		else if (perm_state == -1)
            win.clear(sf::Color::Red);

        win.display();
	}
}

#endif