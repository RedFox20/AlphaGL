//
// Created by jorma on 24.05.17.
//
#pragma once
#include "Shader.h"
#include "AGLConfig.h"
#include "GLInput.h"
#include <rpp/timer.h>
#include <memory> // std::unique_ptr

namespace AGL
{
    using rpp::Color3;
    using rpp::Ray;

    struct GLRendererCtx;

    class AGL_API GLCore
    {
        GLRendererCtx& gl;

    public:
        std::unique_ptr<class SceneRoot> SceneRoot;

        rpp::Timer FrameTimer;
        float DeltaTime = 0.0f;

        /**
         * Creates a new headless or windowed OpenGL renderer
         * @param width Width of the window OR pixel buffer
         * @param height Height of the window OR pixel buffer
         * @param createWindow If TRUE create window, FALSE create headless
         */
        GLCore(int width, int height, bool createWindow);
        ~GLCore();

        GLCore(const GLCore&)=delete;
        GLCore& operator=(const GLCore&)=delete;

        void SetTitle(const string& title);

        GLInput& Input();
        Shader& Color3dShader();
        Shader& VertexColor3dShader();
        Shader& Simple3dShader();

        // Width & Height of the current render target
        int ContextWidth()  const;
        int ContextHeight() const;

        /**
         * Saves current framebuffer state into a BMP file
         */
        void SaveFrameBuffer(const string& bmpFile) const;

        /**
         * Converts current framebuffer texture to a bitmap
         */
        AGL::Bitmap GetFrameBuffer() const;

        bool WindowShouldClose() const;

        // normalized color [0..1]
        void Clear(float red, float green, float blue);
        // classic 0..255 color
        void Clear(int red, int green, int blue) { Clear(red/255.0f, green/255.0f, blue/255.0f); }
        void Clear(const Color3& color) { Clear(color.r, color.g, color.b); }
        void Clear(const Color& color)  { Clear(color.r, color.g, color.b); }

        void SwapBuffers();

        // Unprojects 2D screen coordinates into a 3D world ray
        // Ray origin is the Camera center
        Ray Unproject(int x, int y) const;

        class SceneRoot* CreateSceneRoot(string name);

        /**
         * Update deltaTime
         * Poll input events
         * Update scene
         * Render scene
         */
        void UpdateAndRender();
    };
}
