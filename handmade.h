#if !defined(HANDMADE_H)

/*
 * TODO Services that the platform layer provides to the game.
 */

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

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset,
    game_sound_output_buffer *SoundBuffer);

#define HANDMADE_H
#endif

