#include "StdAfx.h"
#include "frontends/sdl/imgui/sdlimguiframe.h"

#include "frontends/sdl/utils.h"
#include "frontends/common2/fileregistry.h"
#include "frontends/common2/programoptions.h"
#include "frontends/sdl/imgui/image.h"

#include "Interface.h"
#include "Core.h"

#include <iostream>


namespace sa2
{

  SDLImGuiFrame::SDLImGuiFrame(const common2::EmulatorOptions & options)
    : SDLFrame(options)
  {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, SDL_CONTEXT_MAJOR); // from local gles.h
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    const SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    const common2::Geometry & geometry = options.geometry;

    myWindow.reset(SDL_CreateWindow(g_pAppTitle.c_str(), geometry.x, geometry.y, geometry.width, geometry.height, windowFlags), SDL_DestroyWindow);
    if (!myWindow)
    {
      throw std::runtime_error(SDL_GetError());
    }

    SetApplicationIcon();

    myGLContext = SDL_GL_CreateContext(myWindow.get());
    if (!myGLContext)
    {
      throw std::runtime_error(SDL_GetError());
    }

    SDL_GL_MakeCurrent(myWindow.get(), myGLContext);

    // Setup Platform/Renderer backends
    std::cerr << "IMGUI_VERSION: " << IMGUI_VERSION << std::endl;
    std::cerr << "GL_VENDOR: " << glGetString(GL_VENDOR) << std::endl;
    std::cerr << "GL_RENDERER: " << glGetString(GL_RENDERER) << std::endl;
    std::cerr << "GL_VERSION: " << glGetString(GL_VERSION) << std::endl;
    std::cerr << "GL_SHADING_LANGUAGE_VERSION: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    //  const char* runtime_gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
    //  std::cerr << "GL_EXTENSIONS: " << runtime_gl_extensions << std::endl;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    myIniFileLocation = common2::GetConfigFile("imgui.ini");
    if (myIniFileLocation.empty())
    {
      io.IniFilename = nullptr;
    }
    else
    {
      io.IniFilename = myIniFileLocation.c_str();
    }

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(myWindow.get(), myGLContext);

    ImGui_ImplOpenGL3_Init();

    glGenTextures(1, &myTexture);

    Video & video = GetVideo();

    myBorderlessWidth = video.GetFrameBufferBorderlessWidth();
    myBorderlessHeight = video.GetFrameBufferBorderlessHeight();

    const int width = video.GetFrameBufferWidth();
    const size_t borderWidth = video.GetFrameBufferBorderWidth();
    const size_t borderHeight = video.GetFrameBufferBorderHeight();

    myPitch = width;
    myOffset = (width * borderHeight + borderWidth) * sizeof(bgra_t);

    allocateTexture(myTexture, myBorderlessWidth, myBorderlessHeight);
  }

  SDLImGuiFrame::~SDLImGuiFrame()
  {
    glDeleteTextures(1, &myTexture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(myGLContext);
  }

  void SDLImGuiFrame::UpdateTexture()
  {
    loadTextureFromData(myTexture, myFramebuffer.data() + myOffset, myBorderlessWidth, myBorderlessHeight, myPitch);
  }

  void SDLImGuiFrame::ClearBackground()
  {
    const ImVec4 background(0.45f, 0.55f, 0.60f, 1.00f);
    glClearColor(background.x, background.y, background.z, background.w);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  void SDLImGuiFrame::DrawAppleVideo()
  {
    // need to flip the texture vertically
    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);

    const float menuBarHeight = mySettings.drawMenuBar();

    if (mySettings.windowed)
    {
      if (ImGui::Begin("Apple ]["))
      {
        ImGui::Image(myTexture, ImGui::GetContentRegionAvail(), uv0, uv1);
      }
      ImGui::End();
    }
    else
    {
      const ImVec2 zero(0, menuBarHeight);
      // draw on the background
      ImGuiIO& io = ImGui::GetIO();
      ImGui::GetBackgroundDrawList()->AddImage(myTexture, zero, io.DisplaySize, uv0, uv1);
    }
  }

  void SDLImGuiFrame::RenderPresent()
  {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(myWindow.get());
    ImGui::NewFrame();

    // "this" is a bit circular
    mySettings.show(this);
    DrawAppleVideo();

    ImGui::Render();
    ClearBackground();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(myWindow.get());
  }

  void SDLImGuiFrame::ProcessSingleEvent(const SDL_Event & event, bool & quit)
  {
    ImGui_ImplSDL2_ProcessEvent(&event);

    switch (event.type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    case SDL_TEXTINPUT:
      {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard)
        {
          return; // do not pass on
        }
      }
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEMOTION:
      {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
        {
          return; // do not pass on
        }
      }
    }

    SDLFrame::ProcessSingleEvent(event, quit);
  }

}