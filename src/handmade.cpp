#include "handmade.h"

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz)
{
  local_persist real32 tSine;
  int16 ToneVolume = 3000;
  int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
  int16 *SampleOut = SoundBuffer->Samples;

  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
  {
    real32 SineValue = sinf(tSine);
    int16 SampleValue = (int16)(SineValue * ToneVolume);
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;

    tSine += 2.0f * Pi32 / (real32) WavePeriod;
  }
}

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
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

internal void GameUpdateAndRender(game_memory *Memory, game_input *Input,
                                  game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer)
{
  Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

  game_state *GameState = (game_state *) Memory->PermanentStorage;

  if (!Memory->IsInitialized)
  {
    GameState->ToneHz = 256;
    Memory->IsInitialized = true;
  }

  game_controller_input *Input0 = &Input->Controllers[0];

  if (Input0->IsAnalog)
  {
    GameState->BlueOffset += (int) (4.0f * Input0->EndX);
    GameState->ToneHz = 256 + (int) (128.0f * Input0->EndY);
  }
  else
  {
  }

  // Input.AButtonEndedDown;
  // Input.AButtonHalfTransitionCount;
  if (Input0->Down.EndedDown)
  {
    GameState->GreenOffset += 1;
  }

  // TODO Allow sample offsets here for more robust platform options.
  GameOutputSound(SoundBuffer, GameState->ToneHz);
  RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}
