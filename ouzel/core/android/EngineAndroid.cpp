// Copyright (C) 2018 Elviss Strazdins
// This file is part of the Ouzel engine.

#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <android/window.h>
#include "EngineAndroid.hpp"
#include "WindowResourceAndroid.hpp"
#include "events/EventDispatcher.hpp"
#include "graphics/opengl/android/RenderDeviceOGLAndroid.hpp"
#include "utils/Log.hpp"

static int looperCallback(int fd, int events, void* data)
{
    if (events & ALOOPER_EVENT_INPUT)
    {
        char message;
        if (read(fd, &message, sizeof(message)) == -1)
        {
            ouzel::Log(ouzel::Log::Level::ERR) << "Failed to read from pipe";
            return 0;
        }

        ouzel::EngineAndroid* engineAndroid = static_cast<ouzel::EngineAndroid*>(data);
        engineAndroid->executeAll();
    }

    return 1;
}

namespace ouzel
{
    EngineAndroid::EngineAndroid(JavaVM* initJavaVM):
        javaVM(initJavaVM)
    {
        std::fill(std::begin(fd), std::end(fd), 0);

        JNIEnv* jniEnv;

        if (javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6) != JNI_OK)
        {
            Log(Log::Level::ERR) << "Failed to get JNI environment";
            return;
        }

        uriClass = jniEnv->FindClass("android/net/Uri");
        uriClass = static_cast<jclass>(jniEnv->NewGlobalRef(uriClass));
        parseMethod = jniEnv->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
        intentClass = jniEnv->FindClass("android/content/Intent");
        intentClass = static_cast<jclass>(jniEnv->NewGlobalRef(intentClass));
        intentConstructor = jniEnv->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
    }

    EngineAndroid::~EngineAndroid()
    {
        if (updateThread.joinable()) updateThread.join();

        JNIEnv* jniEnv;

        if (javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6) != JNI_OK)
        {
            Log(Log::Level::ERR) << "Failed to get JNI environment";
            return;
        }

        if (mainActivity) jniEnv->DeleteGlobalRef(mainActivity);
        if (androidWindow) jniEnv->DeleteGlobalRef(androidWindow);
        if (surface) jniEnv->DeleteGlobalRef(surface);
        if (intentClass) jniEnv->DeleteGlobalRef(intentClass);
        if (uriClass) jniEnv->DeleteGlobalRef(uriClass);
        if (looper) ALooper_release(looper);
        if (fd[0]) close(fd[0]);
        if (fd[1]) close(fd[1]);
    }

    void EngineAndroid::onCreate(jobject initMainActivity)
    {
        JNIEnv* jniEnv;

        if (javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6) != JNI_OK)
        {
            Log(Log::Level::ERR) << "Failed to get JNI environment";
            return;
        }

        mainActivity = jniEnv->NewGlobalRef(initMainActivity);

        jclass mainActivityClass = jniEnv->GetObjectClass(mainActivity);
        startActivityMethod = jniEnv->GetMethodID(mainActivityClass, "startActivity", "(Landroid/content/Intent;)V");

        // get resources
        jclass resourcesClass = jniEnv->FindClass("android/content/res/Resources");
        jmethodID getResourcesMethod = jniEnv->GetMethodID(mainActivityClass, "getResources", "()Landroid/content/res/Resources;");
        jobject resourcesObject = jniEnv->CallObjectMethod(mainActivity, getResourcesMethod);
        jmethodID getConfigurationMethod = jniEnv->GetMethodID(resourcesClass, "getConfiguration", "()Landroid/content/res/Configuration;");

        // get configuration
        configurationClass = jniEnv->FindClass("android/content/res/Configuration");
        jobject configurationObject = jniEnv->CallObjectMethod(resourcesObject, getConfigurationMethod);

        orientationField = jniEnv->GetFieldID(configurationClass, "orientation", "I");
        orientation = jniEnv->GetIntField(configurationObject, orientationField);

        // get asset manager
        jmethodID getAssetsMethod = jniEnv->GetMethodID(mainActivityClass, "getAssets", "()Landroid/content/res/AssetManager;");
        jobject assetManagerObject = jniEnv->CallObjectMethod(mainActivity, getAssetsMethod);
        assetManager = AAssetManager_fromJava(jniEnv, assetManagerObject);

        // get window
        jmethodID getWindowMethod = jniEnv->GetMethodID(mainActivityClass, "getWindow", "()Landroid/view/Window;");
        androidWindow = jniEnv->CallObjectMethod(mainActivity, getWindowMethod);
        androidWindow = jniEnv->NewGlobalRef(androidWindow);

        jclass windowClass = jniEnv->FindClass("android/view/Window");
        addFlagsMethod = jniEnv->GetMethodID(windowClass, "addFlags", "(I)V");
        clearFlagsMethod = jniEnv->GetMethodID(windowClass, "clearFlags", "(I)V");

        // File class
        jclass fileClass = jniEnv->FindClass("java/io/File");
        jmethodID getAbsolutePathMethod = jniEnv->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");

        // dataDir
        jmethodID getFilesDirMethod = jniEnv->GetMethodID(mainActivityClass, "getFilesDir", "()Ljava/io/File;");
        jobject filesDirFile = jniEnv->CallObjectMethod(mainActivity, getFilesDirMethod);

        jstring filesDirString = static_cast<jstring>(jniEnv->CallObjectMethod(filesDirFile, getAbsolutePathMethod));
        const char* filesDirCString = jniEnv->GetStringUTFChars(filesDirString, 0);
        filesDirectory = filesDirCString;
        jniEnv->ReleaseStringUTFChars(filesDirString, filesDirCString);

        // cacheDir
        jmethodID getCacheDirMethod = jniEnv->GetMethodID(mainActivityClass, "getCacheDir", "()Ljava/io/File;");
        jobject cacheDirFile = jniEnv->CallObjectMethod(mainActivity, getCacheDirMethod);

        jstring cacheDirString = static_cast<jstring>(jniEnv->CallObjectMethod(cacheDirFile, getAbsolutePathMethod));
        const char* cacheDirCString = jniEnv->GetStringUTFChars(cacheDirString, 0);
        cacheDirectory = cacheDirCString;
        jniEnv->ReleaseStringUTFChars(cacheDirString, cacheDirCString);

        // looper
        looper = ALooper_forThread(); // this is called on main thread, so it is safe to get the looper here
        ALooper_acquire(looper);
        pipe(fd);
        if (ALooper_addFd(looper, fd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, looperCallback, this) != 1)
        {
            Log(Log::Level::ERR) << "Failed to add looper file descriptor";
        }
    }

    void EngineAndroid::onSurfaceCreated(jobject newSurface)
    {
        JNIEnv* jniEnv;

        if (javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6) != JNI_OK)
        {
            Log(Log::Level::ERR) << "Failed to get JNI environment";
            return;
        }

        if (surface) jniEnv->DeleteGlobalRef(surface);
        surface = jniEnv->NewGlobalRef(newSurface);

        if (active)
        {
            WindowResourceAndroid* windowAndroid = static_cast<WindowResourceAndroid*>(window.getResource());
            windowAndroid->handleSurfaceChange(surface);

            if (renderer)
            {
                graphics::RenderDevice* renderDevice = renderer->getDevice();
                if (renderDevice->getDriver() == graphics::Renderer::Driver::OPENGL)
                {
                    graphics::RenderDeviceOGLAndroid* renderDeviceOGLAndroid = static_cast<graphics::RenderDeviceOGLAndroid*>(renderDevice);
                    renderDeviceOGLAndroid->reload();
                }
            }
        }
    }

    void EngineAndroid::onConfigurationChanged(jobject newConfig)
    {
        JNIEnv* jniEnv;
        
        if (javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6) != JNI_OK)
        {
            Log(Log::Level::ERR) << "Failed to get JNI environment";
            return;
        }

        jint newOrientation = jniEnv->GetIntField(newConfig, orientationField);

        if (orientation != newOrientation)
        {
            orientation = newOrientation;

            ouzel::Event event;
            event.type = ouzel::Event::Type::ORIENTATION_CHANGE;
        
            switch (orientation)
            {
                case ORIENTATION_PORTRAIT:
                    event.systemEvent.orientation = ouzel::SystemEvent::Orientation::PORTRAIT;
                    break;
                case ORIENTATION_LANDSCAPE:
                    event.systemEvent.orientation = ouzel::SystemEvent::Orientation::LANDSCAPE;
                    break;
                default:
                    event.systemEvent.orientation = ouzel::SystemEvent::Orientation::UNKNOWN;
                    break;
            }
        
            eventDispatcher.postEvent(event);
        }
    }

    void EngineAndroid::onSurfaceDestroyed()
    {
        JNIEnv* jniEnv;

        if (javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6) != JNI_OK)
        {
            Log(Log::Level::ERR) << "Failed to get JNI environment";
            return;
        }

        if (surface)
        {
            jniEnv->DeleteGlobalRef(surface);
            surface = nullptr;
        }

        if (active)
        {
            WindowResourceAndroid* windowAndroid = static_cast<WindowResourceAndroid*>(window.getResource());
            windowAndroid->handleSurfaceDestroy();

            if (renderer)
            {
                graphics::RenderDevice* renderDevice = renderer->getDevice();
                if (renderDevice->getDriver() == graphics::Renderer::Driver::OPENGL)
                {
                    graphics::RenderDeviceOGLAndroid* renderDeviceOGLAndroid = static_cast<graphics::RenderDeviceOGLAndroid*>(renderDevice);
                    renderDeviceOGLAndroid->destroy();
                }
            }
        }
    }

    int EngineAndroid::run()
    {
        if (!init())
        {
            return EXIT_FAILURE;
        }

        start();

        return EXIT_SUCCESS;
    }

    void EngineAndroid::executeOnMainThread(const std::function<void(void)>& func)
    {
        {
            std::lock_guard<std::mutex> lock(executeMutex);

            executeQueue.push(func);
        }

        char message = 1;
        if (write(fd[1], &message, sizeof(message)) == -1)
        {
            Log(Log::Level::ERR) << "Failed to write to pipe";
        }
    }

    bool EngineAndroid::openURL(const std::string& url)
    {
        executeOnMainThread([url, this]() {
            JNIEnv* jniEnv;

            if (javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6) != JNI_OK)
            {
                Log(Log::Level::ERR) << "Failed to get JNI environment";
                return;
            }

            jstring actionString = jniEnv->NewStringUTF("android.intent.action.VIEW");
            jstring urlString = jniEnv->NewStringUTF(url.c_str());
            jobject uri = jniEnv->CallStaticObjectMethod(uriClass, parseMethod, urlString);
            jobject intentObject = jniEnv->NewObject(intentClass, intentConstructor, actionString, uri);

            jniEnv->CallVoidMethod(mainActivity, startActivityMethod, intentObject);

            jniEnv->DeleteLocalRef(intentObject);
            jniEnv->DeleteLocalRef(uri);
            jniEnv->DeleteLocalRef(urlString);
            jniEnv->DeleteLocalRef(actionString);

            if (jniEnv->ExceptionCheck())
            {
                Log(Log::Level::ERR) << "Failed to open URL";
                jniEnv->ExceptionClear();
            }
        });

        return true;
    }

    void EngineAndroid::setScreenSaverEnabled(bool newScreenSaverEnabled)
    {
        Engine::setScreenSaverEnabled(newScreenSaverEnabled);

        executeOnMainThread([newScreenSaverEnabled, this]() {
            JNIEnv* jniEnv;

            if (javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6) != JNI_OK)
            {
                Log(Log::Level::ERR) << "Failed to get JNI environment";
                return;
            }

            if (newScreenSaverEnabled)
            {
                jniEnv->CallVoidMethod(androidWindow, clearFlagsMethod, AWINDOW_FLAG_KEEP_SCREEN_ON);
            }
            else
            {
                jniEnv->CallVoidMethod(androidWindow, addFlagsMethod, AWINDOW_FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    bool EngineAndroid::setCurrentThreadName(const std::string& name)
    {
        return pthread_setname_np(pthread_self(), name.c_str()) == 0;
    }

    void EngineAndroid::executeAll()
    {
        std::function<void(void)> func;

        {
            std::lock_guard<std::mutex> lock(executeMutex);

            if (!executeQueue.empty())
            {
                func = std::move(executeQueue.front());
                executeQueue.pop();
            }
        }

        if (func)
        {
            func();
        }
    }

    void EngineAndroid::main()
    {
        JNIEnv* jniEnv;
        JavaVMAttachArgs attachArgs;
        attachArgs.version = JNI_VERSION_1_6;
        attachArgs.name = nullptr; // thread name
        attachArgs.group = nullptr; // thread group
        if (javaVM->AttachCurrentThread(&jniEnv, &attachArgs) != JNI_OK)
        {
            Log(Log::Level::ERR) << "Failed to attach current thread to Java VM";
        }

        Engine::main();

        if (javaVM->DetachCurrentThread() != JNI_OK)
        {
            Log(Log::Level::ERR) << "Failed to detach current thread from Java VM";
        }
    }
}
