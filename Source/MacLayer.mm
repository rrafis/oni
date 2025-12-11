#include "MacLayer.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>
#import <GameController/GameController.h>

#import <simd/simd.h>

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

	u8*									_pixels;

	CFTimeInterval						_lastTargetTimestamp;
	CFTimeInterval						_lastDeltaTime;

	FMacInternalState					_internalState;

	GCController*						_gamepad;
	GCMouse*							_mouse;
	GCKeyboard*							_keyboard;
}

- (void)createWindow
{
	auto Properties = MacGetSurfaceDesc();

	NSWindowStyleMask mask = NSWindowStyleMaskTitled;
	if (Properties.bClosable)
	{
		mask |= NSWindowStyleMaskClosable;
	}
	if (Properties.bResizable)
	{
		mask |= NSWindowStyleMaskResizable;
	}
	if (Properties.bMinimizable)
	{
		mask |= NSWindowStyleMaskMiniaturizable;
	}
	if (!Properties.bUseFrame)
	{
		mask |= NSWindowStyleMaskFullSizeContentView;
	}

	NSScreen* screen = [NSScreen mainScreen];
	NSRect contentRect = NSMakeRect(Properties.XPos, Properties.YPos, Properties.Width, Properties.Height);
	// NSRect contentRect = NSMakeRect(Properties.XPos, Properties.YPos, _width, _height);
	_window = [[GameWindow alloc] initWithContentRect:contentRect
											styleMask:mask
											  backing:NSBackingStoreBuffered
												defer:NO
											   screen:screen];

	// Center window to the middle of the screen if needed
	if (Properties.bCenterd)
	{
		[_window center];
	}
	else
	{
		NSPoint point = NSMakePoint(Properties.XPos, Properties.YPos);
		point  = [_window convertPointToScreen:point];
		[_window setFrameOrigin:point];
	}

	[_window setHasShadow:Properties.bHasShadow];
	[_window setTitlebarAppearsTransparent:!Properties.bUseFrame];

	_window.releasedWhenClosed = NO;
	_window.minSize = NSMakeSize(Properties.MinWidth, Properties.MinHeight);
	_window.maxSize = NSMakeSize(Properties.MaxWidth, Properties.MaxHeight);
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
	if (_pixels)
	{
		free(_pixels);
	}

	u32 width = _metalLayer.drawableSize.width;
	u32 height = _metalLayer.drawableSize.height;

	MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
														width:width
														height:height
														mipmapped:NO];

	desc.storageMode = MTLStorageModeShared;   // CPU/GPU visible
	desc.usage = MTLTextureUsageShaderRead;
	_backBuffer = [_pDevice newTextureWithDescriptor:desc];

	NSUInteger bytesPerPixel = 4; // BGRA8 = 4 bytes per pixel
	_pixels = (u8*)malloc(width * height * bytesPerPixel * 5);
}

- (void)createMetalDisplayLink
{
	_metalDisplayLink = [[CAMetalDisplayLink alloc] initWithMetalLayer:_metalLayer];
	_metalDisplayLink.delegate = self;
	_metalDisplayLink.preferredFrameRateRange = CAFrameRateRangeMake(30, 120, 120);
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

#pragma region CAMetalDisplayLinkDelegate
- (void)metalDisplayLink:(CAMetalDisplayLink*)link needsUpdate:(CAMetalDisplayLinkUpdate*)update
{
	[self pollInput];

	_internalState.Backbuffer = _pixels;
	_internalState.Width = _backBuffer.width;
	_internalState.Height = _backBuffer.height;
	_internalState.BytesPerPixel = 4;
	_internalState.DeltaTime = (f32)_lastDeltaTime;
	MacRunLoop(_internalState);

	@autoreleasepool
	{
		id<MTLCommandBuffer> pCmd = [_pCommandQueue commandBuffer];

		// Fill texture
		{
			NSUInteger bytesPerRow = _internalState.Width * _internalState.BytesPerPixel;
			MTLRegion region = MTLRegionMake2D(0, 0, _internalState.Width, _internalState.Height);
			[_backBuffer replaceRegion:region mipmapLevel:0 withBytes:_pixels bytesPerRow:bytesPerRow];
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
	_lastDeltaTime = currentTimestamp - _lastTargetTimestamp;
	_lastTargetTimestamp = currentTimestamp;
}
#pragma endregion CAMetalDisplayLinkDelegate

#pragma region NSApplicationDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
	return YES;
}

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification
{
	[self createWindow];
	[self createView];
	// [self createGame];
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

	[self createMetalDisplayLink];

	// Add display link to the run loop to have an entry point for the application
	[_metalDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];

	// NOTE: Before application starts calculate delta time for the very first frame,
	// it is not necessary but i did it.
	_lastDeltaTime = 1000.0 / 120.0;

	[self addInputObservers];
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
}

- (void)applicationWillResignActive:(NSNotification*) notification
{
}

- (void)applicationWillBecomeActive:(NSNotification*) notification
{
}
#pragma endregion NSApplicationDelegate

#pragma region NSWindowDelegate
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
	}
}
#pragma endregion NSWindowDelegate

#pragma region Input Observers

- (void)pollInput
{
	if (_gamepad)
	{
		GCExtendedGamepad* controllerInput = _gamepad.extendedGamepad;
		if (controllerInput == nil)
		{
			return;
		}

		if (controllerInput)
		{
			[self macInputUpdateButtonState:_internalState.GamepadA Input:controllerInput.buttonA];
			[self macInputUpdateButtonState:_internalState.GamepadB Input:controllerInput.buttonB];
			[self macInputUpdateButtonState:_internalState.GamepadX Input:controllerInput.buttonX];
			[self macInputUpdateButtonState:_internalState.GamepadY Input:controllerInput.buttonY];
			[self macInputUpdateButtonState:_internalState.GamepadDpadUp Input:controllerInput.buttonMenu];
			[self macInputUpdateButtonState:_internalState.GamepadDpadDown Input:controllerInput.buttonOptions];
			[self macInputUpdateButtonState:_internalState.GamepadDpadLeft Input:controllerInput.buttonMenu];
			[self macInputUpdateButtonState:_internalState.GamepadDpadRight Input:controllerInput.buttonOptions];
			[self macInputUpdateButtonState:_internalState.GamepadShoulderLeft Input:controllerInput.leftShoulder];
			[self macInputUpdateButtonState:_internalState.GamepadShoulderRight Input:controllerInput.rightShoulder];
			[self macInputUpdateButtonState:_internalState.GamepadThumbstickButtonLeft Input:controllerInput.leftThumbstickButton];
			[self macInputUpdateButtonState:_internalState.GamepadThumbstickButtonRight Input:controllerInput.rightThumbstickButton];
			[self macInputUpdateButtonState:_internalState.GamepadStart Input:controllerInput.buttonMenu];
			[self macInputUpdateButtonState:_internalState.GamepadOption Input:controllerInput.buttonOptions];
			[self macInputUpdateButtonState:_internalState.GamepadTriggerLeft Input:controllerInput.leftTrigger];
			[self macInputUpdateButtonState:_internalState.GamepadTriggerRight Input:controllerInput.rightTrigger];
			[self macInputUpdateAxisState:_internalState.GamepadDpadAxis Input:controllerInput.dpad];
			[self macInputUpdateAxisState:_internalState.GamepadThumbstickLeft Input:controllerInput.leftThumbstick];
			[self macInputUpdateAxisState:_internalState.GamepadThumbstickRight Input:controllerInput.rightThumbstick];
		}
	}

	if (_keyboard)
	{

	}

	if (_mouse)
	{
		
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
	State.Y = -1.0f * Input.yAxis.value;
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

	_keyboard = nil;
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

	if (_gamepad != nil && _gamepad != controller)
	{
		// TODO(Rafis): Add support for multiple controller
		return;
	}

	_gamepad = controller;
}

/// Tells the game input class when a game controller disconnects.
- (void)controllerDidDisconnect:(nullable NSNotification*)notification
{
	GCController* controller = (GCController*)notification.object;
	if (!controller)
	{
		return;
	}

	if (_gamepad != nil && _gamepad != controller)
	{
		// TODO(Rafis): Add support for multiple controller
		return;
	}

	_gamepad = nil;
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
