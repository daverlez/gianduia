#include <stb_image.h>

#include <gianduia/core/factory.h>
#include "gianduia/core/fileResolver.h"
#include "gianduia/textures/texture.h"

namespace gnd {
    class ImageTexture : public Texture<Color3f> {
    public:
        ImageTexture(const PropertyList &props) {
            m_filename = props.getString("filename");
            auto absPath = FileResolver::resolve(m_filename);

            stbi_set_flip_vertically_on_load(true);

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

            m_data = stbi_load(absPath.c_str(), &m_width, &m_height, &m_channels, 3);
            if (m_data == NULL)
                throw std::runtime_error("Failed to load image texture " + m_filename);
        }

        ~ImageTexture() {
            if (m_data)
                stbi_image_free(m_data);
        }

        virtual Color3f evaluate(const SurfaceInteraction& isect) const override {
            Color3f result;
            if (m_interpolation == EInterpolationMode::EClosest)
                result = sampleClosest(isect.uv);
            else
                result = sampleLinear(isect.uv);

            return m_colorSpace == EColorSpace::EsRGB ? result.toLinear() : result;
        }

        virtual std::string toString() const override {
            return std::format(
            "ImageTexture[\n"
                    "  file = {},\n"
                    "  width = {},\n"
                    "  height = {},\n"
                    "  channels = {},\n"
                    "  interpolation = {},\n"
                    "  color space = {},\n"
                    "  wrap mode = {}\n"
                    "]",
                m_filename,
                m_width,
                m_height,
                m_channels,
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
        int m_width, m_height, m_channels;
        unsigned char *m_data;
        EInterpolationMode m_interpolation;
        EColorSpace m_colorSpace;
        ERepeatMode m_repeat;

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
            float u = local.x();
            float v = local.y();

            int x = (int)std::floor(u * m_width);
            if (x > m_width - 1) x = m_width - 1;
            int y = (int)std::floor(v * m_height);
            if (y > m_height - 1) y = m_height - 1;

            int pixelIndex = (y * m_width + x) * m_channels;

            float r = m_data[pixelIndex] / 255.0f;
            float g = m_data[pixelIndex + 1] / 255.0f;
            float b = m_data[pixelIndex + 2] / 255.0f;

            return Color3f(r, g, b);
        }

        Color3f sampleLinear(const Point2f & uv) const {
            Point2f local = getUV(uv);
            float u = local.x();
            float v = local.y();

            float x_coord = u * m_width - 0.5f;
            float y_coord = v * m_height - 0.5f;

            int x0 = (int)std::floor(x_coord);
            int y0 = (int)std::floor(y_coord);

            // Interpolation weights
            float tx = x_coord - (float)x0;
            float ty = y_coord - (float)y0;

            auto getPixelColor = [this](int x, int y) -> Color3f {
                if (m_repeat == ERepeatMode::ERepeat) {
                    x = ((x % m_width) + m_width) % m_width;
                    y = ((y % m_height) + m_height) % m_height;
                } else {
                    x = std::max(0, std::min(x, m_width - 1));
                    y = std::max(0, std::min(y, m_height - 1));
                }

                int pixelIndex = (y * m_width + x) * m_channels;
                float r = m_data[pixelIndex] / 255.0f;
                float g = m_data[pixelIndex + 1] / 255.0f;
                float b = m_data[pixelIndex + 2] / 255.0f;
                return Color3f(r, g, b);
            };

            // Sampling the four pixel colors
            Color3f c00 = getPixelColor(x0, y0);
            Color3f c10 = getPixelColor(x0 + 1, y0);
            Color3f c01 = getPixelColor(x0, y0 + 1);
            Color3f c11 = getPixelColor(x0 + 1, y0 + 1);

            // Bilinear interpolation on both dimensions
            Color3f c_top = c00 * (1.0f - tx) + c10 * tx;
            Color3f c_bottom = c01 * (1.0f - tx) + c11 * tx;

            return c_top * (1.0f - ty) + c_bottom * ty;
        }
    };

    GND_REGISTER_CLASS(ImageTexture, "image");
}
