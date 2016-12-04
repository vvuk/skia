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
#include <vector>
#include <algorithm>
#include <numeric>
#include <assert.h>

#include <chrono>

#include <GLFW/glfw3.h>

#include "yaml-cpp/yaml.h"

static int gWidth = 1920;
static int gHeight = 1080;

extern YAML::Node loadYAMLFile(const char *file);
extern void drawYAMLFile(YAML::Node &doc, SkCanvas *canvas);

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


static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

void
dump_to_png(SkPicture *pic, const char *png_name)
{
    std::shared_ptr<SkSurface> surface = create_raster_surface(gWidth, gHeight);
    SkCanvas* ic = surface->getCanvas();   // We don't manage this pointer's lifetime.
    pic->playback(ic);

    sk_sp<SkImage> image = surface->makeImageSnapshot();
    std::shared_ptr<SkData> png = adopt(image->encode(SkImageEncoder::kPNG_Type, 100));

    std::ofstream(png_name, std::ios::out | std::ios::binary)
        .write((const char*)png->data(), png->size());
    std::cout << "Wrote " << png_name << std::endl;
}

void
usage()
{
    printf("Usage: viewer [-r] [-l seconds] [-w width] [-h height] [-s scale] file.yaml|file.skp\n");
    exit(1);
}

int
main(int argc, char** argv)
{
    bool should_rebuild_pic = false;
    const char *in_file = nullptr;
    const uint32_t frames_between_dumps = 60;
    uint32_t exit_after_seconds = 3;
    double scale = 1.0;

    if (argc == 1) {
        usage();
    }

    int n = 1;
    while (argv[n]) {
        if (strcmp(argv[n], "-r") == 0) {
            should_rebuild_pic = true;
        } else if (strcmp(argv[n], "-l") == 0) {
            exit_after_seconds = atoi(argv[n+1]);
            n++;
        } else if (strcmp(argv[n], "-w") == 0) {
            gWidth = atoi(argv[n+1]);
            n++;
        } else if (strcmp(argv[n], "-h") == 0) {
            gHeight = atoi(argv[n+1]);
            n++;
        } else if (strcmp(argv[n], "-s") == 0) {
            scale = atof(argv[n+1]);
            n++;
        } else if (!in_file) {
            in_file = argv[n];
        } else {
            usage();
        }
        n++;
    }

    sk_sp<SkPicture> pic;

    YAML::Node yaml_doc;
    if (strstr(in_file, ".skp") != nullptr) {
        SkFILEStream stream(in_file);
        pic = SkPicture::MakeFromStream(&stream);
        if (!pic) {
            printf("Failed to load SkPicture from %s!\n", in_file);
            exit(1);
        }

        if (should_rebuild_pic) {
            printf("Warning: -r ignored when loading SkPicture\n");
            should_rebuild_pic = false;
        }
    } else {
        yaml_doc = loadYAMLFile(in_file);
    }

#if 0
    SkFILEWStream stream("out.skp");
    pic->serialize(&stream);
#endif

    std::cout << "Rendering..." << std::endl;

    GLFWwindow* window;
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        exit(EXIT_FAILURE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    window = glfwCreateWindow(gWidth, gHeight, "Skia viewer", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwSetKeyCallback(window, key_callback);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    sk_sp<const GrGLInterface> fBackendContext;

    sk_sp<const GrGLInterface> glInterface;
    glInterface.reset(GrGLCreateNativeInterface());
    fBackendContext.reset(GrGLInterfaceRemoveNVPR(glInterface.get()));

    //SkASSERT(nullptr == fContext);
    auto fContext = GrContext::Create(kOpenGL_GrBackend, (GrBackendContext)fBackendContext.get());

    sk_sp<SkSurface> fSurface;
    if (nullptr == fSurface) {
        auto fActualColorBits =  24;

        if (fContext) {
            GrBackendRenderTargetDesc desc;
            desc.fWidth = gWidth;
            desc.fHeight = gHeight;
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

    using FpMilliseconds = 
        std::chrono::duration<double, std::chrono::milliseconds::period>;

    auto before = std::chrono::high_resolution_clock::now();
    auto first = std::chrono::high_resolution_clock::now();

    auto min_frame = FpMilliseconds::max();
    auto min_min_frame = FpMilliseconds::max();
    auto max_frame = FpMilliseconds::min();
    auto max_max_frame = FpMilliseconds::min();
    auto sum_frame = FpMilliseconds::zero();

    // I really would like some of binning of series data here
    std::vector<double> block_avg_ms;

    uint32_t frame = 0;
    bool warmed_up = false;

    SkMatrix scaleMatrix = SkMatrix::MakeScale(SkDoubleToScalar(scale));

    while (!glfwWindowShouldClose(window))
    {
        if (!pic || should_rebuild_pic) {
            SkPictureRecorder recorder;
            SkCanvas* skp_canvas = recorder.beginRecording(gWidth, gHeight, nullptr, 0);

            skp_canvas->clear(SK_ColorRED);

            drawYAMLFile(yaml_doc, skp_canvas);

            pic = recorder.finishRecordingAsPicture();
        }

        canvas->clear(SK_ColorWHITE);
        canvas->drawPicture(pic, &scaleMatrix, nullptr);
        canvas->flush();
        glfwSwapBuffers(window);
        glfwPollEvents();

        warmed_up = frame > frames_between_dumps;
        auto after = std::chrono::high_resolution_clock::now();
        auto dur = after - before;

        min_frame = std::min(min_frame, FpMilliseconds(dur));
        max_frame = std::max(max_frame, FpMilliseconds(dur));
        sum_frame += dur;

        // only count globals for warmed up frames
        if (warmed_up) {
            min_min_frame = std::min(min_min_frame, FpMilliseconds(dur));
            max_max_frame = std::max(max_max_frame, FpMilliseconds(dur));
        }

        if ((++frame % frames_between_dumps) == 0) {
            double ms = (sum_frame / frames_between_dumps).count();
            printf("%3.3f [%3.3f .. %3.3f]  -- %4.2f fps",
                   ms, min_frame.count(), max_frame.count(), 1000.0 / ms);
            if (warmed_up) {
                printf("  -- (global %3.3f .. %3.3f)\n", min_min_frame.count(), max_max_frame.count());
                block_avg_ms.push_back(ms);
            } else {
                printf("\n");
            }

            min_frame = FpMilliseconds::max();
            max_frame = FpMilliseconds::min();
            sum_frame = FpMilliseconds::zero();
        }

        //printf("%f\n", FpMilliseconds(after - before).count());
        before = after;

        if ((after-first) > std::chrono::seconds(exit_after_seconds)) {
            std::sort(block_avg_ms.begin(), block_avg_ms.end());
            double sum = std::accumulate(block_avg_ms.begin(), block_avg_ms.end(), 0.0);

            size_t len = block_avg_ms.size();
            size_t first_index = std::floor(len * 0.1);
            size_t last_index = std::floor(len * 0.9);

            double val_10th_pct = (block_avg_ms[first_index] + block_avg_ms[first_index+1]) / 2.0;
            double val_90th_pct = (block_avg_ms[last_index] + block_avg_ms[last_index+1]) / 2.0;

            double average_ms = sum / double(len);

#define F(x) (1000.0 / (x))

            if (false) {
                printf("-     % 8s  % 8s  % 8s  % 8s  % 8s\n", "min", "90th", "avg", "10th", "max");
                printf("ms:   % 8.3f  % 8.3f  % 8.3f  % 8.3f  % 8.3f\n",
                       block_avg_ms.front(), val_10th_pct, average_ms, val_90th_pct, block_avg_ms.back());
                printf("fps:  % 8.3f  % 8.3f  % 8.3f  % 8.3f  % 8.3f\n",
                       F(block_avg_ms.front()), F(val_10th_pct), F(average_ms), F(val_90th_pct), F(block_avg_ms.back()));
            } else {
                printf("-     % 8s  % 8s  % 8s\n", "90th", "avg", "10th");
                printf("ms:   % 8.3f  % 8.3f  % 8.3f\n",
                       val_10th_pct, average_ms, val_90th_pct);
                printf("fps:  % 8.3f  % 8.3f  % 8.3f\n",
                       F(val_10th_pct), F(average_ms), F(val_90th_pct));
            }

            exit(0);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}

