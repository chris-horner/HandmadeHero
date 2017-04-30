#if !defined(HANDMADE_H)

/*
 * HANDMADE_INTERNAL:
 *  0 - Build for public release
 *  1 = Build for developer only
 *
 * HANDMADE_SLOW:
 *  0 - No slow code allowed
 *  1 - Slow code welcome
 */

#if HANDMADE_SLOW
#define Assert(Expression) \
  if (!(Expression)) *(int *) 0 = 0;
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) (Value * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32 SafeTruncateUInt64(uint64 value)
{
  // TODO Defines for maximum values.
  Assert(value <= 0xFFFFFFFF);
  return (uint32) value;
}

/*
 * Services that the platform layer provides to the game.
 */
#if HANDMADE_INTERNAL
/* IMPORTANT
 *
 * These are NOT for doing anything in the shipping game - they are blocking and the write
 * doesn't protect against data loss!
 */
struct debug_read_file_result
{
  uint32 ContentsSize;
  void *Contents;
};

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename);
internal void DEBUGPlatformFreeFileMemory(void *BitmapMemory);
internal bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory);
#endif

/*
 * Services that the game provides to the platform layer.
 */

struct game_offscreen_buffer
{
  // Pixels are always 32 bits wide. Memory order: 0x xx RR GG BB
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};

struct game_sound_output_buffer
{
  int SamplesPerSecond;
  int SampleCount;
  int16 *Samples;
};

struct game_button_state
{
  int HalfTransitionCount;
  bool32 EndedDown;
};

struct game_controller_input
{
  bool32 IsConnected;
  bool32 IsAnalog;
  real32 StickAverageX;
  real32 StickAverageY;

  union {
    game_button_state Buttons[12];

    struct
    {
      game_button_state MoveUp;
      game_button_state MoveDown;
      game_button_state MoveLeft;
      game_button_state MoveRight;

      game_button_state ActionUp;
      game_button_state ActionDown;
      game_button_state ActionLeft;
      game_button_state ActionRight;

      game_button_state LeftShoulder;
      game_button_state RightShoulder;

      game_button_state Back;
      game_button_state Start;
    };
  };
};

struct game_input
{
  game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input *Input, int ControllerIndex)
{
  Assert(ControllerIndex < ArrayCount(Input->Controllers));
  game_controller_input *Result = &Input->Controllers[ControllerIndex];
  return Result;
}

struct game_memory
{
  bool32 IsInitialized;

  uint64 PermanentStorageSize;
  void *PermanentStorage;  // NOTE: REQUIRED to be cleared to zero at startup.

  uint64 TransientStorageSize;
  void *TransientStorage;  // NOTE: REQUIRED to be cleared to zero at startup.
};

internal void GameUpdateAndRender(game_memory *Memory, game_input *Input,
                                  game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer);

struct game_state
{
  int ToneHz;
  int GreenOffset;
  int BlueOffset;
};

#define HANDMADE_H
#endif

