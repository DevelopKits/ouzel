// Copyright (C) 2016 Elviss Strazdins
// This file is part of the Ouzel engine.

#import "MetalView.h"
#include "core/Engine.h"
#include "core/Window.h"
#include "input/Input.h"
#include "graphics/metal/RendererMetal.h"
#include "utils/Utils.h"

using namespace ouzel;

@interface ViewDelegate: NSObject<MTKViewDelegate>

@end

@implementation ViewDelegate

-(void)mtkView:(nonnull __unused MTKView *)view drawableSizeWillChange:(CGSize)size
{
    std::shared_ptr<ouzel::graphics::RendererMetal> rendererMetal = std::static_pointer_cast<ouzel::graphics::RendererMetal>(sharedEngine->getRenderer());
    rendererMetal->handleResize(ouzel::Size2(static_cast<float>(size.width),
                                             static_cast<float>(size.height)));
}

-(void)drawInMTKView:(nonnull __unused MTKView*)view
{
    if (sharedEngine->isRunning() && !sharedEngine->draw())
    {
        // iOS app should not be exited
    }
}

@end

@implementation MetalView
{
    id<MTKViewDelegate> viewDelegate;
}

-(id)initWithFrame:(CGRect)frameRect
{
    if (self = [super initWithFrame:frameRect])
    {
        viewDelegate = [[ViewDelegate alloc] init];
        self.delegate = viewDelegate;
    }

    return self;
}

-(void)dealloc
{
    [self close];
    [super dealloc];
}

-(void)close
{
    if (viewDelegate)
    {
        [viewDelegate release];
        viewDelegate = Nil;
        self.delegate = Nil;
    }
}

-(void)touchesBegan:(NSSet*)touches withEvent:(__unused ::UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        CGPoint location = [touch locationInView:self];

        sharedEngine->getInput()->touchBegin(reinterpret_cast<uint64_t>(touch),
                                             sharedEngine->getWindow()->convertWindowToNormalizedLocation(Vector2(location.x, location.y)));
    }
}

-(void)touchesMoved:(NSSet*)touches withEvent:(__unused ::UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        CGPoint location = [touch locationInView:self];

        sharedEngine->getInput()->touchMove(reinterpret_cast<uint64_t>(touch),
                                            sharedEngine->getWindow()->convertWindowToNormalizedLocation(Vector2(location.x, location.y)));
    }
}

-(void)touchesEnded:(NSSet*)touches withEvent:(__unused ::UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        CGPoint location = [touch locationInView:self];

        sharedEngine->getInput()->touchEnd(reinterpret_cast<uint64_t>(touch),
                                           sharedEngine->getWindow()->convertWindowToNormalizedLocation(Vector2(location.x, location.y)));
    }
}

-(void)touchesCancelled:(NSSet*)touches withEvent:(__unused ::UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        CGPoint location = [touch locationInView:self];

        sharedEngine->getInput()->touchCancel(reinterpret_cast<uint64_t>(touch),
                                              sharedEngine->getWindow()->convertWindowToNormalizedLocation(Vector2(location.x, location.y)));
    }
}

@end
