#include<windows.h>
#include<stdint.h>
#include<xinput.h>
#include<dsound.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer {
  // Pixels are always 32 bits wide. Memory order: 0x xx RR GG BB
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};

struct win32_window_dimension
{
  int Width;
  int Height;
};

// TODO This is a global for now.
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
  return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
  return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32LoadXInput(void)
{
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

  if (!XInputLibrary)
  {
    // TODO Logging
    XInputLibrary = LoadLibraryA("xinput1_3.dll");
  }

  if (XInputLibrary)
  {
    XInputGetState = (x_input_get_state *) GetProcAddress(XInputLibrary, "XInputGetState");
    if (!XInputGetState) { XInputGetState = XInputGetStateStub; }

    XInputSetState = (x_input_set_state *) GetProcAddress(XInputLibrary, "XInputSetState");
    if (!XInputSetState) { XInputSetState = XInputSetStateStub; }
  }
  else
  {
    // TODO Logging
  }
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
  // Load the library.
  HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

  if (DSoundLibrary)
  {
    // Get a DirectSound object.
    direct_sound_create *DirectSoundCreate = (direct_sound_create *) GetProcAddress(DSoundLibrary, "DirectSoundCreate");
    LPDIRECTSOUND DirectSound;

    if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
    {
      WAVEFORMATEX WaveFormat = {};
      WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
      WaveFormat.nChannels = 2;
      WaveFormat.wBitsPerSample = 16;
      WaveFormat.nSamplesPerSec = SamplesPerSecond;
      WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
      WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
      WaveFormat.cbSize = 0;

      if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
      {
        DSBUFFERDESC BufferDescription = {};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

        // Create a primary buffer.
        LPDIRECTSOUNDBUFFER PrimaryBuffer;

        if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
        {
          if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
          {
            // We have finally set the format!
            OutputDebugStringA("Primary buffer was set.\n");
          }
          else
          {
            // TODO Logging
          }
        }
        else
        {
          // TODO Logging
        }
      }
      else
      {
        // TODO Logging
      }

      // TODO DSBCAPS_GETCURRENTPOSITION2
      // Create a secondary buffer.
      DSBUFFERDESC BufferDescription = {};
      BufferDescription.dwSize = sizeof(BufferDescription);
      BufferDescription.dwFlags = 0;
      BufferDescription.dwBufferBytes = BufferSize;
      BufferDescription.lpwfxFormat = &WaveFormat;

      if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
      {
        OutputDebugStringA("Secondary buffer was created successfully.\n");
      }
    }
    else
    {
      // TODO Logging
    }
  }
  else
  {
    // TODO Logging
  }
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
  win32_window_dimension Result;

  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;

  return Result;
}

internal void RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
  // TODO Let's see what the optimiser does.
  uint8 *Row = (uint8 *) Buffer->Memory;

  for (int Y = 0; Y < Buffer->Height; ++Y)
  {
    uint32 *Pixel = (uint32 *) Row;

    for (int X = 0; X < Buffer->Width; ++X)
    {
      uint8 Blue = X + BlueOffset;
      uint8 Green = Y + GreenOffset;

      *Pixel++ = Green << 8 | Blue;
    }

    Row += Buffer->Pitch;
  }
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
  // TODO Bulletproof this.
  // Maybe don't free first, free after, then free first if that fails.

  if (Buffer->Memory)
  {
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  Buffer->Width = Width;
  Buffer->Height = Height;

  int BytesPerPixel = 4;

  // When the biHeight field is negative, this is the clue to Windows
  // to treat this bitmap as top-down, not bottom-up. This means that
  // the first three bytes of the image are the colour for the top left
  // pixel in the bitmap, not the bottom left!
  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Width;
  Buffer->Info.bmiHeader.biHeight = -Height;
  Buffer->Info.bmiHeader.biPlanes = 1;
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;

  int BitmapMemorySize = (Width * Height) * BytesPerPixel;
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Width * BytesPerPixel;

  // TODO Probably want to clear this to black.
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
    HDC DeviceContext, int WindowWidth, int WindowHeight)
{
  // TODO Aspect ratio correction.
  StretchDIBits(DeviceContext,
      0, 0, WindowWidth, WindowHeight,
      0, 0, Buffer->Width, Buffer->Height,
      Buffer->Memory,
      &Buffer->Info,
      DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(
    HWND Window,
    UINT Message,
    WPARAM WParam,
    LPARAM LParam)
{
  LRESULT Result = 0;

  switch (Message)
  {
    case WM_CLOSE:
    {
      // TODO Handle this with a message to the user?
      GlobalRunning = false;
    } break;

    case WM_ACTIVATEAPP:
    {
      OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_DESTROY:
    {
      // TODO Handle this as an error - recreate window?
      GlobalRunning = false;
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
      uint32 VKCode = WParam;
      bool WasDown = (LParam & (1 << 30)) != 0;
      bool IsDown = (LParam & (1 << 31)) == 0;

      if (WasDown != IsDown)
      {
        if (VKCode == 'W')
        {
        }
        else if (VKCode == 'A')
        {
        }
        else if (VKCode == 'S')
        {
        }
        else if (VKCode == 'D')
        {
        }
        else if (VKCode == 'Q')
        {
        }
        else if (VKCode == 'E')
        {
        }
        else if (VKCode == VK_UP)
        {
        }
        else if (VKCode == VK_LEFT)
        {
        }
        else if (VKCode == VK_DOWN)
        {
        }
        else if (VKCode == VK_RIGHT)
        {
        }
        else if (VKCode == VK_ESCAPE)
        {
        }
        else if (VKCode == VK_SPACE)
        {
        }
      }

      // Handle ALT-F4
      bool AltKeyWasDown = (LParam & (1 << 29)) != 0;
      if (VKCode == VK_F4 && AltKeyWasDown) { GlobalRunning = false; }
    } break;

    case WM_PAINT:
    {
      PAINTSTRUCT Paint;
      HDC DeviceContext = BeginPaint(Window, &Paint);
      win32_window_dimension Dimension = Win32GetWindowDimension(Window);
      Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
      EndPaint(Window, &Paint);
    } break;

    default:
    {
      Result = DefWindowProc(Window, Message, WParam, LParam);
    } break;
  }

  return(Result);
}

int CALLBACK WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CommandLine,
    int ShowCode)
{
  Win32LoadXInput();
  WNDCLASSA WindowClass = {};
  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  if (RegisterClass(&WindowClass))
  {
    HWND Window = CreateWindowExA(
          0,
          WindowClass.lpszClassName,
          "Handmade Hero",
          WS_OVERLAPPEDWINDOW|WS_VISIBLE,
          CW_USEDEFAULT,
          CW_USEDEFAULT,
          CW_USEDEFAULT,
          CW_USEDEFAULT,
          0,
          0,
          Instance,
          0);

    if (Window)
    {
      // Since we specified CS_OWNDC, we can just get one device context
      // and use it forever because we are not sharing it with anyone.
      HDC DeviceContext = GetDC(Window);

      int XOffset = 0;
      int YOffset = 0;

      int SamplesPerSecond = 48000;
      int ToneHz = 256;
      int16 ToneVolume = 1000;
      uint32 RunningSampleIndex = 0;
      int SquareWavePeriod = SamplesPerSecond / ToneHz;
      int HalfSquareWavePeriod = SquareWavePeriod / 2;
      int BytesPerSample = sizeof(int16) * 2;
      int SecondaryBufferSize = SamplesPerSecond * BytesPerSample;

      Win32InitDSound(Window, SamplesPerSecond, SecondaryBufferSize);
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      GlobalRunning = true;

      while (GlobalRunning)
      {
        MSG Message;

        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
          if (Message.message == WM_QUIT)
          {
            GlobalRunning = false;
          }

          TranslateMessage(&Message);
          DispatchMessageA(&Message);
        }

        // TODO Should we poll this more frequently?
        for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
        {
          XINPUT_STATE ControllerState;

          if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
          {
            // This controller is plugged in.
            // TODO See if ControllerState.dwPacketNumber increments too rapidly.
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

            bool Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
            bool Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
            bool Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            bool Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
            bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;
            bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
            bool LeftShoulder = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
            bool RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
            bool AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
            bool BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
            bool XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
            bool YButton = Pad->wButtons & XINPUT_GAMEPAD_Y;

            int16 StickX = Pad->sThumbLX;
            int16 StickY = Pad->sThumbLY;

            XOffset += StickX >> 12;
            YOffset += StickY >> 12;
          }
          else
          {
            // The controller is not available.
          }
        }

        RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);

        // DirectSound output test
        DWORD PlayCursor;
        DWORD WriteCursor;

        if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
        {
          DWORD ByteToLock = RunningSampleIndex * BytesPerSample % SecondaryBufferSize;
          DWORD BytesToWrite;

          if (ByteToLock == PlayCursor)
          {
            BytesToWrite = SecondaryBufferSize;
          }
          if (ByteToLock > PlayCursor)
          {
            BytesToWrite = SecondaryBufferSize - ByteToLock;
            BytesToWrite += PlayCursor;
          }
          else
          {
            BytesToWrite = PlayCursor - ByteToLock;
          }

          VOID *Region1;
          DWORD Region1Size;
          VOID *Region2;
          DWORD Region2Size;

          // TODO More strenuous test!
          if (SUCCEEDED(GlobalSecondaryBuffer->Lock(
              ByteToLock, BytesToWrite,
              &Region1, &Region1Size,
              &Region2, &Region2Size,
              0)))
          {
            // TODO Assert that Region1Size/Region2Size is valid.
            DWORD Region1SampleCount = Region1Size / BytesPerSample;
            int16 *SampleOut = (int16 *) Region1;

            //TODO Collapse these two loops.
            for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
            {
              int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
              *SampleOut++ = SampleValue;
              *SampleOut++ = SampleValue;
            }

            DWORD Region2SampleCount = Region2Size / BytesPerSample;
            SampleOut = (int16 *) Region1;

            for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
            {
              int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
              *SampleOut++ = SampleValue;
              *SampleOut++ = SampleValue;
            }

            GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
          }
        }

        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
      }
    }
    else
    {
      // TODO Logging
    }
  }
  else
  {
    // TODO Logging
  }

  return(0);
}

