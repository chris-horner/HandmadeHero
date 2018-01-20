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
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

// TODO This is a global for now.
global_variable bool32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

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

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename)
{
  debug_read_file_result Result = {};
  HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  if (FileHandle != INVALID_HANDLE_VALUE)
  {
    LARGE_INTEGER FileSize;

    if (GetFileSizeEx(FileHandle, &FileSize))
    {
      uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
      Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

      if (Result.Contents)
      {
        DWORD BytesRead;

        if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
            FileSize32 == BytesRead)
        {
          // File read successfully.
          Result.ContentsSize = FileSize32;
        }
        else
        {
          DEBUGPlatformFreeFileMemory(Result.Contents);
          Result.Contents = 0;
        }
      }
    }

    CloseHandle(FileHandle);
  }

  return Result;
}

internal void DEBUGPlatformFreeFileMemory(void *Memory)
{
  if (Memory)
  {
    VirtualFree(Memory, 0, MEM_RELEASE);
  }
}

internal bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory)
{
  bool32 Result = false;
  HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

  if (FileHandle != INVALID_HANDLE_VALUE)
  {
    DWORD BytesWritten;

    if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
    {
      Result = BytesWritten == MemorySize;
    }

    CloseHandle(FileHandle);
  }

  return Result;
}

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
      BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
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
  Buffer->BytesPerPixel = BytesPerPixel;

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
      Assert(!"Keyboard input came in through a non-dispatch message!");
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

  return Result;
}

internal void Win32ClearBuffer(win32_sound_output *SoundOutput)
{
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1,
                                            &Region1Size, &Region2, &Region2Size, 0)))
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

internal void Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
  Assert(NewState->EndedDown != IsDown);
  NewState->EndedDown = IsDown;
  ++NewState->HalfTransitionCount;
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState,
                                              DWORD ButtonBit, game_button_state *NewState)
{
  NewState->EndedDown = (XInputButtonState & ButtonBit) == ButtonBit;
  NewState->HalfTransitionCount = OldState->EndedDown != NewState->EndedDown ? 1 : 0;
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
  real32 Result = 0;

  if (Value < -DeadZoneThreshold)
  {
    Result = (real32)(Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);
  }
  else if (Value > DeadZoneThreshold)
  {
    Result = (real32)(Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold);
  }

  return Result;
}

internal void Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
  MSG Message;

  while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
  {
    switch (Message.message)
    {
      case WM_QUIT:
      {
        GlobalRunning = false;
      }
      break;

      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_KEYUP:
      {
        uint32 VKCode = (uint32) Message.wParam;
        bool32 WasDown = (Message.lParam & (1 << 30)) != 0;
        bool32 IsDown = (Message.lParam & (1 << 31)) == 0;

        if (WasDown != IsDown)
        {
          if (VKCode == 'W')
          {
            Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
          }
          else if (VKCode == 'A')
          {
            Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
          }
          else if (VKCode == 'S')
          {
            Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
          }
          else if (VKCode == 'D')
          {
            Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
          }
          else if (VKCode == 'Q')
          {
            Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
          }
          else if (VKCode == 'E')
          {
            Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
          }
          else if (VKCode == VK_UP)
          {
            Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
          }
          else if (VKCode == VK_LEFT)
          {
            Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
          }
          else if (VKCode == VK_DOWN)
          {
            Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
          }
          else if (VKCode == VK_RIGHT)
          {
            Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
          }
          else if (VKCode == VK_ESCAPE)
          {
            Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
          }
          else if (VKCode == VK_SPACE)
          {
            Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
          }
        }

        // Handle ALT-F4
        bool32 AltKeyWasDown = (Message.lParam & (1 << 29)) != 0;
        if (VKCode == VK_F4 && AltKeyWasDown) GlobalRunning = false;
      }
      break;

      default:
      {
        TranslateMessage(&Message);
        DispatchMessageA(&Message);
      }
      break;
    }
  }
}

inline LARGE_INTEGER Win32GetWallClock()
{
  LARGE_INTEGER Result;
  QueryPerformanceCounter(&Result);
  return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
  return (real32) ((real32) (End.QuadPart - Start.QuadPart)) / (real32) GlobalPerfCountFrequency;
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *Backbuffer, int X, int Top, int Bottom, uint32 Color)
{
  uint8 *Pixel = (uint8 *) Backbuffer->Memory +
                           X * Backbuffer->BytesPerPixel +
                           Top * Backbuffer->Pitch;

  for (int Y = Top; Y < Bottom; ++Y)
  {
    *(uint32 *) Pixel = Color;
    Pixel += Backbuffer->Pitch;
  }
}

inline void Win32DrawSoundBufferMarker(win32_offscreen_buffer *Backbuffer,
                                       win32_sound_output *SoundOutput,
                                       real32 C, int PadX, int Top, int Bottom,
                                       DWORD Value, uint32 Color)
{
    Assert(Value < SoundOutput->SecondaryBufferSize);
    real32 XReal32 = C * (real32) Value;
    int X = PadX + (int) XReal32;
    Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer *Backbuffer, int MarkerCount,
                                    win32_debug_time_marker *Markers, win32_sound_output *SoundOutput,
                                    real32 TargetSecondsPerFrame)
{
  // TODO Draw where we're writing out sound.

  int PadX = 16;
  int PadY = 16;
  int Top = PadY;
  int Bottom = Backbuffer->Height - PadY;
  real32 C = (real32) (Backbuffer->Width - 2 * PadX) / (real32) SoundOutput->SecondaryBufferSize;

  for(int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
  {
    win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->PlayCursor, 0xFFFFFFFF);
    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->WriteCursor, 0xFFFF0000);
  }
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

  // Set the Windows scheduler granularity to 1ms so that our Sleep() can be more granular.
  UINT DesiredSchedulerMs = 1;
  bool32 SleepIsGranular = timeBeginPeriod(DesiredSchedulerMs) == TIMERR_NOERROR;

  Win32LoadXInput();
  WNDCLASSA WindowClass = {};
  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  // TODO How do we reliably query this on Windows?
#define FramesOfAudioLatency 3
#define MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)
  real32 TargetSecondsPerFrame = 1.0f / (real32) GameUpdateHz;

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
      win32_sound_output SoundOutput = {};

      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.RunningSampleIndex = 0;
      SoundOutput.BytesPerSample = sizeof(int16) * 2;
      SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
      SoundOutput.LatencySampleCount = FramesOfAudioLatency * (SoundOutput.SamplesPerSecond / GameUpdateHz);
      Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
      Win32ClearBuffer(&SoundOutput);
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      GlobalRunning = true;

      int16 *Samples = (int16 *) VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                              MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
      LPVOID BaseAddress = (LPVOID) Terabytes(2);
#else
      LPVOID BaseAddress = 0;
#endif

      game_memory GameMemory = {};
      GameMemory.PermanentStorageSize = Megabytes(64);
      GameMemory.TransientStorageSize = Gigabytes(1);
      uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

      GameMemory.PermanentStorage =
          VirtualAlloc(BaseAddress, (size_t) TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      GameMemory.TransientStorage =
          (uint8 *) GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;

      if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
      {
        game_input Input[2] = {};
        game_input *NewInput = &Input[0];
        game_input *OldInput = &Input[1];

        LARGE_INTEGER LastCounter = Win32GetWallClock();

        int DebugTimeMarkerIndex = 0;
        win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {0};

        DWORD LastPlayCursor = 0;
        bool32 SoundIsValid = false;

        uint64 LastCycleCount = __rdtsc();

        while (GlobalRunning)
        {
          game_controller_input *OldKeyboardController = GetController(OldInput, 0);
          game_controller_input *NewKeyboardController = GetController(NewInput, 0);
          *NewKeyboardController = {};
          NewKeyboardController->IsConnected = true;

          for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
               ++ButtonIndex)
          {
            NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                OldKeyboardController->Buttons[ButtonIndex].EndedDown;
          }

          Win32ProcessPendingMessages(NewKeyboardController);

          // TODO Need to not poll disconnected controllers to avoid xinput frame rate hit on older
          // libraries.
          // TODO Should we poll this more frequently?
          DWORD MaxControllerCount = XUSER_MAX_COUNT;

          if (MaxControllerCount > ArrayCount(NewInput->Controllers) - 1)
          {
            MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
          }

          for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex)
          {
            DWORD OurControllerIndex = ControllerIndex + 1;
            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

            XINPUT_STATE ControllerState;

            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
            {
              NewController->IsConnected = true;
              // This controller is plugged in.
              // TODO See if ControllerState.dwPacketNumber increments too rapidly.
              XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

              // TODO This is a square deadzone, check XInput to verify that the deadzone is
              // "round".
              NewController->StickAverageX =
                  Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
              NewController->StickAverageY =
                  Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

              if (NewController->StickAverageX != 0.0f || NewController->StickAverageY != 0.0f)
              {
                NewController->IsAnalog = true;
              }

              if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
              {
                NewController->StickAverageY = 1.0f;
                NewController->IsAnalog = false;
              }
              if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
              {
                NewController->StickAverageY = -1.0f;
                NewController->IsAnalog = false;
              }
              if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
              {
                NewController->StickAverageX = -1.0f;
                NewController->IsAnalog = false;
              }
              if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
              {
                NewController->StickAverageX = 1.0f;
                NewController->IsAnalog = false;
              }

              real32 Threshold = 0.5f;

              Win32ProcessXInputDigitalButton(NewController->StickAverageX < -Threshold ? 1 : 0,
                                              &OldController->MoveLeft, 1,
                                              &NewController->MoveLeft);
              Win32ProcessXInputDigitalButton(NewController->StickAverageX > Threshold ? 1 : 0,
                                              &OldController->MoveRight, 1,
                                              &NewController->MoveRight);
              Win32ProcessXInputDigitalButton(NewController->StickAverageY < -Threshold ? 1 : 0,
                                              &OldController->MoveDown, 1,
                                              &NewController->MoveDown);
              Win32ProcessXInputDigitalButton(NewController->StickAverageY > Threshold ? 1 : 0,
                                              &OldController->MoveUp, 1, &NewController->MoveUp);

              DWORD FakeMoveButtons = NewController->StickAverageX < -Threshold ? 1 : 0;

              Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown,
                                              XINPUT_GAMEPAD_A, &NewController->ActionDown);
              Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight,
                                              XINPUT_GAMEPAD_B, &NewController->ActionRight);
              Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft,
                                              XINPUT_GAMEPAD_X, &NewController->ActionLeft);
              Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp,
                                              XINPUT_GAMEPAD_Y, &NewController->ActionUp);
              Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder,
                                              XINPUT_GAMEPAD_LEFT_SHOULDER,
                                              &NewController->LeftShoulder);
              Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder,
                                              XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                              &NewController->RightShoulder);

              Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back,
                                              XINPUT_GAMEPAD_BACK, &NewController->Back);
              Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start,
                                              XINPUT_GAMEPAD_START, &NewController->Start);
            }
            else
            {
              // The controller is not available.
              NewController->IsConnected = false;
            }
          }

          // Compute how much sound to write and where.
          DWORD ByteToLock = 0;
          DWORD TargetCursor = 0;
          DWORD BytesToWrite = 0;

          if (SoundIsValid)
          {
            ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
                         SoundOutput.SecondaryBufferSize;
            TargetCursor =
                (LastPlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
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
          GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);

          if (SoundIsValid)
          {
            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
          }

          LARGE_INTEGER WorkCounter = Win32GetWallClock();

          real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
          real32 SecondsElapsedForFrame = WorkSecondsElapsed;

          if (SecondsElapsedForFrame < TargetSecondsPerFrame)
          {
            if (SleepIsGranular)
            {
              DWORD SleepMs = (DWORD) (1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));

              if (SleepMs > 0)
              {
                Sleep(SleepMs);
              }
            }

            real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
            // TODO (Chris) Why does this break on my machine but not Casey's?
            //Assert(TestSecondsElapsedForFrame < TargetSecondsPerFrame);

            while (SecondsElapsedForFrame < TargetSecondsPerFrame)
            {
              SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
            }
          }
          else
          {
            // TODO MISSED FRAME RATE!
          }

          LARGE_INTEGER EndCounter = Win32GetWallClock();
          real32 MsPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
          LastCounter = EndCounter;

          win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
          Win32DebugSyncDisplay(&GlobalBackbuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers,
                                &SoundOutput, TargetSecondsPerFrame);
#endif
          Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width,
                                     Dimension.Height);

          DWORD PlayCursor;
          DWORD WriteCursor;

          if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
          {
            LastPlayCursor = PlayCursor;

            if (!SoundIsValid)
            {
              SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
              SoundIsValid = true;
            }
          }
          else
          {
            SoundIsValid = false;
          }
#if HANDMADE_INTERNAL
          {
            win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex++];

            if (DebugTimeMarkerIndex > ArrayCount(DebugTimeMarkers))
            {
              DebugTimeMarkerIndex = 0;
            }

            Marker->PlayCursor = PlayCursor;
            Marker->WriteCursor = WriteCursor;
          }
#endif

          game_input *Temp = NewInput;
          NewInput = OldInput;
          OldInput = Temp;
          // TODO Should these be cleared here?

          uint64 EndCycleCount = __rdtsc();
          uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
          LastCycleCount = EndCycleCount;

          real64 FPS = 0.0f;
          real64 MCPF = (real64) CyclesElapsed / (1000.0f * 1000.0f);

          char FpsBuffer[256];
          _snprintf_s(FpsBuffer, sizeof(FpsBuffer), "%.02fms/f,  %.02ff/s,  %.02fmc/f\n", MsPerFrame, FPS, MCPF);
          OutputDebugStringA(FpsBuffer);
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
  else
  {
    // TODO Logging
  }

  return 0;
}

