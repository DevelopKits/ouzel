// Copyright (C) 2018 Elviss Strazdins
// This file is part of the Ouzel engine.

#include "core/Setup.h"

#if OUZEL_PLATFORM_TVOS && OUZEL_COMPILE_OPENAL

#include "AudioDeviceALTVOS.hpp"
#import <AVFoundation/AVFoundation.h>

namespace ouzel
{
    namespace audio
    {
        bool AudioDeviceALTVOS::init(bool debugAudio)
        {
            if (!AudioDeviceAL::init(debugAudio))
            {
                return false;
            }

            [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:nil];

            return true;
        }
    }
}

#endif
