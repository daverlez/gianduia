#include <gianduia/materials/transmission.h>"
#include <gianduia/materials/fresnel.h>

namespace gnd {

   Color3f SpecularTransmission::f(const Vector3f &wo, const Vector3f &wi) const {
      return 0.0f;   // Delta material
   }

   Color3f SpecularTransmission::sample(const Vector3f& wo, Vector3f& wi,
                        const Point2f& sample, float& pdf,
                                    BxDFType* sampledType = nullptr) const {
       bool entering = wo.z() > 0;

       float etaI = entering ? m_etaExt : m_etaInt;     // eta from the incident side
       float etaT = entering ? m_etaInt : m_etaExt;     // eta from the transmitted side

       Normal3f n(0, 0, 1);
       if (!entering) n = Normal3f(0, 0, -1); // Flipping the normal if coming from inside the dielectric

       float etaRatio = etaI / etaT;
       Vector3f wi_refracted;
       if (!Refract(wo, n, etaRatio, &wi_refracted)) {
           return Color3f(0.f);
       }

       wi = wi_refracted;
       pdf = 1.f;
       Color3f ft = m_T * (1.f - FrDielectric(wo.z(), m_etaExt, m_etaInt));

       if (sampledType) *sampledType = type;

       ft *= (etaRatio * etaRatio);
       return ft;
   }

}
