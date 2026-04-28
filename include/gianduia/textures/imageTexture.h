#pragma once

#include <gianduia/textures/texture.h>
#include <gianduia/textures/textureCache.h>
#include <memory>
#include <string>

namespace gnd {

    template <ValidTextureValue T>
    class ImageTexture : public Texture<T> {
    public:
        ImageTexture(const PropertyList &props);

        virtual T evaluate(const SurfaceInteraction& isect) const override;
        virtual std::string toString() const override;

    protected:
        enum class EInterpolationMode { ELinear, EClosest };
        enum class EColorSpace { EsRGB, ELinear };
        enum class ERepeatMode { EClamp, ERepeat };

        std::string m_filename;
        EInterpolationMode m_interpolation;
        EColorSpace m_colorSpace;
        ERepeatMode m_repeat;
        std::shared_ptr<ImageData> m_imageData;

    private:
        Point2f getUV(const Point2f & uv) const;

        T extractValue(int x, int y) const;

        T sampleClosest(const Point2f & uv) const;
        T sampleLinear(const Point2f & uv) const;
    };

} // namespace gnd