#include <gianduia/core/factory.h>
#include <gianduia/core/primitive.h>
#include <gianduia/core/shape.h>

namespace gnd {

    enum class CSGOperation { Union, Intersect, Difference };

    class CSGShape : public Shape {
    public:
        CSGShape(const PropertyList& props) : Shape(props) {
            std::string op = props.getString("operation");
            op = str_tolower(op);

            if (op == "intersection")
                m_operation = CSGOperation::Intersect;
            else if (op == "difference")
                m_operation = CSGOperation::Difference;
            else
                m_operation = CSGOperation::Union;
        }

        virtual void addChild(std::shared_ptr<GndObject> child) override {
            if (child->getClassType() == EPrimitive) {
                if (!m_leftChild)
                    m_leftChild = std::static_pointer_cast<Primitive>(child);
                else if (!m_rightChild)
                    m_rightChild = std::static_pointer_cast<Primitive>(child);
                else
                    throw std::runtime_error("CSGShape: cannot attach more than two child to a node!");
            }
        }

        virtual void activate() override {
            if (!m_leftChild && !m_rightChild)
                throw std::runtime_error("CSGShape: no child attached to the node!");

            if (m_leftChild)
                m_leftChild->activate();
            if (m_rightChild)
                m_rightChild->activate();
        }

        bool rayIntersect(const Ray& ray, SurfaceInteraction& isect) const override {
            std::vector<BoundaryEvent> hits;
            getAllIntersections(ray, hits);

            for (const auto& hit : hits) {
                if (hit.t > ray.tMin && hit.t < ray.tMax) {

                    hit.hitPrimitive->fillInteraction(ray, hit.t, isect);

                    if (hit.flipNormal) {
                        isect.n = -isect.n;
                    }

                    return true;
                }
            }
            return false;
        }

        void getAllIntersections(const Ray& ray, std::vector<BoundaryEvent>& outHits) const override {
            std::vector<BoundaryEvent> hitsL, hitsR;
            if (m_leftChild)  m_leftChild->getAllIntersections(ray, hitsL);
            if (m_rightChild) m_rightChild->getAllIntersections(ray, hitsR);

            std::sort(hitsL.begin(), hitsL.end());
            std::sort(hitsR.begin(), hitsR.end());

            auto itL = hitsL.begin();
            auto itR = hitsR.begin();

            bool inL = false;
            bool inR = false;

            bool wasInside = applyOperation(m_operation, false, false);

            while (itL != hitsL.end() || itR != hitsR.end()) {
                BoundaryEvent currentHit;
                bool fromLeft = false;

                if (itL != hitsL.end() && (itR == hitsR.end() || itL->t - itR->t < Epsilon)) {
                    currentHit = *itL;
                    fromLeft = true;
                    ++itL;
                    inL = currentHit.isEntry;
                } else {
                    currentHit = *itR;
                    fromLeft = false;
                    ++itR;
                    inR = currentHit.isEntry;
                }

                bool isInside = applyOperation(m_operation, inL, inR);

                if (isInside != wasInside) {
                    currentHit.isEntry = isInside;

                    if (m_operation == CSGOperation::Difference && !fromLeft)
                        currentHit.flipNormal = true;

                    outHits.push_back(currentHit);
                    wasInside = isInside;
                }
            }
        }

        void fillInteraction(const Ray& ray, float t, SurfaceInteraction& isect) const override {
            throw std::runtime_error(
                "CSGShape::fillInteraction called! This is a logic error. "
                "The boundary event should point to the LEAF primitive (Sphere, etc.), "
                "not the CSG container."
            );
        }

        Bounds3f getBounds() const override {
            Bounds3f bounds;
            if (m_leftChild)
                bounds = Union(bounds, m_leftChild->getWorldBounds());
            if (m_rightChild)
                bounds = Union(bounds, m_rightChild->getWorldBounds());

            return bounds;
        }

        std::string toString() const override {
            std::string opString;
            switch (m_operation) {
                case CSGOperation::Intersect : opString = "Intersection"; break;
                case CSGOperation::Difference : opString = "Difference"; break;
                default : opString = "Union"; break;
            }

            return std::format(
                "CSGShape[\n"
                "  operation = {},\n"
                "  left child = \n"
                "{},\n"
                "  right child = \n"
                "{}\n"
                "]",
                opString,
                indent(m_leftChild ? m_leftChild->toString() : "null",2),
                indent(m_rightChild ? m_rightChild->toString() : "null",2)
            );
        }

    private:
        CSGOperation m_operation;
        std::shared_ptr<Primitive> m_leftChild;
        std::shared_ptr<Primitive> m_rightChild;

        static std::string str_tolower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c){ return std::tolower(c); }
            );
            return s;
        }

        static bool applyOperation(CSGOperation op, bool inL, bool inR) {
            switch (op) {
                case CSGOperation::Union:      return inL || inR;
                case CSGOperation::Intersect:  return inL && inR;
                case CSGOperation::Difference: return inL && !inR;
                default: return false;
            }
        }
    };

    GND_REGISTER_CLASS(CSGShape, "csg");
}
