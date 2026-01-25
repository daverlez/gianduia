#pragma once

#include <gianduia/math/geometry.h>
#include <gianduia/math/transform.h>
#include <gianduia/math/color.h>
#include <map>
#include <string>
#include <stdexcept>

namespace gnd {

    class PropertyList {
    public:
        PropertyList() = default;

        void setBoolean(const std::string& name, bool value)                { m_booleans[name] = value; }
        void setInteger(const std::string& name, int value)                 { m_integers[name] = value; }
        void setFloat(const std::string& name, float value)                 { m_floats[name] = value; }
        void setString(const std::string& name, const std::string& value)   { m_strings[name] = value; }
        void setPoint(const std::string& name, const Point3f& value)        { m_points[name] = value; }
        void setVector(const std::string& name, const Vector3f& value)      { m_vectors[name] = value; }
        void setColor(const std::string& name, const Color3f& value)        { m_colors[name] = value; }
        void setTransform(const std::string& name, const Transform& value)  { m_transforms[name] = value; }

        bool hasBoolean(const std::string& name) const      { return m_booleans.contains(name); }
        bool hasInteger(const std::string& name) const      { return m_integers.contains(name); }
        bool hasFloat(const std::string& name) const        { return m_floats.contains(name); }
        bool hasString(const std::string& name) const       { return m_strings.contains(name); }
        bool hasPoint(const std::string& name) const        { return m_points.contains(name); }
        bool hasVector(const std::string& name) const       { return m_vectors.contains(name); }
        bool hasColor(const std::string& name) const        { return m_colors.contains(name); }
        bool hasTransform(const std::string& name) const    { return m_transforms.contains(name); }

        bool getBoolean(const std::string& name, bool defaultValue = false) const {
            auto it = m_booleans.find(name);
            return (it != m_booleans.end()) ? it->second : defaultValue;
        }

        int getInteger(const std::string& name, int defaultValue = 0) const {
            auto it = m_integers.find(name);
            return (it != m_integers.end()) ? it->second : defaultValue;
        }

        float getFloat(const std::string& name, float defaultValue = 0.0f) const {
            auto it = m_floats.find(name);
            return (it != m_floats.end()) ? it->second : defaultValue;
        }

        std::string getString(const std::string& name, const std::string& defaultValue = "") const {
            auto it = m_strings.find(name);
            return (it != m_strings.end()) ? it->second : defaultValue;
        }

        Point3f getPoint(const std::string& name, const Point3f& defaultValue = Point3f()) const {
            auto it = m_points.find(name);
            return (it != m_points.end()) ? it->second : defaultValue;
        }

        Vector3f getVector(const std::string& name, const Vector3f& defaultValue = Vector3f()) const {
            auto it = m_vectors.find(name);
            return (it != m_vectors.end()) ? it->second : defaultValue;
        }

        Color3f getColor(const std::string& name, const Color3f& defaultValue = Color3f(0.f)) const {
            auto it = m_colors.find(name);
            return (it != m_colors.end()) ? it->second : defaultValue;
        }

        Transform getTransform(const std::string& name, const Transform& defaultValue = Transform()) const {
            auto it = m_transforms.find(name);
            return (it != m_transforms.end()) ? it->second : defaultValue;
        }

    private:

        std::map<std::string, bool>        m_booleans;
        std::map<std::string, int>         m_integers;
        std::map<std::string, float>       m_floats;
        std::map<std::string, std::string> m_strings;
        std::map<std::string, Point3f>     m_points;
        std::map<std::string, Vector3f>    m_vectors;
        std::map<std::string, Color3f>     m_colors;
        std::map<std::string, Transform>   m_transforms;
    };

}