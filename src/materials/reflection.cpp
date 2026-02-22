#include <gianduia/materials/reflection.h>
#include <gianduia/math/warp.h>

namespace gnd {

    Color3f LambertianReflection::f(const Vector3f& wo, const Vector3f& wi) const {
        return m_albedo * M_1_PI;
    }

    Color3f LambertianReflection::sample(const Vector3f &wo, Vector3f &wi,
                                            const Point2f &sample, float &pdf,
                                            BxDFType *sampledType) const {
        if (wo.z() <= 0.0f) {
            pdf = 0.0f;
            return Color3f(0.0f);
        }

        wi = Warp::squareToCosineHemisphere(sample);
        pdf = Warp::squareToCosineHemispherePdf(wi);

        if (sampledType) *sampledType = type;

        return f(wo, wi);
    }

    float LambertianReflection::pdf(const Vector3f& wo, const Vector3f& wi) const {
        return Warp::squareToCosineHemispherePdf(wi);
    }


}
