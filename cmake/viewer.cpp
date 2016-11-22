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

// These headers are just handy for writing this example file.  Nothing Skia specific.
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <assert.h>

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

#include "yaml-cpp/yaml.h"

using namespace std;

// SkColor is typdef to unsigned int so we wrap
// it in a new type
struct SkColorW
{
    SkColor color;
};

namespace YAML {

template<>
struct convert<SkColorW> {
    static bool decode(const Node& node, SkColorW& rhs) {
        if (node.IsScalar()) {
            auto val = node.as<string>();
            if (val == "red") {
                rhs.color = SK_ColorRED;
                return true;
            }
            if (val == "green") {
                rhs.color = SK_ColorGREEN;
                return true;
            }
            if (val == "blue") {
                rhs.color = SK_ColorBLUE;
                return true;
            }


            auto vec = node.as<vector<double>>();
            if (vec.size() == 4) {
                SkColor4f color;
                color.fR = vec[0] / 255.;
                color.fG = vec[1] / 255.;
                color.fB = vec[2] / 255.;
                color.fA = vec[3];
                rhs.color = color.toSkColor();
                return true;
            } else if (vec.size() == 4) {
                SkColor4f color;
                color.fR = vec[0] / 255.;
                color.fG = vec[1] / 255.;
                color.fB = vec[2] / 255.;
                color.fA = 1.;
                rhs.color = color.toSkColor();
                return true;
            }
        }
        return false;
    }
};

template<>
struct convert<vector<double>> {
    static bool decode(const Node& node, vector<double>& rhs) {
        if (node.IsScalar()) {
            stringstream ss(node.as<string>());
            double token;
            vector<double> vec;
            while (ss >> token) {
                vec.push_back(token);
            }
            rhs = vec;
            return true;
        }
        return false;
    }
};

template<>
struct convert<SkRect> {
    static bool decode(const Node& node, SkRect& rhs) {
        if (node.IsScalar()) {
            auto vec = node.as<vector<double>>();
            std::cout << "is rect";
            rhs = SkRect::MakeXYWH(vec[0],
                                   vec[1],
                                   vec[2],
                                   vec[3]);
            return true;
        }
        return false;
    }
};

}

void drawText(SkCanvas *c, YAML::Node &item) {
    auto origin = item["origin"].as<vector<double>>();
    SkPaint paint;
    if (item["color"]) {
        auto color = item["color"].as<SkColorW>().color;
        paint.setColor(color);
    }
    if (item["size"]) {
        paint.setTextSize(item["size"].as<double>());
        cout << item["size"].as<double>();
    }

    auto text = item["text"].as<string>();
    paint.setAntiAlias(true);
    c->drawText(text.c_str(), strlen(text.c_str()),
                origin[0],
                origin[1],
                paint);

}
void drawRect(SkCanvas *c, YAML::Node &item) {
    // XXX: handle bounds
    SkRect bounds;
    if (item["rect"])
            bounds = item["rect"].as<SkRect>();
    else
            bounds = item["bounds"].as<SkRect>();
    SkPaint paint;
    if (item["color"]) {
        auto color = item["color"].as<SkColorW>().color;
        paint.setColor(color);
    }
    c->drawRect(bounds, paint);

}

void drawGlyphs(SkCanvas *c, YAML::Node &item) {
    // XXX: handle bounds
    vector<uint16_t> indices;
    vector<SkPoint> offsets;
    for (auto i : item["glyphs"]) {
            indices.push_back(i.as<uint16_t>());
    }
    for (int i = 0; i < item["offsets"].size(); i+= 2) {
            SkPoint p;
            p.fX = item["offsets"][i].as<double>();
            p.fY = item["offsets"][i+1].as<double>();
            offsets.push_back(p);
    }

    SkPaint paint;
    if (item["size"]) {
        paint.setTextSize(item["size"].as<double>());
        cout << item["size"].as<double>();
    }

    SkFontStyle::Weight weight = SkFontStyle::kNormal_Weight;
    if (item["weight"]) {
            weight = (SkFontStyle::Weight)item["weight"].as<int>();
    }

    if (item["family"]) {
        sk_sp<SkTypeface> typeface = SkTypeface::MakeFromName(item["family"].as<string>().c_str(), SkFontStyle(weight,
                                                                                                               SkFontStyle::kNormal_Width,
                                                                                                               SkFontStyle::kUpright_Slant));
        paint.setTypeface(typeface);
    }

    if (item["color"]) {
        auto color = item["color"].as<SkColorW>().color;
        paint.setColor(color);
    }
    
    if (item["color"]) {
        auto color = item["color"].as<SkColorW>().color;
        paint.setColor(color);
    }

    assert(indices.size() == offsets.size());
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    paint.setAntiAlias(true);
    c->drawPosText(indices.data(), indices.size()*2, offsets.data(), paint);

}

void drawImage(SkCanvas *c, YAML::Node &node) {
    auto path = node["image"].as<string>();
    auto bounds = node["bounds"].as<vector<double>>();
    sk_sp<SkImage> img = GetResourceAsImage(path.c_str());
    c->drawImage(img, bounds[0], bounds[1]);
}
void drawItem(SkCanvas *c, YAML::Node &node);
void drawStackingContext(SkCanvas *c, YAML::Node &node) {
    auto bounds = node["bounds"].as<vector<double>>();
    c->save();
    c->translate(bounds[0], bounds[1]);
    for (auto i : node["items"]) {
        drawItem(c, i);
    }
    c->restore();
}


void drawItem(SkCanvas *c, YAML::Node &node) {
    std::cout << "item\n";
    if (node["text"]) {
        drawText(c, node);
    } else if (node["rect"]) {
        drawRect(c, node);
    } else if (node["image"]) {
        drawImage(c, node);
    } else if (node["glyphs"]) {
        drawGlyphs(c, node);
    } else if (node["stacking_context"]) {
    } else if (node["type"]) {
            auto type = node["type"].as<string>();
            if (type == "stacking_context") {
                    drawStackingContext(c, node);
            } else if (type == "rect") {
                    drawRect(c, node);
            }
    }

}
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
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
	desc.fRenderTargetHandle = buffer;
    SkSurfaceProps    fSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType);

	fSurface = SkSurface::MakeFromBackendRenderTarget(fContext, desc, &fSurfaceProps);
	
    }
}

    glClearColor(0.0f,1.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    SkCanvas* canvas = fSurface->getCanvas();   // We don't manage this pointer's lifetime.

    timespec last;
    clock_gettime(CLOCK_MONOTONIC, &last);

    while (1) {
    gPic->playback(canvas);
    canvas->flush();


    // Black background
    //Draw i
    glFlush();
    timespec res;
    clock_gettime(CLOCK_MONOTONIC, &res);
    double before = last.tv_sec*1000*1000*1000. + last.tv_nsec;
    double after = res.tv_sec*1000*1000*1000. + res.tv_nsec;
    printf("%f\n", (after-before)/(1000*1000));
    last = res;
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

    std::ifstream fin("frame-1155.yaml");

    YAML::Node doc = YAML::Load(fin);
    for (auto i : doc["root"]["items"]) {
        drawItem(canvas, i);
    }

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

