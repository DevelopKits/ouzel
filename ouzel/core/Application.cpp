// Copyright (C) 2017 Elviss Strazdins
// This file is part of the Ouzel engine.

#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include "Application.h"

namespace ouzel
{
    ouzel::Application* sharedApplication = nullptr;

    Application::Application():
        screenSaverEnabled(false)
    {
        sharedApplication = this;
    }

    Application::Application(int aArgc, char* aArgv[]):
        argc(aArgc), argv(aArgv)
    {
        sharedApplication = this;

        for (int i = 0; i < argc; ++i)
        {
            args.push_back(argv[i]);
        }
    }

    Application::Application(const std::vector<std::string>& aArgs):
        args(aArgs)
    {
        sharedApplication = this;
    }

    Application::~Application()
    {
        sharedApplication = nullptr;
    }

    int Application::run()
    {
        return EXIT_SUCCESS;
    }

    void Application::exit()
    {
        active = false;

        if (sharedEngine)
        {
            sharedEngine->exit();
        }
    }

    void Application::execute(const std::function<void(void)>& func)
    {
        std::lock_guard<std::mutex> lock(executeMutex);

        executeQueue.push(func);
    }

    void Application::executeAll()
    {
        std::function<void(void)> func;

        for (;;)
        {
            {
                std::lock_guard<std::mutex> lock(executeMutex);

                if (executeQueue.empty())
                {
                    break;
                }

                func = std::move(executeQueue.front());
                executeQueue.pop();
            }

            if (func)
            {
                func();
            }
        }
    }

    bool Application::openURL(const std::string&)
    {
        return false;
    }

    void Application::setScreenSaverEnabled(bool newScreenSaverEnabled)
    {
        screenSaverEnabled = newScreenSaverEnabled;
    }
}
