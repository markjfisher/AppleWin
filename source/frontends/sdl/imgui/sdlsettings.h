#pragma once

#include "frontends/sdl/imgui/gles.h"

namespace sa2
{

  class SDLFrame;

  class ImGuiSettings
  {
  public:
    void show(SDLFrame* frame);
    float drawMenuBar();

    bool windowed = false;

  private:
    bool myShowDemo = false;
    bool myShowSettings = false;
    bool myShowMemory = false;
    bool myShowDebugger = false;
    bool mySyncCPU = true;

    int myStepCycles = 0;
    int mySpeakerVolume = 50;
    int myMockingboardVolume = 50;

    MemoryEditor myMainMemoryEditor;
    MemoryEditor myAuxMemoryEditor;

    void showSettings();
    void showDebugger(SDLFrame* frame);
    void showMemory();

    void drawDisassemblyTable();
    void drawConsole();
  };

}