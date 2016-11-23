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
            if (val == "white") {
                rhs.color = SK_ColorWHITE;
                return true;
            }
            if (val == "black") {
                rhs.color = SK_ColorBLACK;
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
            } else if (vec.size() == 3) {
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
        paint.setTextSize(item["size"].as<double>() * 16. / 12.);
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
        paint.setTextSize(item["size"].as<double>() * 16. / 12.);
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


void drawYAMLFile(SkCanvas *canvas, const char *file) {
    std::ifstream fin(file);

    YAML::Node doc = YAML::Load(fin);
    for (auto i : doc["root"]["items"]) {
        drawItem(canvas, i);
    }
}

