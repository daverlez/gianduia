#include <stb_image.h>

#include <gianduia/core/factory.h>
#include <gianduia/core/fileResolver.h>
#include <gianduia/textures/texture.h>
#include <gianduia/textures/textureCache.h>

namespace gnd {
    class ImageTexture : public Texture<Color3f> {
    public:
        ImageTexture(const PropertyList &props) {
            m_filename = props.getString("filename");
            auto absPath = FileResolver::resolve(m_filename);

            // Reading wrap mode (either "closest" or "linear")
            std::string wrapMode = props.getString("interpolation_mode", "closest");
            if (wrapMode == "closest")
                m_interpolation = EInterpolationMode::EClosest;
            else if (wrapMode == "linear")
                m_interpolation = EInterpolationMode::ELinear;
            else
                throw std::runtime_error("ImageTexture: Unknown interpolation mode " + wrapMode);

            // Reading color space (either "sRGB" or "linear").
            std::string colorSpace = props.getString("color_space", "linear");
            if (colorSpace == "sRGB")
                m_colorSpace = EColorSpace::EsRGB;
            else if (colorSpace == "linear")
                m_colorSpace = EColorSpace::ELinear;
            else
                throw std::runtime_error("ImageTexture: Unknown color space " + colorSpace);

            std::string repeatMode = props.getString("repeat_mode", "repeat");
            if (repeatMode == "repeat")
                m_repeat = ERepeatMode::ERepeat;
            else if (repeatMode == "clamp")
                m_repeat = ERepeatMode::EClamp;
            else
                throw std::runtime_error("ImageTexture: Unknown repeat mode " + repeatMode);

            bool isSRGB = (m_colorSpace == EColorSpace::EsRGB);
            m_imageData = TextureCache::load(absPath.c_str(), isSRGB);
        }

        virtual Color3f evaluate(const SurfaceInteraction& isect) const override {
            if (m_interpolation == EInterpolationMode::EClosest)
                return sampleClosest(isect.uv);
            else
                return sampleLinear(isect.uv);
        }

        virtual std::string toString() const override {
            return std::format(
            "ImageTexture[\n"
                    "  file = {},\n"
                    "  width = {},\n"
                    "  height = {},\n"
                    "  interpolation = {},\n"
                    "  color space = {},\n"
                    "  wrap mode = {}\n"
                    "]",
                m_filename,
                m_imageData->width,
                m_imageData->height,
                m_interpolation == EInterpolationMode::EClosest ? "closest" : "linear",
                m_colorSpace == EColorSpace::EsRGB ? "sRGB" : "linear",
                m_repeat == ERepeatMode::ERepeat ? "repeat" : "clamp"
            );
        }

    protected:
        enum class EInterpolationMode{
            ELinear,
            EClosest
        };

        enum class EColorSpace {
            EsRGB,
            ELinear
        };

        enum class ERepeatMode {
            EClamp,
            ERepeat
        };

        std::string m_filename;
        EInterpolationMode m_interpolation;
        EColorSpace m_colorSpace;
        ERepeatMode m_repeat;
        std::shared_ptr<ImageData> m_imageData;

    private:
        Point2f getUV(const Point2f & uv) const {
            if (m_repeat == ERepeatMode::EClamp)
                return Point2f(
                    std::clamp(uv.x(), 0.0f, 1.0f),
                    std::clamp(uv.y(), 0.0f, 1.0f)
                );
            return Point2f(
                uv.x() - std::floor(uv.x()),
                uv.y() - std::floor(uv.y())
            );
        }

        Color3f sampleClosest(const Point2f & uv) const {
            Point2f local = getUV(uv);

            int w = m_imageData->width;
            int h = m_imageData->height;

            int x = std::min((int)std::floor(local.x() * w), w - 1);
            int y = std::min((int)std::floor(local.y() * h), h - 1);

            return m_imageData->pixels[y * w + x];
        }

        Color3f sampleLinear(const Point2f & uv) const {
            Point2f local = getUV(uv);

            int w = m_imageData->width;
            int h = m_imageData->height;

            float x_coord = local.x() * w - 0.5f;
            float y_coord = local.y() * h - 0.5f;

            int x0 = (int)std::floor(x_coord);
            int y0 = (int)std::floor(y_coord);

            float tx = x_coord - (float)x0;
            float ty = y_coord - (float)y0;

            auto getPixelColor = [this, w, h](int x, int y) -> Color3f {
                if (m_repeat == ERepeatMode::ERepeat) {
                    x = ((x % w) + w) % w;
                    y = ((y % h) + h) % h;
                } else {
                    x = std::max(0, std::min(x, w - 1));
                    y = std::max(0, std::min(y, h - 1));
                }
                return m_imageData->pixels[y * w + x];
            };

            Color3f c00 = getPixelColor(x0, y0);
            Color3f c10 = getPixelColor(x0 + 1, y0);
            Color3f c01 = getPixelColor(x0, y0 + 1);
            Color3f c11 = getPixelColor(x0 + 1, y0 + 1);

            Color3f c_top = c00 * (1.0f - tx) + c10 * tx;
            Color3f c_bottom = c01 * (1.0f - tx) + c11 * tx;

            return c_top * (1.0f - ty) + c_bottom * ty;
        }
    };

    GND_REGISTER_CLASS(ImageTexture, "image");
}
