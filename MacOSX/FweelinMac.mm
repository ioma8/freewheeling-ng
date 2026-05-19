//
//  FweelinMac.mm
//  fweelin
//

#import <Cocoa/Cocoa.h>
#include <SDL2/SDL.h>
#include "FweelinMac.h"
#include "SDLMain.h"

#include "fweelin_core.h"

void *FweelinMac::sdlmain = 0;
void *FweelinMac::fweelin = 0;

void FweelinMac::ClearMIDIInputList() {
	// printf("***SDLMAIN: %p\n",sdlmain);
	
	SDLMain *s = (SDLMain *)sdlmain;
	[s clearMIDIInputList];
};

void FweelinMac::AddMIDIInputSource(char *name) {
	SDLMain *s = (SDLMain *)sdlmain;
	[s addMIDIInputSource:[NSString stringWithUTF8String:name]];
};

void FweelinMac::SetMIDIInput(int idx) {
	// Connect MIDI 
	Fweelin *fw = (Fweelin *)fweelin;
	
	fw->getMIDI()->SetMIDIInput(idx);
};

void FweelinMac::SetDebugMode(int active) {
	Fweelin *fw = (Fweelin *)fweelin;

	ShowDebugInfoEvent *devt = (ShowDebugInfoEvent *) 
		Event::GetEventByType(T_EV_ShowDebugInfo);  
	devt->show = active;
	fw->getEMG()->BroadcastEventNow(devt, fw);
};

void FweelinMac::Quit() {
	Fweelin *fw = (Fweelin *)fweelin;

	ExitSessionEvent *evt = (ExitSessionEvent *) 
		Event::GetEventByType(T_EV_ExitSession);  
	fw->getEMG()->BroadcastEventNow(evt, fw);
};

void FweelinMac::ShowHelp() {
	Fweelin *fw = (Fweelin *)fweelin;
	
	VideoShowHelpEvent *evt = (VideoShowHelpEvent *) 
		Event::GetEventByType(T_EV_VideoShowHelp);  
	evt->page = 1;
	fw->getEMG()->BroadcastEventNow(evt, fw);
};

SDL_Surface *FweelinMac::LoadImage(const char *path) {
  @autoreleasepool {
    if (path == nullptr)
      return nullptr;

    NSString *imagePath = [NSString stringWithUTF8String:path];
    NSImage *image = [[NSImage alloc] initWithContentsOfFile:imagePath];
    if (image == nil)
      return nullptr;

    NSSize size = [image size];
    const int width = (int) size.width;
    const int height = (int) size.height;
    if (width < 1 || height < 1)
      return nullptr;

    SDL_Surface *surface = SDL_CreateRGBSurface(
        0, width, height, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (surface == nullptr)
      return nullptr;

    unsigned char *planes[5] = {(unsigned char *) surface->pixels, nullptr,
                                nullptr, nullptr, nullptr};
    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:planes
                      pixelsWide:width
                      pixelsHigh:height
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:surface->pitch
                    bitsPerPixel:32];
    if (bitmap == nil) {
      SDL_FreeSurface(surface);
      return nullptr;
    }

    NSGraphicsContext *context =
        [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:context];
    [[NSColor clearColor] set];
    NSRectFill(NSMakeRect(0, 0, width, height));
    [image drawInRect:NSMakeRect(0, 0, width, height)
             fromRect:NSZeroRect
            operation:NSCompositingOperationCopy
             fraction:1.0];
    [context flushGraphics];
    [NSGraphicsContext restoreGraphicsState];

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    return surface;
  }
}

// Performs initialization on a new pthread to make it behave well with Cocoa
void FweelinMac::SetupCocoaThread() {
	// printf("MULTITHREADED: %d\n",[NSThread isMultiThreaded]);	
	autoreleasepool = [[NSAutoreleasePool alloc] init];
};

void FweelinMac::TakedownCocoaThread() {
	NSAutoreleasePool *tmp = (NSAutoreleasePool *) autoreleasepool;
	[tmp release];
};
