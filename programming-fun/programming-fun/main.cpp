/**
 * \file
 * \author Rudy Castan
 * \date 2024 Fall
 * \copyright DigiPen Institute of Technology
 */

#include <GL/glew.h>
#include <SDL.h>
#include <al.h>
#include <alc.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <filesystem>
#include <fstream>
#include <glm/vec3.hpp> // vec3, bvec3, dvec3, ivec3 and uvec3
#include <gsl/gsl>
#include <imgui.h>
#include <iostream>
#include <optional>
#include <sstream>
#include <stb_image.h>
#include <vector>
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

namespace
{
    int                   gWindowWidth  = 640;
    int                   gWindowHeight = 480;
    [[maybe_unused]] bool gNeedResize   = false;

    class Demo
    {
    public:
        void Setup();
        void Shutdown();
        void SetDisplaySize(int width, int height);
        void Draw() const;
        void ImGuiDraw();

    private:
        glm::vec3 background_color{ 0.392f, 0.584f, 0.929f }; // https://www.colorhexa.com/6495ed

        struct
        {
            GLuint handle = 0;
            int    width  = 0;
            int    height = 0;
            bool   loaded = false;
        } example_image;

        ALuint alBufferHandles[2] = { 0 };
        ALuint alSourceHandles[2] = { 0 };
    };

    class [[nodiscard]] Application
    {
    public:
        Application(gsl::czstring title = "Programming Fun App");
        ~Application();

        Application(const Application&)                = delete;
        Application& operator=(const Application&)     = delete;
        Application(Application&&) noexcept            = delete;
        Application& operator=(Application&&) noexcept = delete;

        void Update();
        bool IsDone() const noexcept;

        [[maybe_unused]] void ForceResize(int desired_width, int desired_height) const;

    private:
        void setupSDLWindow(gsl::czstring title);
        void setupOpenGL();
        void setupImGui();
        void updateWindowEvents();

    private:
        Demo                      demo;
        gsl::owner<SDL_Window*>   ptr_window = nullptr;
        gsl::owner<SDL_GLContext> gl_context = nullptr;
        gsl::owner<ALCdevice*>    al_device  = nullptr;
        gsl::owner<ALCcontext*>   al_context = nullptr;
        bool                      is_done    = false;
    };
}

#if defined(__EMSCRIPTEN__)
#    include <emscripten.h>
#    include <emscripten/bind.h>

void main_loop(Application* application)
{
    if (gNeedResize)
    {
        application->ForceResize(gWindowWidth, gWindowHeight);
        gNeedResize = false;
    }
    application->Update();
    if (application->IsDone())
        emscripten_cancel_main_loop();
}

EMSCRIPTEN_BINDINGS(main_window)
{
    emscripten::function(
        "setWindowSize", emscripten::optional_override(
                             [](int sizeX, int sizeY)
                             {
                                 sizeX = (sizeX < 400) ? 400 : sizeX;
                                 sizeY = (sizeY < 400) ? 400 : sizeY;
                                 if (sizeX != gWindowWidth || sizeY != gWindowHeight)
                                 {
                                     gNeedResize   = true;
                                     gWindowWidth  = sizeX;
                                     gWindowHeight = sizeY;
                                 }
                             }));
}
#endif

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
try
{
    Application application;
#if !defined(__EMSCRIPTEN__)
    while (!application.IsDone())
    {
        application.Update();
    }
#else
    // https://kripken.github.io/emscripten-site/docs/api_reference/emscripten.h.html#c.emscripten_set_main_loop_arg
    int simulate_infinite_loop  = 1;
    int match_browser_framerate = -1;
    emscripten_set_main_loop_arg(reinterpret_cast<void (*)(void*)>(&main_loop), reinterpret_cast<void*>(&application), match_browser_framerate, simulate_infinite_loop);
#endif

    return 0;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << '\n';
    return -1;
}

namespace
{
    void hint_gl(SDL_GLattr attr, int value)
    {
        // https://wiki.libsdl.org/SDL_GL_SetAttribute
        if (const auto success = SDL_GL_SetAttribute(attr, value); success != 0)
        {
            std::cerr << "Failed to Set GL Attribute: " << SDL_GetError() << '\n';
        }
    }

    template <typename... Messages>
    [[noreturn]] void throw_error_message(Messages&&... more_messages)
    {
        std::ostringstream sout;
        (sout << ... << more_messages);
        std::cerr << sout.str() << '\n';
        throw std::runtime_error{ sout.str() };
    }
}

Application::Application(gsl::czstring title)
{
    if (title == nullptr || title[0] == '\0')
        throw_error_message("App title shouldn't be empty");
    setupSDLWindow(title);
    setupOpenGL();
    SDL_GetWindowSize(ptr_window, &gWindowWidth, &gWindowHeight);
    setupImGui();
    al_device  = alcOpenDevice(nullptr);
    al_context = alcCreateContext(al_device, nullptr);
    alcMakeContextCurrent(al_context);
    demo.Setup();
}

Application::~Application()
{
    demo.Shutdown();

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(al_context);
    alcCloseDevice(al_device);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(ptr_window);
    SDL_Quit();
}

void Application::setupSDLWindow(gsl::czstring title)
{
    // https://wiki.libsdl.org/SDL_Init
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        throw_error_message("Failed to init SDK error: ", SDL_GetError());
    }

#if defined(IS_WEBGL2)
    hint_gl(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    hint_gl(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    hint_gl(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
// else Desktop will pick the highest OpenGL version by default
#else
    hint_gl(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    hint_gl(SDL_GL_DOUBLEBUFFER, true);
    hint_gl(SDL_GL_STENCIL_SIZE, 8);
    hint_gl(SDL_GL_DEPTH_SIZE, 24);
    hint_gl(SDL_GL_RED_SIZE, 8);
    hint_gl(SDL_GL_GREEN_SIZE, 8);
    hint_gl(SDL_GL_BLUE_SIZE, 8);
    hint_gl(SDL_GL_ALPHA_SIZE, 8);
    hint_gl(SDL_GL_MULTISAMPLEBUFFERS, 1);
    hint_gl(SDL_GL_MULTISAMPLESAMPLES, 4);


    // https://wiki.libsdl.org/SDL_CreateWindow
    ptr_window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, gWindowWidth, gWindowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (ptr_window == nullptr)
    {
        throw_error_message("Failed to create window: ", SDL_GetError());
    }
}

void Application::setupOpenGL()
{
    // https://wiki.libsdl.org/SDL_GL_CreateContext
    if (gl_context = SDL_GL_CreateContext(ptr_window); gl_context == nullptr)
    {
        throw_error_message("Failed to create opengl context: ", SDL_GetError());
    }

    // https://wiki.libsdl.org/SDL_GL_MakeCurrent
    SDL_GL_MakeCurrent(ptr_window, gl_context);

    // http://glew.sourceforge.net/basic.html
    if (const auto result = glewInit(); GLEW_OK != result)
    {
        throw_error_message("Unable to initialize GLEW - error: ", glewGetErrorString(result));
    }

    // https://wiki.libsdl.org/SDL_GL_SetSwapInterval
    constexpr int ADAPTIVE_VSYNC = -1;
    constexpr int VSYNC          = 1;
    if (const auto result = SDL_GL_SetSwapInterval(ADAPTIVE_VSYNC); result != 0)
    {
        SDL_GL_SetSwapInterval(VSYNC);
    }
}

void Application::setupImGui()
{
    IMGUI_CHECKVERSION();
    ImGui ::CreateContext();
    {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }
    ImGui_ImplSDL2_InitForOpenGL(ptr_window, gl_context);
    ImGui_ImplOpenGL3_Init();
}

void Application::Update()
{
    updateWindowEvents();
    demo.Draw();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    demo.ImGuiDraw();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    const ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(ptr_window, gl_context);
    }
    SDL_GL_SwapWindow(ptr_window);
}

void Application::updateWindowEvents()
{
    SDL_Event event = { 0 };
    while (SDL_PollEvent(&event) != 0)
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        switch (event.type)
        {
            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        {
                            is_done = true;
                        }
                        break;
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        {
                            gWindowWidth  = event.window.data1;
                            gWindowHeight = event.window.data2;
                            demo.SetDisplaySize(gWindowWidth, gWindowHeight);
                        }
                        break;
                }
                break;
            case SDL_QUIT: [[unlikely]] is_done = true; break;
        }
    }
}

bool Application::IsDone() const noexcept
{
    return is_done;
}

void Application::ForceResize(int desired_width, int desired_height) const
{
    SDL_SetWindowSize(ptr_window, desired_width, desired_height);
}

namespace
{
    std::optional<std::filesystem::path> try_get_asset_path(const std::filesystem::path& starting_directory)
    {
        namespace fs        = std::filesystem;
        fs::path       p    = starting_directory;
        const fs::path root = p.root_path();
        do
        {
            fs::path assets_folder = fs::absolute(p / "assets");
            if (fs::is_directory(assets_folder))
            {
                return std::optional<fs::path>{ assets_folder };
            }
            p = fs::absolute(p / "..");
        } while (fs::absolute(p) != root);
        return std::optional<fs::path>{};
    }

    std::filesystem::path get_base_path()
    {
        namespace fs                  = std::filesystem;
        static fs::path assets_folder = []()
        {
            auto result = try_get_asset_path(fs::current_path());
            if (result)
                return result.value();
            // try from the exe path rather than the current working directory
            const auto base_path = SDL_GetBasePath();
            result               = try_get_asset_path(base_path);
            SDL_free(base_path);
            if (result)
                return result.value();
            throw std::runtime_error{ "Failed to find assets folder in parent folders" };
        }();
        return assets_folder;
    }

    // https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples
    bool LoadTextureFromFile(const std::filesystem::path& filename, GLuint& out_texture, int& out_width, int& out_height)
    {
        // Load from file
        int            image_width  = 0;
        int            image_height = 0;
        unsigned char* image_data   = stbi_load(filename.string().c_str(), &image_width, &image_height, NULL, 4);
        if (image_data == NULL)
            return false;

        // Create a OpenGL texture identifier
        GLuint image_texture;
        glGenTextures(1, &image_texture);
        glBindTexture(GL_TEXTURE_2D, image_texture);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        stbi_image_free(image_data);

        out_texture = image_texture;
        out_width   = image_width;
        out_height  = image_height;

        return true;
    }
}

namespace
{
    ALenum get_openal_format(const SDL_AudioSpec* spec)
    {
        if ((spec->channels == 1) && (spec->format == AUDIO_U8))
        {
            return AL_FORMAT_MONO8;
        }
        else if ((spec->channels == 1) && (spec->format == AUDIO_S16SYS))
        {
            return AL_FORMAT_MONO16;
        }
        else if ((spec->channels == 2) && (spec->format == AUDIO_U8))
        {
            return AL_FORMAT_STEREO8;
        }
        else if ((spec->channels == 2) && (spec->format == AUDIO_S16SYS))
        {
            return AL_FORMAT_STEREO16;
        }
        else if ((spec->channels == 1) && (spec->format == AUDIO_F32SYS))
        {
            return alIsExtensionPresent("AL_EXT_FLOAT32") ? alGetEnumValue("AL_FORMAT_MONO_FLOAT32") : AL_NONE;
        }
        else if ((spec->channels == 2) && (spec->format == AUDIO_F32SYS))
        {
            return alIsExtensionPresent("AL_EXT_FLOAT32") ? alGetEnumValue("AL_FORMAT_STEREO_FLOAT32") : AL_NONE;
        }
        return AL_NONE;
    }
}

void Demo::Setup()
{
    glViewport(0, 0, gWindowWidth, gWindowHeight);
    glClearColor(background_color.r, background_color.g, background_color.b, 1.0f);
    example_image.loaded = LoadTextureFromFile(::get_base_path() / "images" / "duck.png", example_image.handle, example_image.width, example_image.height);

    alGenBuffers(2, alBufferHandles);
    alGenSources(2, alSourceHandles);

    ALenum  format = 0;
    ALvoid* data   = nullptr;
    ALsizei size   = 0;
    ALsizei freq   = 0;

    SDL_AudioSpec wavSpec;
    Uint32        wavLength;
    Uint8*        wavBuffer;
    const auto    e_path = get_base_path() / "audio" / "duck-quacking-loudly-three-times.wav";
    if (SDL_LoadWAV(e_path.string().c_str(), &wavSpec, &wavBuffer, &wavLength) == nullptr)
    {
        throw_error_message("Failed to load WAV file: ", e_path, SDL_GetError());
    }
    format = get_openal_format(&wavSpec);
    data   = wavBuffer;
    size   = gsl::narrow_cast<ALsizei>(wavLength);
    freq   = wavSpec.freq;
    alBufferData(alBufferHandles[0], format, data, size, freq);
    SDL_FreeWAV(wavBuffer);
    alSourcei(alSourceHandles[0], AL_BUFFER, gsl::narrow_cast<ALint>(alBufferHandles[0]));

    int        channels, sample_rate;
    short*     output;
    const auto stereo_path = get_base_path() / "audio" / "duck_vocalizations.ogg";
    int        samples     = stb_vorbis_decode_filename(stereo_path.string().c_str(), &channels, &sample_rate, &output);
    if (samples == -1)
    {
        throw_error_message("Failed to load OGG file: ", stereo_path);
    }

    format = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
    data   = output;
    size   = samples * channels * int(sizeof(short));
    freq   = sample_rate;
    alBufferData(alBufferHandles[1], format, data, size, freq);
    free(output);
    alSourcei(alSourceHandles[1], AL_BUFFER, gsl::narrow_cast<ALint>(alBufferHandles[1]));
}

void Demo::Shutdown()
{
    if (example_image.loaded)
    {
        glDeleteTextures(1, &example_image.handle);
    }

    alDeleteSources(2, alSourceHandles);
    alDeleteBuffers(2, alBufferHandles);
}

void Demo::Draw() const
{
    glClearColor(background_color.r, background_color.g, background_color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Demo::ImGuiDraw()
{
    ImGui::Begin("OpenGL Texture Test");
    if (example_image.loaded)
    {
        ImGui::Text("handle = %d", example_image.handle);
        ImGui::Text("size = %d x %d", example_image.width, example_image.height);
        ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(example_image.handle)), ImVec2(static_cast<float>(example_image.width), static_cast<float>(example_image.height)));
    }
    else
    {
        ImGui::Text("%s", "Failed to load texture image...");
    }
    ImGui::End();

    ImGui::Begin("Audio Test");
    {
        if (ImGui::Button("Play Mono SFX"))
        {
            alSourcePlay(alSourceHandles[0]);
        }
        ImGui::SameLine();
        if (ImGui::Button("Play Stereo SFX"))
        {
            alSourcePlay(alSourceHandles[1]);
        }
    }
    ImGui::End();
}

void Demo::SetDisplaySize(int width, int height)
{
    glViewport(0, 0, width, height);
}
