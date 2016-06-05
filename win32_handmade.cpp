#include <math.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#include "handmade.h"
#include "handmade.cpp"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>

struct win32_offscreen_buffer
{
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
global_variable bool32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) \
  HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

void *PlatformLoadFile(char *FileName) { return 0; }
internal void Win32LoadXInput()
{
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

  if (!XInputLibrary)
  {
    // TODO Logging
    XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
  }

  if (!XInputLibrary)
  {
    // TODO Logging
    XInputLibrary = LoadLibraryA("xinput1_3.dll");
  }

  if (XInputLibrary)
  {
    XInputGetState = (x_input_get_state *) GetProcAddress(XInputLibrary, "XInputGetState");
    if (!XInputGetState)
    {
      XInputGetState = XInputGetStateStub;
    }

    XInputSetState = (x_input_set_state *) GetProcAddress(XInputLibrary, "XInputSetState");
    if (!XInputSetState)
    {
      XInputSetState = XInputSetStateStub;
    }
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
    direct_sound_create *DirectSoundCreate =
      (direct_sound_create *) GetProcAddress(DSoundLibrary, "DirectSoundCreate");
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
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Width * BytesPerPixel;

  // TODO Probably want to clear this to black.
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext,
    int WindowWidth, int WindowHeight)
{
  // TODO Aspect ratio correction.
  StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0, Buffer->Width, Buffer->Height,
      Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
  LRESULT Result = 0;

  switch (Message)
  {
    case WM_CLOSE:
      {
        // TODO Handle this with a message to the user?
        GlobalRunning = false;
      }
      break;

    case WM_ACTIVATEAPP:
      {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
      }
      break;

    case WM_DESTROY:
      {
        // TODO Handle this as an error - recreate window?
        GlobalRunning = false;
      }
      break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
      {
        uint32 VKCode = WParam;
        bool32 WasDown = (LParam & (1 << 30)) != 0;
        bool32 IsDown = (LParam & (1 << 31)) == 0;

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
        bool32 AltKeyWasDown = (LParam & (1 << 29)) != 0;
        if (VKCode == VK_F4 && AltKeyWasDown) GlobalRunning = false;
      }
      break;

    case WM_PAINT:
      {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width,
            Dimension.Height);
        EndPaint(Window, &Paint);
      }
      break;

    default:
      {
        Result = DefWindowProcA(Window, Message, WParam, LParam);
      }
      break;
  }

  return (Result);
}

struct win32_sound_output
{
  int SamplesPerSecond;
  int ToneHz;
  int16 ToneVolume;
  uint32 RunningSampleIndex;
  int WavePeriod;
  int BytesPerSample;
  int SecondaryBufferSize;
  real32 tSine;
  int LatencySampleCount;
};

internal void Win32ClearBuffer(win32_sound_output* SoundOutput)
{
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size,
          &Region2, &Region2Size, 0)))
  {
    uint8 *DestSample = (uint8 *) Region1;

    for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
    {
      *DestSample++ = 0;
    }

    DestSample = (uint8 *) Region2;

    for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
    {
      *DestSample++ = 0;
    }

    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock,
    DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  // TODO More strenuous test!
  if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size,
          &Region2, &Region2Size, 0)))
  {
    // TODO Assert that Region1Size/Region2Size is valid.
    DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    int16 *DestSample = (int16 *) Region1;
    int16 *SourceSample = SourceBuffer->Samples;

    // TODO Collapse these two loops.
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
    {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }

    DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    DestSample = (int16 *) Region2;

    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
    {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }

    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

  Win32LoadXInput();
  WNDCLASSA WindowClass = {};
  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  if (RegisterClass(&WindowClass))
  {
    HWND Window = CreateWindowExA(0, WindowClass.lpszClassName, "Handmade Hero",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);

    if (Window)
    {
      // Since we specified CS_OWNDC, we can just get one device context
      // and use it forever because we are not sharing it with anyone.
      HDC DeviceContext = GetDC(Window);

      int XOffset = 0;
      int YOffset = 0;

      win32_sound_output SoundOutput = {};

      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.ToneHz = 256;
      SoundOutput.ToneVolume = 3000;
      SoundOutput.RunningSampleIndex = 0;
      SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
      SoundOutput.BytesPerSample = sizeof(int16) * 2;
      SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
      SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
      Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
      Win32ClearBuffer(&SoundOutput);
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      GlobalRunning = true;

      int16 *Samples = (int16 *) VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

      LARGE_INTEGER LastCounter;
      QueryPerformanceCounter(&LastCounter);
      uint64 LastCycleCount = __rdtsc();

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

            bool32 Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
            bool32 Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
            bool32 Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            bool32 Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
            bool32 Start = Pad->wButtons & XINPUT_GAMEPAD_START;
            bool32 Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
            bool32 LeftShoulder = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
            bool32 RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
            bool32 AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
            bool32 BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
            bool32 XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
            bool32 YButton = Pad->wButtons & XINPUT_GAMEPAD_Y;

            int16 StickX = Pad->sThumbLX;
            int16 StickY = Pad->sThumbLY;

            // TODO Handle deadzone properly with
            // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
            // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE
            XOffset += StickX / 4096;
            YOffset += StickY / 4096;

            SoundOutput.ToneHz = 512 + (int) (256.0f * ((real32) StickY) / 30000.0f);
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
          }
          else
          {
            // The controller is not available.
          }
        }

        DWORD ByteToLock = 0;
        DWORD TargetCursor = 0;
        DWORD BytesToWrite = 0;
        DWORD PlayCursor = 0;
        DWORD WriteCursor = 0;
        bool32 SoundIsValid = false;

        if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
        {
          ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
            SoundOutput.SecondaryBufferSize;
          TargetCursor =
            (PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
            SoundOutput.SecondaryBufferSize;

          if (ByteToLock > TargetCursor)
          {
            BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
            BytesToWrite += TargetCursor;
          }
          else
          {
            BytesToWrite = TargetCursor - ByteToLock;
          }

          SoundIsValid = true;
        }

        game_sound_output_buffer SoundBuffer = {};
        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
        SoundBuffer.Samples = Samples;

        game_offscreen_buffer Buffer = {};
        Buffer.Memory = GlobalBackbuffer.Memory;
        Buffer.Width = GlobalBackbuffer.Width;
        Buffer.Height = GlobalBackbuffer.Height;
        Buffer.Pitch = GlobalBackbuffer.Pitch;
        GameUpdateAndRender(&Buffer, XOffset, YOffset, &SoundBuffer, SoundOutput.ToneHz);

        if (SoundIsValid)
        {
          Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
        }

        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width,
            Dimension.Height);

        uint64 EndCycleCount = __rdtsc();

        LARGE_INTEGER EndCounter;
        QueryPerformanceCounter(&EndCounter);

        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
        int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
        int32 MSPerFrame = (int32)((1000 * CounterElapsed) / PerfCountFrequency);
        int32 FPS = PerfCountFrequency / CounterElapsed;
        int32 MCPF = (int32)(CyclesElapsed / (1000 * 1000));

        // char Buffer[256];
        // wsprintf(Buffer, "%dms/f,  %df/s,  %dmc/f\n", MSPerFrame, FPS, MCPF);
        // OutputDebugStringA(Buffer);

        LastCounter = EndCounter;
        LastCycleCount = EndCycleCount;
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

  return 0;
}

