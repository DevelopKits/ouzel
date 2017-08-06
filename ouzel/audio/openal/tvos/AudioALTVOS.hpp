// Copyright (C) 2017 Elviss Strazdins
// This file is part of the Ouzel engine.

#pragma once

#include "core/CompileConfig.hpp"

#if OUZEL_PLATFORM_TVOS && OUZEL_SUPPORTS_OPENAL

#include "audio/openal/AudioAL.hpp"

namespace ouzel
{
    namespace audio
    {
        class AudioALTVOS: public AudioAL
        {
        public:
            virtual bool init(bool debugAudio) override;
        };
    }
}

#endif