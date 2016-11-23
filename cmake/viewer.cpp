/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "../include/core/SkCanvas.h"
#include "../include/core/SkTypeface.h"
#include "../include/core/SkStream.h"
#include "../include/core/SkData.h"
#include "../include/core/SkSurface.h"
#include "../include/core/SkRefCnt.h"
#include "../include/effects/SkGradientShader.h"
#include "../include/gpu/GrContext.h"
#include "../include/gpu/gl/GrGLInterface.h"
//#include "../include/gpu/gl/GrGLDefines.h"
#include "../include/core/SkPictureRecorder.h"
#include "../tools/Resources.h"
#include "yaml.h"

// These headers are just handy for writing this example file.  Nothing Skia specific.
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <assert.h>

#include <chrono>

extern void drawYAMLFile(SkCanvas *canvas, const char *file);

// These setup_gl_context() are not meant to represent good form.
// They are just quick hacks to get us going.
#if defined(__APPLE__)
    #include <OpenGL/OpenGL.h>
    static bool setup_gl_context() {
        CGLPixelFormatAttribute attributes[] = { (CGLPixelFormatAttribute)0 };
        CGLPixelFormatObj format;
        GLint npix;
        CGLChoosePixelFormat(attributes, &format, &npix);
        CGLContextObj context;
        CGLCreateContext(format, nullptr, &context);
        CGLSetCurrentContext(context);
        CGLReleasePixelFormat(format);
        return true;
    }
#else
    static bool setup_gl_context() {
        return false;
    }
#endif

// Most pointers returned by Skia are derived from SkRefCnt,
// meaning we need to call ->unref() on them when done rather than delete them.
template <typename T> std::shared_ptr<T> adopt(T* ptr) {
    return std::shared_ptr<T>(ptr, [](T* p) { p->unref(); });
}

static std::shared_ptr<SkSurface> create_raster_surface(int w, int h) {
    std::cout << "Using raster surface" << std::endl;
    return adopt(SkSurface::MakeRasterN32Premul(w, h).release());
}

static std::shared_ptr<SkSurface> create_opengl_surface(int w, int h) {
    std::cout << "Using opengl surface" << std::endl;
    std::shared_ptr<GrContext> grContext = adopt(GrContext::Create(kOpenGL_GrBackend, 0));
    return adopt(SkSurface::MakeRenderTarget(grContext.get(),
                                            SkBudgeted::kNo,
                                            SkImageInfo::MakeN32Premul(w,h)).release());
}


sk_sp<SkImage> GetResourceAsImage(const char* path) {
    sk_sp<SkData> resourceData(SkData::MakeFromFileName(path));
    return SkImage::MakeFromEncoded(resourceData);
}


extern SkImageEncoder_EncodeReg gEReg;
SkImageEncoder_EncodeReg *k = &gEReg;

#include <GL/glut.h>



sk_sp<SkPicture> gPic;
void draw(void) {
    sk_sp<const GrGLInterface> fBackendContext;

    sk_sp<const GrGLInterface> glInterface;
    glInterface.reset(GrGLCreateNativeInterface());
    fBackendContext.reset(GrGLInterfaceRemoveNVPR(glInterface.get()));

    //SkASSERT(nullptr == fContext);
    auto fContext = GrContext::Create(kOpenGL_GrBackend, (GrBackendContext)fBackendContext.get());

    sk_sp<SkSurface> fSurface;
if (nullptr == fSurface) {
    auto fActualColorBits =  24;

    int width = 1024;
    int height = 768;
    if (fContext) {
	GrBackendRenderTargetDesc desc;
	desc.fWidth = width;
	desc.fHeight = height;
	desc.fConfig = kRGBA_8888_GrPixelConfig;
	desc.fOrigin = kBottomLeft_GrSurfaceOrigin;
	desc.fSampleCnt = 0;
	desc.fStencilBits = 0;
	GrGLint buffer;

    #ifndef GL_FRAMEBUFFER_BINDING
    #define GL_FRAMEBUFFER_BINDING            0x8CA6
    #endif

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
	desc.fRenderTargetHandle = buffer;
    SkSurfaceProps    fSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType);

	fSurface = SkSurface::MakeFromBackendRenderTarget(fContext, desc, &fSurfaceProps);
	
    }
}

    glClearColor(0.0f,1.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    SkCanvas* canvas = fSurface->getCanvas();   // We don't manage this pointer's lifetime.

    auto before = std::chrono::high_resolution_clock::now();

    int k;
    while (1) {
//    gPic->playback(canvas);

///    canvas->clear(0xff000000 | k++);
    canvas->drawPicture(gPic);
            canvas->flush();


    // Black background
    //Draw i
    glFlush();

    auto after = std::chrono::high_resolution_clock::now();

    using FpMilliseconds = 
        std::chrono::duration<double, std::chrono::milliseconds::period>;

    printf("%f\n", FpMilliseconds(after - before).count());
    before = after;
    }

}

int main(int argc, char** argv) {
    bool gl_ok = setup_gl_context();
    srand((unsigned)time(nullptr));
    int width = 1024;
    int height = 2048;
    std::shared_ptr<SkSurface> surface = (gl_ok && rand() % 2) ? create_opengl_surface(width, height)
                                                               : create_raster_surface(width, height);



    // Create a left-to-right green-to-purple gradient shader.
    SkPoint pts[] = { {0,0}, {320,240} };
    SkColor colors[] = { 0xFF00FF00, 0xFFFF00FF };
    // Our text will draw with this paint: size 24, antialiased, with the shader.
    SkPaint paint;
    paint.setTextSize(24);
    paint.setAntiAlias(true);
    paint.setShader(SkGradientShader::MakeLinear(pts, colors, nullptr, 2, SkShader::kRepeat_TileMode));

    SkPictureRecorder recorder;
#define INDIRECT
#ifdef INDIRECT
    SkCanvas* canvas = recorder.beginRecording(width, height, nullptr, 0);
#else
    SkCanvas* canvas = surface->getCanvas();   // We don't manage this pointer's lifetime.
#endif

    static const char* msg = "Hello world!";
    canvas->clear(SK_ColorRED);
    //canvas->drawText(msg, strlen(msg), 90,120, paint);

    drawYAMLFile(canvas, argv[1]);

#ifdef INDIRECT
    sk_sp<SkPicture> pic = recorder.finishRecordingAsPicture();
    // Draw to the surface via its SkCanvas.
    gPic = pic;
    SkCanvas* ic = surface->getCanvas();   // We don't manage this pointer's lifetime.

    pic->playback(ic);

    SkFILEWStream stream("out.skp");
    pic->serialize(&stream);
#endif
    // Grab a snapshot of the surface as an immutable SkImage.
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    // Encode that image as a .png into a blob in memory.
    std::shared_ptr<SkData> png = adopt(image->encode(SkImageEncoder::kPNG_Type, 100));

    // This code is no longer Skia-specific.  We just dump the .png to disk.  Any way works.
    static const char* path = "example.png";
    std::ofstream(path, std::ios::out | std::ios::binary)
        .write((const char*)png->data(), png->size());
    std::cout << "Wrote " << path << std::endl;

    glutInit(&argc, argv);

    /*Setting up  The Display
    /    -RGB color model + Alpha Channel = GLUT_RGBA
    */
    glutInitDisplayMode(GLUT_RGBA|GLUT_SINGLE);

    //Configure Window Postion
    glutInitWindowPosition(50, 25);

    //Configure Window Size
    glutInitWindowSize(1024,768);

    //Create Window
    glutCreateWindow("Hello OpenGL");


    //Call to the drawing function
    glutDisplayFunc(draw);

    // Loop require by OpenGL
    glutMainLoop();

    return 0;
}

