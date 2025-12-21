#include "Platform.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>
#import <GameController/GameController.h>

#import <simd/simd.h>

#include <mach/mach_init.h>
#include <mach/mach_vm.h>
// #include <mach/task.h>

#include <dlfcn.h>

using FGameSurfaceDescFunc = FGameSurfaceDesc* (*)();
using FGameInitializeFunc = void (*)(FGameMemory&);
using FGameUpdateAndRenderFunc = void (*)(FGameMemory&, f32, FOffscreenBuffer&, const FInputGamepad&);

struct FGameModule
{
	void* LibraryHandle;
	FGameSurfaceDescFunc GameGetSurfaceDesc;
	FGameInitializeFunc GameInitialize;
	FGameUpdateAndRenderFunc GameUpdateAndRender;

	bool bIsValid;

	NSString* BasePath;
	NSString* HotLoadPath;
	NSDate* BaseLastWriteTime;
};

auto GameModuleLoad() -> FGameModule
{
	NSString* exeDir = [[[NSBundle mainBundle] executableURL]URLByDeletingLastPathComponent].path;
	NSString* src = [exeDir stringByAppendingPathComponent:[NSString stringWithFormat:@"libTestbed.dylib"]];
	static u32 LibraryLoadCounter = 0;
	NSString* dst = [exeDir stringByAppendingPathComponent:[NSString stringWithFormat:@"libTestbed_hot_reload_%d.dylib", LibraryLoadCounter++]];
	[[NSFileManager defaultManager] copyItemAtPath:src toPath:dst error:nil];

	FGameModule GameModule = {};
	GameModule.LibraryHandle = dlopen(dst.UTF8String, RTLD_NOW);
	if (GameModule.LibraryHandle)
	{
		GameModule.GameGetSurfaceDesc = (FGameSurfaceDescFunc)dlsym(GameModule.LibraryHandle, "GameGetSurfaceDesc");
		GameModule.GameInitialize = (FGameInitializeFunc)dlsym(GameModule.LibraryHandle, "GameInitialize");
		GameModule.GameUpdateAndRender = (FGameUpdateAndRenderFunc)dlsym(GameModule.LibraryHandle, "GameUpdateAndRender");

		GameModule.bIsValid = (GameModule.GameGetSurfaceDesc && GameModule.GameInitialize && GameModule.GameUpdateAndRender);
	}

	if (!GameModule.bIsValid)
	{
		// TODO(Rafis): Assert failed to load game library
	}

	GameModule.BasePath = src;
	[GameModule.BasePath retain];

	GameModule.HotLoadPath = dst;
	[GameModule.HotLoadPath retain];

	NSDictionary* attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:GameModule.BasePath error:nil];
	GameModule.BaseLastWriteTime = attrs[NSFileModificationDate];
	[GameModule.BaseLastWriteTime retain];

	return GameModule;
}

auto GameModuleUnload(FGameModule& GameModule) -> void
{
	if (GameModule.LibraryHandle)
	{
		dlclose(GameModule.LibraryHandle);

		[[NSFileManager defaultManager] removeItemAtPath:GameModule.HotLoadPath error:nil];
	}

	[GameModule.BasePath release];
	[GameModule.HotLoadPath release];
	[GameModule.BaseLastWriteTime release];

	GameModule = {};
}

auto GameModuleChanged(FGameModule& GameModule) -> bool
{
	NSDictionary* attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:GameModule.BasePath error:nil];
	NSDate* mod = attrs[NSFileModificationDate];
	return (!GameModule.BaseLastWriteTime || ![mod isEqualToDate:GameModule.BaseLastWriteTime]);
}

@interface GameApplication : NSObject <NSApplicationDelegate, NSWindowDelegate, CAMetalDisplayLinkDelegate>

- (void)setBrightness:(id)sender;

- (void)setEDRBias:(id)sender;

@end

@interface GameWindow : NSWindow
@end

@implementation GameWindow
// Overriding this function allows to prevent clicking noise when using keyboard and esc key to go windowed
- (void)keyDown:(NSEvent*)event
{
}
@end

@implementation GameApplication
{
	GameWindow*							_window;
	NSView*								_view;
	CAMetalLayer*						_metalLayer;
	CAMetalDisplayLink*					_metalDisplayLink;

	id<MTLDevice>						_pDevice;
	id<MTLCommandQueue>					_pCommandQueue;
	id<MTLRenderPipelineState>			_pipelineState;
	id<MTLTexture>						_backBuffer;
	id<MTLSamplerState>					_backBufferSampler;
	MTLRenderPassDescriptor* 			_drawableRenderPass;

	CFTimeInterval						_lastTargetTimestamp;
	f32									_lastDeltaTime;
	u32									_targetFPS;

	GCController*						_nativeGamepad;
	GCMouse*							_mouse;
	GCKeyboard*							_keyboard;

	FOffscreenBuffer					_offscreenBuffer;
	FInputGamepad						_gamepad;
	FGameMemory							_gameMemory;
	FGameModule							_gameModule;
}

// NOTE: Engine and Game initialization procedure
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification
{
	_gameModule = GameModuleLoad();

	[self createWindow];
	[self createView];
	[self createGame];
	[self addInputObservers];
	[self createMetalResources];
	[self showWindow];
	[self evaluateCommandLine];
	[self updateMaxEDRValue];

	[NSNotificationCenter.defaultCenter addObserver:self 
										   selector:@selector(maxEDRValueDidChange:)
											   name:NSApplicationDidChangeScreenParametersNotification
											 object:nil];

	// Disable system cursor movement
	// CGAssociateMouseAndMouseCursorPosition(false);

	// // After initialization finished move app (and window) to a foreground.
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp activateIgnoringOtherApps:YES];

	// Creaet and Add display link to the run loop to have an entry point for the application
	[self createMetalDisplayLink];
	[_metalDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];

	_lastTargetTimestamp = CACurrentMediaTime();
}

// NOTE: Update and Render handling
- (void)metalDisplayLink:(CAMetalDisplayLink*)link needsUpdate:(CAMetalDisplayLinkUpdate*)update
{
	if (GameModuleChanged(_gameModule))
	{
		GameModuleUnload(_gameModule);
		_gameModule = GameModuleLoad();
	}

	[self pollInput];
	_gameModule.GameUpdateAndRender(_gameMemory, _lastDeltaTime, _offscreenBuffer, _gamepad);

	@autoreleasepool
	{
		id<MTLCommandBuffer> pCmd = [_pCommandQueue commandBuffer];

		// Fill texture
		{
			NSUInteger bytesPerRow = _offscreenBuffer.Width * _offscreenBuffer.BytesPerPixel;
			MTLRegion region = MTLRegionMake2D(0, 0, _offscreenBuffer.Width, _offscreenBuffer.Height);
			[_backBuffer replaceRegion:region mipmapLevel:0 withBytes:_offscreenBuffer.Backbuffer bytesPerRow:bytesPerRow];
		}
		// Present backbuffer render pass
		{
			_drawableRenderPass.colorAttachments[0].texture = update.drawable.texture;

			id<MTLRenderCommandEncoder> encoder = [pCmd renderCommandEncoderWithDescriptor:_drawableRenderPass];
			[encoder setRenderPipelineState:_pipelineState];
			[encoder setFragmentTexture:_backBuffer atIndex:0];
			[encoder setFragmentSamplerState:_backBufferSampler atIndex:0];
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
			[encoder endEncoding];

		}

		[pCmd presentDrawable:update.drawable];
		[pCmd commit];
	}

	CFTimeInterval currentTimestamp = CACurrentMediaTime();
	_lastDeltaTime = (f32)(currentTimestamp - _lastTargetTimestamp);
	_lastTargetTimestamp = currentTimestamp;
}

// NOTE: Raw input polling
- (void)pollInput
{
	if (_nativeGamepad)
	{
		GCExtendedGamepad* controllerInput = _nativeGamepad.extendedGamepad;
		if (controllerInput == nil)
		{
			return;
		}

		if (controllerInput)
		{
			[self macInputUpdateButtonState:_gamepad.GamepadA Input:controllerInput.buttonA];
			[self macInputUpdateButtonState:_gamepad.GamepadB Input:controllerInput.buttonB];
			[self macInputUpdateButtonState:_gamepad.GamepadX Input:controllerInput.buttonX];
			[self macInputUpdateButtonState:_gamepad.GamepadY Input:controllerInput.buttonY];
			[self macInputUpdateButtonState:_gamepad.GamepadDpadUp Input:controllerInput.buttonMenu];
			[self macInputUpdateButtonState:_gamepad.GamepadDpadDown Input:controllerInput.buttonOptions];
			[self macInputUpdateButtonState:_gamepad.GamepadDpadLeft Input:controllerInput.buttonMenu];
			[self macInputUpdateButtonState:_gamepad.GamepadDpadRight Input:controllerInput.buttonOptions];
			[self macInputUpdateButtonState:_gamepad.GamepadShoulderLeft Input:controllerInput.leftShoulder];
			[self macInputUpdateButtonState:_gamepad.GamepadShoulderRight Input:controllerInput.rightShoulder];
			[self macInputUpdateButtonState:_gamepad.GamepadThumbstickButtonLeft Input:controllerInput.leftThumbstickButton];
			[self macInputUpdateButtonState:_gamepad.GamepadThumbstickButtonRight Input:controllerInput.rightThumbstickButton];
			[self macInputUpdateButtonState:_gamepad.GamepadStart Input:controllerInput.buttonMenu];
			[self macInputUpdateButtonState:_gamepad.GamepadOption Input:controllerInput.buttonOptions];
			[self macInputUpdateButtonState:_gamepad.GamepadTriggerLeft Input:controllerInput.leftTrigger];
			[self macInputUpdateButtonState:_gamepad.GamepadTriggerRight Input:controllerInput.rightTrigger];
			[self macInputUpdateAxisState:_gamepad.GamepadDpadAxis Input:controllerInput.dpad];
			[self macInputUpdateAxisState:_gamepad.GamepadThumbstickLeft Input:controllerInput.leftThumbstick];
			[self macInputUpdateAxisState:_gamepad.GamepadThumbstickRight Input:controllerInput.rightThumbstick];
		}
	}

	if (_keyboard)
	{
		auto buttonW = [_keyboard.keyboardInput buttonForKeyCode:GCKeyCodeKeyW];
		auto buttonS = [_keyboard.keyboardInput buttonForKeyCode:GCKeyCodeKeyS];
		auto buttonA = [_keyboard.keyboardInput buttonForKeyCode:GCKeyCodeKeyA];
		auto buttonD = [_keyboard.keyboardInput buttonForKeyCode:GCKeyCodeKeyD];
		auto buttonR = [_keyboard.keyboardInput buttonForKeyCode:GCKeyCodeKeyR];
		[self macInputUpdateButtonState:_gamepad.GamepadDpadUp Input:buttonW];
		[self macInputUpdateButtonState:_gamepad.GamepadDpadDown Input:buttonS];
		[self macInputUpdateButtonState:_gamepad.GamepadDpadLeft Input:buttonA];
		[self macInputUpdateButtonState:_gamepad.GamepadDpadRight Input:buttonD];
		[self macInputUpdateButtonState:_gamepad.GamepadOption Input:buttonR];
	}

	if (_mouse)
	{
		
	}
}

- (void)createWindow
{
	auto Properties = _gameModule.GameGetSurfaceDesc();

	_targetFPS = Properties->TargetFPS;

	NSWindowStyleMask mask = NSWindowStyleMaskTitled;
	if (Properties->bClosable)
	{
		mask |= NSWindowStyleMaskClosable;
	}
	if (Properties->bResizable)
	{
		mask |= NSWindowStyleMaskResizable;
	}
	if (Properties->bMinimizable)
	{
		mask |= NSWindowStyleMaskMiniaturizable;
	}
	if (!Properties->bUseFrame)
	{
		mask |= NSWindowStyleMaskFullSizeContentView;
	}

	NSScreen* screen = [NSScreen mainScreen];
	NSRect contentRect = NSMakeRect(Properties->XPos, Properties->YPos, Properties->Width, Properties->Height);
	_window = [[GameWindow alloc] initWithContentRect:contentRect
											styleMask:mask
											  backing:NSBackingStoreBuffered
												defer:NO
											   screen:screen];

	// Center window to the middle of the screen if needed
	if (Properties->bCenterd)
	{
		[_window center];
	}
	else
	{
		NSPoint point = NSMakePoint(Properties->XPos, Properties->YPos);
		point  = [_window convertPointToScreen:point];
		[_window setFrameOrigin:point];
	}

	[_window setHasShadow:Properties->bHasShadow];
	[_window setTitlebarAppearsTransparent:!Properties->bUseFrame];

	_window.releasedWhenClosed = NO;
	_window.minSize = NSMakeSize(Properties->MinWidth, Properties->MinHeight);
	_window.maxSize = NSMakeSize(Properties->MaxWidth, Properties->MaxHeight);
	_window.delegate = self;

	[self updateWindowTitle:_window];
}

- (void)showWindow
{
	[_window setIsVisible:YES];
	[_window makeMainWindow];
	[_window makeKeyAndOrderFront:_window];
}

// Create the view and Metal layer backing it that renders the game
- (void)createView
{
	NSAssert(_window, @"You need to create the window before the view");
	
	_pDevice = MTLCreateSystemDefaultDevice();

	_metalLayer = [[CAMetalLayer alloc] init];
	_metalLayer.device = _pDevice;
	
	// Set layer size and make it opaque:
	_metalLayer.drawableSize = _window.contentLayoutRect.size;
	_metalLayer.opaque = YES;
	_metalLayer.framebufferOnly = YES;
	
	// Configure CoreAnimation letterboxing:
	_metalLayer.contentsGravity = kCAGravityResizeAspect;
	_metalLayer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
	
	// Prepare the layer for EDR rendering:
	_metalLayer.pixelFormat = MTLPixelFormatRGBA16Float;
	_metalLayer.wantsExtendedDynamicRangeContent = YES;
	_metalLayer.colorspace = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearDisplayP3);

	// Create a view and customize its layer to a MetalLayer where
	// the game can render Metal content:
	_view = [[NSView alloc] initWithFrame:_window.contentLayoutRect];
	_view.layer = _metalLayer;

	_window.contentView = _view;
}

// Initialize Memory for the Game and Game itself
- (void)createGame
{
	mach_vm_address_t addr = 0;
	int flags = VM_FLAGS_ANYWHERE;
	// During development it is usefull to always have game memory in the same address
#if defined(RE_INTERNAL)
	addr = 0x0000600000000000ULL;
	flags = VM_FLAGS_FIXED;
#endif

	_gameMemory.PermanentStorageSize = MiB(64);
	_gameMemory.TempStorageSize = GiB(2);
	kern_return_t kr = mach_vm_allocate(mach_task_self(), &addr, _gameMemory.PermanentStorageSize + _gameMemory.TempStorageSize, flags);
	if (kr != KERN_SUCCESS)
	{
		printf("FAILED TO ALLOCATE GAME MEMORY");
	}
	_gameMemory.PermanentStorage = (void*)addr;
	_gameMemory.TempStorage = (void*)(addr + _gameMemory.PermanentStorageSize);

	_gameModule.GameInitialize(_gameMemory);

	// NOTE: Before game starts calculate delta time for the very first frame,
	// it is not necessary but i did it.
	_lastDeltaTime = 1000.0f / _targetFPS;
}

- (void)createMetalResources
{
	_drawableRenderPass = [[MTLRenderPassDescriptor alloc] init];

	_pCommandQueue = [_pDevice newCommandQueue];

	// Shader to present texture to the full screen
	NSString* fsShaderSource = @"#include <metal_stdlib>\n"
								"using namespace metal;\n"
								"struct VSOut {\n"
								"	float4 position [[position]];\n"
								"	float2 uv;\n"
								"};\n"
								"vertex VSOut vertex_main(uint vid [[vertex_id]]) {\n"
								"	float2 pos;\n"
								"	if (vid == 0) pos = float2(-1.0, -1.0);\n"
								"	if (vid == 1) pos = float2( 3.0, -1.0);\n"
								"	if (vid == 2) pos = float2(-1.0,  3.0);\n"
								"	VSOut out;\n"
								"	out.position = float4(pos, 0.0, 1.0);\n"
								"	out.uv = pos* float2(0.5, -0.5) + 0.5; // flip vertical uv for the metal api\n"
								"	return out;\n"
								"}\n"
								"fragment float4 fragment_main(VSOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler samp [[sampler(0)]]) {\n"
								"	return tex.sample(samp, in.uv);\n"
								"}";

	NSError* error = nil;
	id<MTLLibrary> library = [_pDevice newLibraryWithSource:fsShaderSource
												options:nil
													error:&error];
	if (!library) {
		NSLog(@"Shader compile failed: %@", error);
	}
	id<MTLFunction> vertexFunc = [library newFunctionWithName:@"vertex_main"];
	id<MTLFunction> fragmentFunc = [library newFunctionWithName:@"fragment_main"];

	[self recreateBackbufferAndBackingStorage];
	
	// Create a simple sampler
	MTLSamplerDescriptor* sdesc = [MTLSamplerDescriptor new];
	sdesc.minFilter = MTLSamplerMinMagFilterNearest;
	sdesc.magFilter = MTLSamplerMinMagFilterNearest;
	sdesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
	sdesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
	_backBufferSampler = [_pDevice newSamplerStateWithDescriptor:sdesc];

	// Create pipeline
	MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	pipelineDesc.vertexFunction = vertexFunc;
	pipelineDesc.fragmentFunction = fragmentFunc;
	pipelineDesc.colorAttachments[0].pixelFormat = _metalLayer.pixelFormat;
	_pipelineState = [_pDevice newRenderPipelineStateWithDescriptor:pipelineDesc error:nil];
}

- (void)recreateBackbufferAndBackingStorage
{
	if (_backBuffer)
	{
		[_backBuffer release];
	}
	if (_offscreenBuffer.Backbuffer)
	{
		free(_offscreenBuffer.Backbuffer);
	}

	_offscreenBuffer.Width = _metalLayer.drawableSize.width;
	_offscreenBuffer.Height = _metalLayer.drawableSize.height;
	_offscreenBuffer.BytesPerPixel = 4;
	_offscreenBuffer.Backbuffer = (u8*)malloc(_offscreenBuffer.Width * _offscreenBuffer.Height * _offscreenBuffer.BytesPerPixel);

	MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
														width:_offscreenBuffer.Width
														height:_offscreenBuffer.Height
														mipmapped:NO];

	desc.storageMode = MTLStorageModeShared;   // CPU/GPU visible
	desc.usage = MTLTextureUsageShaderRead;
	_backBuffer = [_pDevice newTextureWithDescriptor:desc];
}

- (void)createMetalDisplayLink
{
	_metalDisplayLink = [[CAMetalDisplayLink alloc] initWithMetalLayer:_metalLayer];
	_metalDisplayLink.delegate = self;
	//                                                               Min,   Max,       Preffered
	_metalDisplayLink.preferredFrameRateRange = CAFrameRateRangeMake(60.0f, _targetFPS, _targetFPS);
}

- (void)addInputObservers
{
	NSNotificationCenter* defaultCenter = NSNotificationCenter.defaultCenter;
	[defaultCenter addObserver:self
					  selector:@selector(controllerDidConnect:)
						  name:GCControllerDidConnectNotification
						object:nil];
	[defaultCenter addObserver:self
					  selector:@selector(controllerDidDisconnect:)
						  name:GCControllerDidDisconnectNotification
						object:nil];
	[defaultCenter addObserver:self
					  selector:@selector(mouseDidConnect:)
						  name:GCMouseDidConnectNotification
						object:nil];
	[defaultCenter addObserver:self
					  selector:@selector(mouseDidDisconnect:)
						  name:GCMouseDidDisconnectNotification
						object:nil];
	[defaultCenter addObserver:self
					  selector:@selector(keyboardDidConnect:)
						  name:GCKeyboardDidConnectNotification
						object:nil];
	[defaultCenter addObserver:self
					  selector:@selector(keyboardDidDisconnect:)
						  name:GCKeyboardDidDisconnectNotification
						object:nil];
}

- (void)evaluateCommandLine
{
	NSArray<NSString*>* args = [[NSProcessInfo processInfo] arguments];
	BOOL exitAfterOneFrame = [args containsObject:@"--auto-close"];

	if (exitAfterOneFrame)
	{
		NSLog(@"Automatically terminating in 5 seconds...");
		[[NSApplication sharedApplication] performSelector:@selector(terminate:) withObject:self afterDelay:5];
	}
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
	return YES;
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
	GameModuleUnload(_gameModule);
}

- (void)applicationWillResignActive:(NSNotification*) notification
{
}

- (void)applicationWillBecomeActive:(NSNotification*) notification
{
}

- (void)updateMaxEDRValue
{
	float maxEDRValue = _window.screen.maximumExtendedDynamicRangeColorComponentValue;
	// [_gameCoordinator maxEDRValueDidChangeTo:maxEDRValue];
	[self updateWindowTitle:_window];
}

- (void)maxEDRValueDidChange:(NSNotification*)notification
{
	[self updateMaxEDRValue];
}

- (void)updateWindowTitle:(nonnull NSWindow*) window
{
	NSScreen* screen = window.screen;

	NSString* title = [NSString stringWithFormat:@"(%@ @ %ldHz, EDR max: %.2f)",
					   screen.localizedName,
					   (long)screen.maximumFramesPerSecond,
					   screen.maximumExtendedDynamicRangeColorComponentValue
	];

	window.title = title;
}

- (void)windowDidChangeScreen:(NSNotification*)notification
{
	[self updateMaxEDRValue];
}

- (void)setBrightness:(id)sender
{
	NSSlider* slider = (NSSlider*)sender;
	float brightness = slider.floatValue * 100;
}

- (void)setEDRBias:(id)sender
{
	NSSlider* slider = (NSSlider*)sender;
	float edrBias = slider.floatValue;
}

- (void)windowDidResize:(NSNotification*)notification
{
	// NOTE: This event fires every time while holding a left mouse button and resizing. On the contrary windowDidEndLiveResize fired once when mouse button is released.
}

- (void)windowDidEndLiveResize:(NSNotification*)notification
{
	@autoreleasepool
	{
		CGSize viewSize = _window.contentLayoutRect.size;
		CGSize newDrawableSize = [_view convertSizeToBacking:viewSize];
		if (newDrawableSize.width != _metalLayer.drawableSize.width || newDrawableSize.height != _metalLayer.drawableSize.height)
		{
			_metalLayer.drawableSize = newDrawableSize;
			_metalLayer.contentsScale = _window.backingScaleFactor;
		}

		// NOTE: Backbuffer texture is alwasy fixed size
		// [self recreateBackbufferAndBackingStorage];
	}
}

- (void)macInputUpdateButtonState:(FInputButtonState&)State Input:(GCControllerButtonInput*)Input
{
	State.bPressed = Input.pressed;
	State.Value = Input.value;
}

- (void)macInputUpdateAxisState:(FInputAxisState&)State Input:(GCControllerDirectionPad*)Input
{
	State.X = Input.xAxis.value;
	State.Y = Input.yAxis.value;
}

/// Tells the game input class when a keyboard connects.
- (void)keyboardDidConnect:(nullable NSNotification*)notification
{
	GCKeyboard* keyboard = (GCKeyboard*)notification.object;
	if (!keyboard)
	{
		return;
	}

	if (_keyboard != nil && _keyboard != keyboard)
	{
		return;
	}

	_keyboard = keyboard;
}

/// Tells the game input class when a keyboard disconnects.
- (void)keyboardDidDisconnect:(nullable NSNotification*)notification
{
	GCKeyboard* keyboard = (GCKeyboard*)notification.object;

	if (_keyboard != nil && _keyboard != keyboard)
	{
		return;
	}

	_keyboard = nil;
}

/// Tells the game input class when a mouse connects.
- (void)mouseDidConnect:(nullable NSNotification*)notification
{
	GCMouse* mouse = (GCMouse*)notification.object;
	if (mouse == nil)
	{
		return;
	}

	GCMouseInput* mouseInput = mouse.mouseInput;
	if (mouseInput == nil)
	{
		return;
	}

	// __block GameInput* weakSelf = self;
	// mouseInput.mouseMovedHandler = ^(GCMouseInput* mouse, float deltaX, float deltaY) {
	// 	GameInput* strongSelf = weakSelf;
	// 	if (strongSelf == nil)
	// 	{
	// 		return;
	// 	}

	// 	strongSelf.mouseDelta = simd_make_float2(deltaX, deltaY);
	// };

	if (_mouse != nil && _mouse != mouse)
	{
		return;
	}

	_mouse = mouse;
}

/// Tells the game input class when a mouse disconnects.
- (void)mouseDidDisconnect:(nullable NSNotification*)notification
{
	GCMouse* mouse = (GCMouse*)notification.object;

	if (_mouse != nil && _mouse != mouse)
	{
		return;
	}

	_mouse = nil;
}

/// Tells the game input class when a game controller connects.
- (void)controllerDidConnect:(nullable NSNotification*)notification
{
	GCController* controller = (GCController*)notification.object;
	if (!controller)
	{
		return;
	}

	GCExtendedGamepad* controllerInput = controller.extendedGamepad;
	if (controllerInput == nil)
	{
		// TODO(Rafis): Notify that game only support regular gamepads (Xbox/DualShock and similar)
		return;
	}

	if (_nativeGamepad != nil && _nativeGamepad != controller)
	{
		// TODO(Rafis): Add support for multiple controller
		return;
	}

	_nativeGamepad = controller;
}

/// Tells the game input class when a game controller disconnects.
- (void)controllerDidDisconnect:(nullable NSNotification*)notification
{
	GCController* controller = (GCController*)notification.object;
	if (!controller)
	{
		return;
	}

	if (_nativeGamepad != nil && _nativeGamepad != controller)
	{
		// TODO(Rafis): Add support for multiple controller
		return;
	}

	_nativeGamepad = nil;
}

#pragma region Input Observers

@end

int main(int argc, const char* argv[])
{
	NSApplication* application = [NSApplication sharedApplication];
	// createApplicationMenu(application);

	// Set up the application and window delegate:
	GameApplication* gameApplication = [[GameApplication alloc] init];
	application.delegate             = gameApplication;

	// Yield the game loop to the `NSApplication`, which calls
	// application:didFinishLaunchingWithOptions: from the application delegate
	// (an instance of `macOS/GameApplication`), where this sample then sets
	// up view, and game.
	return NSApplicationMain(argc, argv);
}
