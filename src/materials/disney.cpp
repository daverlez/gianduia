#include "gianduia/materials/disney.h"
#include "gianduia/core/factory.h"
#include "gianduia/materials/reflection.h"

namespace gnd {

    // Diffuse lobe

    Color3f DisneyDiffuseBxDF::f(const Vector3f &wo, const Vector3f &wi) const {
        if (wo.z() * wi.z() <= 0.0f) return Color3f(0.0f);

        float cosThetaO = Frame::cosTheta(wo);
        float cosThetaI = Frame::cosTheta(wi);

        float Fo = SchlickWeight(cosThetaO);
        float Fi = SchlickWeight(cosThetaI);

        Vector3f wh = Normalize(wo + wi);
        float cosThetaD = Dot(wi, wh);

        // Base diffuse
        float FD90 = 0.5f + 2.0f * m_roughness * cosThetaD * cosThetaD;
        float fDiffuse = (1.0f + (FD90 - 1.0f) * Fo) * (1.0f + (FD90 - 1.0f) * Fi);

        // Subsurface scattering approximation (Hanrahan - Krueger)
        float FSS90 = m_roughness * cosThetaD * cosThetaD;
        float Fss = (1.0f + (FSS90 - 1.0f) * Fi) * (1.0f + (FSS90 - 1.0f) * Fo);
        float fSubsurface = 1.25f * (Fss * (1.0f / (cosThetaO + cosThetaI) - 0.5f) + 0.5f);

        return InvPi * m_R * Lerp(m_subsurface, fDiffuse, fSubsurface);
    }

    float DisneyDiffuseBxDF::pdf(const Vector3f& wo, const Vector3f& wi) const {
        if (wo.z() * wi.z() <= 0.0f) return 0.0f;
        return Warp::squareToCosineHemispherePdf(wi);
    }

    Color3f DisneyDiffuseBxDF::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample,
                                        float uc, float& pdf, BxDFType* sampledType) const {
        if (wo.z() <= 0.0f) {
            pdf = 0.0f;
            return Color3f(0.0f);
        }

        wi = Warp::squareToCosineHemisphere(sample);
        pdf = Warp::squareToCosineHemispherePdf(wi);

        if (sampledType) *sampledType = type;

        return f(wo, wi);
    }


    // Sheen lobe

    Color3f DisneySheenBxDF::f(const Vector3f& wo, const Vector3f& wi) const {
        if (wo.z() * wi.z() <= 0.0f) return Color3f(0.0f);

        Vector3f wh = Normalize(wo + wi);
        float cosThetaD = Dot(wi, wh);

        float lum = m_R.luminance();
        Color3f tintColor = lum > 0.0f ? m_R / lum : Color3f(1.0f);

        return m_weight * Lerp(m_tint, Color3f(1.0f), tintColor) * SchlickWeight(cosThetaD);
    }

    float DisneySheenBxDF::pdf(const Vector3f& wo, const Vector3f& wi) const {
        // Not registered for sampling in the overall model
        return 0.0f;
    }

    Color3f DisneySheenBxDF::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample,
                                        float uc, float& pdf, BxDFType* sampledType) const {
        // Not registered for sampling in the overall model
        pdf = 0.0f;
        return Color3f(0.0f);
    }


    // Clearcoat lobe

    Color3f DisneyClearcoatBxDF::f(const Vector3f& wo, const Vector3f& wi) const {
        if (wo.z() * wi.z() <= 0.0f) return Color3f(0.0f);

        Vector3f wh = Normalize(wo + wi);
        float cosThetaD = Dot(wi, wh);

        float D = m_distrib->D(wh);
        float F = FrSchlick(0.04f, cosThetaD);
        float G = m_distrib->G(wo, wi);

        float den = 4.0f * std::abs(wo.z() * wi.z());
        if (den < Epsilon) return Color3f(0.0f);

        return Color3f(m_weight * D * F * G / den);
    }

    float DisneyClearcoatBxDF::pdf(const Vector3f& wo, const Vector3f& wi) const {
        if (wo.z() * wi.z() <= 0.0f) return 0.0f;

        Vector3f wh = Normalize(wo + wi);
        float dotOH = Dot(wo, wh);
        if (dotOH <= 0.0f) return 0.0f;

        float invDen = 1.0f / (4.0f * dotOH);
        return m_distrib->pdf(wo, wh) * invDen;
    }

    Color3f DisneyClearcoatBxDF::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample,
                                        float uc, float& pdf, BxDFType* sampledType) const {
        if (std::abs(wo.z()) < Epsilon) { pdf = 0.0f; return Color3f(0.0f); }

        Vector3f wh = m_distrib->sample_wh(wo, sample);
        float dotOH = Dot(wo, wh);
        if (dotOH <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }

        wi = -wo + wh * 2.0f * dotOH;
        if (wo.z() * wi.z() <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }

        pdf = this->pdf(wo, wi);
        if (sampledType) *sampledType = type;

        return f(wo, wi);
    }


    // Specular transmission lobe

    Color3f DisneyTransmissionBxDF::f(const Vector3f& wo, const Vector3f& wi) const {
        if (wo.z() * wi.z() >= 0.0f) return Color3f(0.0f);

        bool entering = wo.z() > 0.0f;
        float etaI = entering ? m_etaExt : m_etaInt;
        float etaT = entering ? m_etaInt : m_etaExt;

        Vector3f wh = Normalize(wo * etaI + wi * etaT);
        if (wh.z() * wo.z() < 0.0f) wh = -wh;

        float dotO = Dot(wo, wh);
        float dotI = Dot(wi, wh);

        if (dotO * dotI > 0.0f) return Color3f(0.0f);

        float F = FrDielectric(dotO, etaI, etaT);
        float D = m_distrib->D(wh);
        float G = m_distrib->G(wo, wi);

        float denom = (etaI * dotO + etaT * dotI);
        denom *= denom;

        float factor = std::abs(dotO * dotI) / std::abs(wo.z() * wi.z());
        return m_T * factor * (etaI * etaI) * D * G * (1.0f - F) / denom;
    }

    float DisneyTransmissionBxDF::pdf(const Vector3f& wo, const Vector3f& wi) const {
        if (wo.z() * wi.z() >= 0.0f) return 0.0f;

        bool entering = wo.z() > 0.0f;
        float etaI = entering ? m_etaExt : m_etaInt;
        float etaT = entering ? m_etaInt : m_etaExt;

        Vector3f wh = Normalize(wo * etaI + wi * etaT);
        if (wh.z() * wo.z() < 0.0f) wh = -wh;

        float dotO = Dot(wo, wh);
        float dotI = Dot(wi, wh);
        if (dotO * dotI > 0.0f) return 0.0f;

        float F = FrDielectric(dotO, etaI, etaT);
        float pdf_wh = m_distrib->pdf(wo, wh);

        float denom = (etaI * dotO + etaT * dotI);
        float dwh_dwi = (etaT * etaT * std::abs(dotI)) / (denom * denom);

        return (1.0f - F) * pdf_wh * dwh_dwi;
    }

    Color3f DisneyTransmissionBxDF::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample, float uc, float& pdf, BxDFType* sampledType) const {
        if (std::abs(wo.z()) < Epsilon) { pdf = 0.0f; return Color3f(0.0f); }

        Vector3f wh = m_distrib->sample_wh(wo, sample);
        float dotO = Dot(wo, wh);
        if (dotO <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }

        bool entering = wo.z() > 0.0f;
        float etaI = entering ? m_etaExt : m_etaInt;
        float etaT = entering ? m_etaInt : m_etaExt;

        Normal3f n_wh(wh.x(), wh.y(), wh.z());

        if (!Refract(wo, n_wh, etaI / etaT, &wi)) { pdf = 0.0f; return Color3f(0.0f); }
        if (wo.z() * wi.z() > 0.0f) { pdf = 0.0f; return Color3f(0.0f); }

        if (sampledType) *sampledType = type;
        pdf = this->pdf(wo, wi);

        return f(wo, wi);
    }


    class DisneyMaterial : public Material {
    public:
        DisneyMaterial(const PropertyList& props) : Material(props) {
            auto loadColor = [&](const std::string& name, std::shared_ptr<Texture<Color3f>>& tex) {
                if (props.hasColor(name)) {
                    PropertyList p;
                    p.setColor("value", props.getColor(name));
                    tex = std::static_pointer_cast<Texture<Color3f>>(
                        std::shared_ptr<GndObject>(
                            GndFactory::getInstance()->createInstance("constant_color", p)));
                }
            };

            auto loadFloat = [&](const std::string& name, std::shared_ptr<Texture<float>>& tex) {
                if (props.hasFloat(name)) {
                    PropertyList p;
                    p.setFloat("value", props.getFloat(name));
                    tex = std::static_pointer_cast<Texture<float>>(
                        std::shared_ptr<GndObject>(
                            GndFactory::getInstance()->createInstance("constant_float", p)));
                }
            };

            loadColor("baseColor", m_baseColor);
            loadFloat("metallic", m_metallic);
            loadFloat("roughness", m_roughness);
            loadFloat("specular", m_specular);
            loadFloat("specularTransmission", m_specularTransmission);
            loadFloat("specularTint", m_specularTint);
            loadFloat("anisotropic", m_anisotropic);
            loadFloat("sheen", m_sheen);
            loadFloat("sheenTint", m_sheenTint);
            loadFloat("clearcoat", m_clearcoat);
            loadFloat("clearcoatGloss", m_clearcoatGloss);

            m_subsurface = props.getFloat("subsurface", 0.0f);
            m_eta = props.getFloat("eta", 1.4f);
        }

        void addChild(std::shared_ptr<GndObject> child) override {
            if (child->getClassType() != ETexture)
                throw std::runtime_error("DisneyMaterial: cannot add specified child!");

            std::string n = child->getName();

            #define CHECK_AND_ADD_COLOR(name, var) \
                if (n == name) { if (var) throw std::runtime_error("DisneyMaterial: " name " already defined!"); \
                                var = std::static_pointer_cast<Texture<Color3f>>(child); return; }
            #define CHECK_AND_ADD_FLOAT(name, var) \
                if (n == name) { if (var) throw std::runtime_error("DisneyMaterial: " name " already defined!"); \
                                var = std::static_pointer_cast<Texture<float>>(child); return; }

            CHECK_AND_ADD_COLOR("baseColor", m_baseColor)
            CHECK_AND_ADD_FLOAT("metallic", m_metallic)
            CHECK_AND_ADD_FLOAT("roughness", m_roughness)
            CHECK_AND_ADD_FLOAT("specular", m_specular)
            CHECK_AND_ADD_FLOAT("specularTransmission", m_specularTransmission)
            CHECK_AND_ADD_FLOAT("specularTint", m_specularTint)
            CHECK_AND_ADD_FLOAT("anisotropic", m_anisotropic)
            CHECK_AND_ADD_FLOAT("sheen", m_sheen)
            CHECK_AND_ADD_FLOAT("sheenTint", m_sheenTint)
            CHECK_AND_ADD_FLOAT("clearcoat", m_clearcoat)
            CHECK_AND_ADD_FLOAT("clearcoatGloss", m_clearcoatGloss)

            #undef CHECK_AND_ADD_COLOR
            #undef CHECK_AND_ADD_FLOAT

            Material::addChild(child);
        }

        void activate() override {
            auto ensureColor = [&](std::shared_ptr<Texture<Color3f>>& tex, const Color3f& defaultVal) {
                if (!tex) {
                    PropertyList p; p.setColor("value", defaultVal);
                    tex = std::static_pointer_cast<Texture<Color3f>>(
                        std::shared_ptr<GndObject>(GndFactory::getInstance()->createInstance("constant_color", p)));
                    tex->activate();
                }
            };

            auto ensureFloat = [&](std::shared_ptr<Texture<float>>& tex, float defaultVal) {
                if (!tex) {
                    PropertyList p; p.setFloat("value", defaultVal);
                    tex = std::static_pointer_cast<Texture<float>>(
                        std::shared_ptr<GndObject>(GndFactory::getInstance()->createInstance("constant_float", p)));
                    tex->activate();
                }
            };

            ensureColor(m_baseColor, Color3f(0.5f));
            ensureFloat(m_metallic, 0.0f);
            ensureFloat(m_roughness, 0.5f);
            ensureFloat(m_specular, 0.5f);
            ensureFloat(m_specularTransmission, 0.0f);
            ensureFloat(m_specularTint, 0.0f);
            ensureFloat(m_anisotropic, 0.0f);
            ensureFloat(m_sheen, 0.0f);
            ensureFloat(m_sheenTint, 0.5f);
            ensureFloat(m_clearcoat, 0.0f);
            ensureFloat(m_clearcoatGloss, 1.0f);
        }

        void computeScatteringFunctions(SurfaceInteraction &isect, MemoryArena &arena) const override {
            applyNormalOrBump(isect);

            Color3f color = m_baseColor->evaluate(isect);
            float metallic = m_metallic->evaluate(isect);
            float roughness = m_roughness->evaluate(isect);
            float spec = m_specular->evaluate(isect);
            float specTrans = m_specularTransmission->evaluate(isect);
            float specTint = m_specularTint->evaluate(isect);
            float sheen = m_sheen->evaluate(isect);
            float cc = m_clearcoat->evaluate(isect);

            isect.bsdf = arena.create<BSDF>(isect, m_eta);

            float transmissionWeight = specTrans * (1.0f - metallic);
            float diffuseWeight = (1.0f - specTrans) * (1.0f - metallic);

            // Diffuse & Sheen
            if (diffuseWeight > 0.0f) {
                isect.bsdf->add(arena.create<DisneyDiffuseBxDF>(diffuseWeight * color, roughness, m_subsurface), diffuseWeight);

                if (sheen > 0.0f) {
                    float sheenWeight = diffuseWeight * sheen;
                    isect.bsdf->add(arena.create<DisneySheenBxDF>(sheenWeight, color, m_sheenTint->evaluate(isect)), 0.0f);
                }
            }

            // Specular
            float aspect = std::sqrt(1.0f - 0.9f * m_anisotropic->evaluate(isect));
            float ax = std::max(0.001f, roughness * roughness / aspect);
            float ay = std::max(0.001f, roughness * roughness * aspect);

            float specularWeight = std::max(0.01f, Lerp(metallic, spec, 1.0f));

            auto* distGTR2 = arena.create<TrowbridgeReitzDistribution>(ax, ay);
            auto* fresnel = arena.create<FresnelDisney>(color, metallic, spec, specTint);
            isect.bsdf->add(arena.create<MicrofacetReflection>(Color3f(1.0f), distGTR2, fresnel), specularWeight);

            // Clearcoat
            if (cc > 0.0f) {
                float gloss = m_clearcoatGloss->evaluate(isect);
                auto* distGTR1 = arena.create<GTR1Distribution>(std::lerp(0.1f, 0.001f, gloss));
                isect.bsdf->add(arena.create<DisneyClearcoatBxDF>(0.25f * cc, distGTR1), cc);
            }

            // Specular transmission
            if (transmissionWeight > 0.0f) {
                Color3f T(std::sqrt(color.r()), std::sqrt(color.g()), std::sqrt(color.b()));
                isect.bsdf->add(arena.create<DisneyTransmissionBxDF>(transmissionWeight * T, distGTR2, 1.0f, m_eta), transmissionWeight);
            }
        }

        Color3f getAlbedo(const SurfaceInteraction& isect) const override {
            return m_baseColor->evaluate(isect);
        }

        float getRoughness(const SurfaceInteraction &isect) const override {
            return m_roughness->evaluate(isect);
        }

        float getMetallic(const SurfaceInteraction &isect) const override {
            return m_metallic->evaluate(isect);
        }

        std::string toString() const {
            return std::format(
                "Disney[\n"
                "  baseColor = \t\t{}\n"
                "  metallic = \t\t{}\n"
                "  roughness = \t\t{}\n"
                "  specular = \t\t{}\n"
                "  specularTransmission= \t{}\n"
                "  specularTint = \t{}\n"
                "  anisotropic = \t{}\n"
                "  sheen = \t\t\t{}\n"
                "  sheenTint = \t\t{}\n"
                "  clearcoat = \t\t{}\n"
                "  clearcoatGloss = \t{}\n"
                "  subsurface = {}\n"
                "  normal = {}\n"
                "]",
                indent(m_baseColor->toString(), 2),
                indent(m_metallic->toString(), 2),
                indent(m_roughness->toString(), 2),
                indent(m_specular->toString(), 2),
                indent(m_specularTransmission->toString(), 2),
                indent(m_specularTint->toString(), 2),
                indent(m_anisotropic->toString(), 2),
                indent(m_sheen->toString(), 2),
                indent(m_sheenTint->toString(), 2),
                indent(m_clearcoat->toString(), 2),
                indent(m_clearcoatGloss->toString(), 2),
                m_subsurface,
                m_normalMap ? indent(m_normalMap->toString(), 2) : "  null");
        }

    private:
        std::shared_ptr<Texture<Color3f>> m_baseColor;
        std::shared_ptr<Texture<float>> m_metallic, m_roughness, m_specular, m_specularTint, m_specularTransmission;
        std::shared_ptr<Texture<float>> m_anisotropic, m_sheen, m_sheenTint, m_clearcoat, m_clearcoatGloss;
        float m_subsurface, m_eta;
    };

    GND_REGISTER_CLASS(DisneyMaterial, "disney");
}
