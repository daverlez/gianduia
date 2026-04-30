#include "gianduia/textures/imageTexture.h"
#include "gianduia/core/factory.h"
#include "gianduia/core/fileResolver.h"

#include <algorithm>
#include <format>
#include <stdexcept>

namespace gnd {
    template<ValidTextureValue T>
    ImageTexture<T>::ImageTexture(const PropertyList &props) {
        m_filename = props.getString("filename");
        auto absPath = FileResolver::resolve(m_filename);

        std::string wrapMode = props.getString("interpolation_mode", "closest");
        if (wrapMode == "closest") m_interpolation = EInterpolationMode::EClosest;
        else if (wrapMode == "linear") m_interpolation = EInterpolationMode::ELinear;
        else throw std::runtime_error("ImageTexture: Unknown interpolation mode " + wrapMode);

        std::string colorSpace = props.getString("color_space", "linear");
        if (colorSpace == "sRGB") m_colorSpace = EColorSpace::EsRGB;
        else if (colorSpace == "linear") m_colorSpace = EColorSpace::ELinear;
        else throw std::runtime_error("ImageTexture: Unknown color space " + colorSpace);

        std::string repeatMode = props.getString("repeat_mode", "repeat");
        if (repeatMode == "repeat") m_repeat = ERepeatMode::ERepeat;
        else if (repeatMode == "clamp") m_repeat = ERepeatMode::EClamp;
        else throw std::runtime_error("ImageTexture: Unknown repeat mode " + repeatMode);

        bool isSRGB = (m_colorSpace == EColorSpace::EsRGB);
        m_imageData = TextureCache::load(absPath.string(), isSRGB);
    }

    template<ValidTextureValue T>
    T ImageTexture<T>::evaluate(const SurfaceInteraction &isect) const {
        if (m_interpolation == EInterpolationMode::EClosest)
            return sampleClosest(isect.uv);

        return sampleLinear(isect.uv);
    }

    template<ValidTextureValue T>
    std::string ImageTexture<T>::toString() const {
        std::string instantiation;
        if constexpr (std::is_same_v<T, float>) instantiation = "float";
        else if constexpr (std::is_same_v<T, Color3f>) instantiation = "color";
        else if constexpr (std::is_same_v<T, Normal3f>) instantiation = "normal";

        return std::format(
            "ImageTexture({})[\n"
            "  file = {},\n"
            "  width = {},\n"
            "  height = {},\n"
            "  channels = {},\n"
            "  interpolation = {},\n"
            "  color space = {},\n"
            "  wrap mode = {}\n"
            "]",
            instantiation,
            m_filename,
            m_imageData->width,
            m_imageData->height,
            m_imageData->channels,
            m_interpolation == EInterpolationMode::EClosest ? "closest" : "linear",
            m_colorSpace == EColorSpace::EsRGB ? "sRGB" : "linear",
            m_repeat == ERepeatMode::ERepeat ? "repeat" : "clamp"
        );
    }

    template<ValidTextureValue T>
    Point2f ImageTexture<T>::getUV(const Point2f &uv) const {
        if (m_repeat == ERepeatMode::EClamp)
            return Point2f(std::clamp(uv.x(), 0.0f, 1.0f), std::clamp(uv.y(), 0.0f, 1.0f));
        return Point2f(uv.x() - std::floor(uv.x()), uv.y() - std::floor(uv.y()));
    }

    template<ValidTextureValue T>
    T ImageTexture<T>::extractValue(int x, int y) const {
        int w = m_imageData->width;
        int h = m_imageData->height;
        int c = m_imageData->channels;

        if (m_repeat == ERepeatMode::ERepeat) {
            x = ((x % w) + w) % w;
            y = ((y % h) + h) % h;
        } else {
            x = std::clamp(x, 0, w - 1);
            y = std::clamp(y, 0, h - 1);
        }

        int idx = (y * w + x) * c;
        const float *ptr = &m_imageData->data[idx];

        if constexpr (std::is_same_v<T, float>) {
            return ptr[0];
        } else if constexpr (std::is_same_v<T, Color3f>) {
            if (c >= 3) return Color3f(ptr[0], ptr[1], ptr[2]);
            return Color3f(ptr[0]);
        } else if constexpr (std::is_same_v<T, Normal3f>) {
            if (c >= 3) return Normal3f(ptr[0] * 2.0f - 1.0f, ptr[1] * 2.0f - 1.0f, ptr[2] * 2.0f - 1.0f);
            return Normal3f(0.0f, 0.0f, 1.0f);
        }
    }

    template<ValidTextureValue T>
    T ImageTexture<T>::sampleClosest(const Point2f &uv) const {
        Point2f local = getUV(uv);
        int x = (int) std::floor(local.x() * m_imageData->width);
        int y = (int) std::floor(local.y() * m_imageData->height);
        return extractValue(x, y);
    }

    template<ValidTextureValue T>
    T ImageTexture<T>::sampleLinear(const Point2f &uv) const {
        Point2f local = getUV(uv);
        int w = m_imageData->width;
        int h = m_imageData->height;

        float x_coord = local.x() * w - 0.5f;
        float y_coord = local.y() * h - 0.5f;

        int x0 = (int) std::floor(x_coord);
        int y0 = (int) std::floor(y_coord);

        float tx = x_coord - (float) x0;
        float ty = y_coord - (float) y0;

        T c00 = extractValue(x0, y0);
        T c10 = extractValue(x0 + 1, y0);
        T c01 = extractValue(x0, y0 + 1);
        T c11 = extractValue(x0 + 1, y0 + 1);

        T c_top = c00 * (1.0f - tx) + c10 * tx;
        T c_bottom = c01 * (1.0f - tx) + c11 * tx;

        return c_top * (1.0f - ty) + c_bottom * ty;
    }

    template class ImageTexture<float>;
    template class ImageTexture<Color3f>;
    template class ImageTexture<Normal3f>;

    using ColorImageTexture  = ImageTexture<Color3f>;
    using FloatImageTexture  = ImageTexture<float>;
    using NormalImageTexture = ImageTexture<Normal3f>;

    GND_REGISTER_CLASS(ColorImageTexture,  "image_color");
    GND_REGISTER_CLASS(FloatImageTexture,  "image_float");
    GND_REGISTER_CLASS(NormalImageTexture, "image_normal");
}