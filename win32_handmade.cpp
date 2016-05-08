#include<windows.h>
#include<stdint.h>

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

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
  win32_window_dimension Result;

  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;

  return Result;
}

internal void RenderWeirdGradient(win32_offscreen_buffer Buffer, int BlueOffset, int GreenOffset)
{
  // TODO Let's see what the optimiser does.
  uint8 *Row = (uint8 *) Buffer.Memory;

  for (int Y = 0; Y < Buffer.Height; ++Y)
  {
    uint32 *Pixel = (uint32 *) Row;

    for (int X = 0; X < Buffer.Width; ++X)
    {
      uint8 Blue = X + BlueOffset;
      uint8 Green = Y + GreenOffset;

      *Pixel++ = Green << 8 | Blue;
    }

    Row += Buffer.Pitch;
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
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Width * BytesPerPixel;

  // TODO Probably want to clear this to black.
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight,
    win32_offscreen_buffer Buffer,
    int X, int Y, int Width, int Height)
{
  // TODO Aspect ratio correction.
  StretchDIBits(DeviceContext,
      0, 0, WindowWidth, WindowHeight,
      0, 0, Buffer.Width, Buffer.Height,
      Buffer.Memory,
      &Buffer.Info,
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

    case WM_PAINT:
    {
      PAINTSTRUCT Paint;
      HDC DeviceContext = BeginPaint(Window, &Paint);
      int X = Paint.rcPaint.left;
      int Y = Paint.rcPaint.top;
      int Width = Paint.rcPaint.right - Paint.rcPaint.left;
      int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

      win32_window_dimension Dimension = Win32GetWindowDimension(Window);
      Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackbuffer, X, Y, Width, Height);
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
  WNDCLASS WindowClass = {};

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

      GlobalRunning = true;
      int XOffset = 0;
      int YOffset = 0;

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

        RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);

        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
            GlobalBackbuffer,
            0, 0, Dimension.Width, Dimension.Height);

        ++XOffset;
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

