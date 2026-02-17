#include <gianduia/core/parser.h>
#include <gianduia/core/factory.h>
#include <gianduia/core/propertyList.h>
#include <gianduia/core/fileResolver.h>

#include <pugixml.hpp>
#include <vector>
#include <sstream>

namespace gnd {

    // Converts string "x,y,z" in Vector3f
    static Vector3f parseVector(const std::string& value) {
        std::stringstream ss(value);
        float x, y, z;
        char comma;
        if (!(ss >> x >> comma >> y >> comma >> z)) {
            // Fallback: tries reading with spaces
            std::stringstream ss2(value);
            if (!(ss2 >> x >> y >> z))
                throw std::runtime_error("Could not parse vector from string: " + value);
        }
        return Vector3f(x, y, z);
    }

    std::unique_ptr<GndObject> parseObject(pugi::xml_node node) {
        PropertyList props;
        std::vector<pugi::xml_node> childrenNodes;

        for (pugi::xml_node child : node.children()) {
            std::string tagName = child.name();
            std::string name = child.attribute("name").value();

            if (tagName == "boolean") {
                props.setBoolean(name, child.attribute("value").as_bool());
            }
            else if (tagName == "integer") {
                props.setInteger(name, child.attribute("value").as_int());
            }
            else if (tagName == "float") {
                props.setFloat(name, child.attribute("value").as_float());
            }
            else if (tagName == "string") {
                props.setString(name, child.attribute("value").value());
            }
            else if (tagName == "point") {
                if (child.attribute("value")) {
                    Vector3f v = parseVector(child.attribute("value").value());
                    props.setPoint3(name, Point3f(v.x(), v.y(), v.z()));
                } else {
                    float x = child.attribute("x").as_float(0.f);
                    float y = child.attribute("y").as_float(0.f);
                    float z = child.attribute("z").as_float(0.f);
                    props.setPoint3(name, Point3f(x, y, z));
                }
            }
            else if (tagName == "vector") {
                if (child.attribute("value")) {
                    props.setVector3(name, parseVector(child.attribute("value").value()));
                } else {
                    float x = child.attribute("x").as_float(0.f);
                    float y = child.attribute("y").as_float(0.f);
                    float z = child.attribute("z").as_float(0.f);
                    props.setVector3(name, Vector3f(x, y, z));
                }
            }
            else if (tagName == "color") {
                if (child.attribute("value")) {
                    Vector3f c = parseVector(child.attribute("value").value());
                    props.setColor(name, Color3f(c.x(), c.y(), c.z()));
                } else {
                    float r = child.attribute("r").as_float(0.f);
                    float g = child.attribute("g").as_float(0.f);
                    float b = child.attribute("b").as_float(0.f);
                    props.setColor(name, Color3f(r, g, b));
                }
            }
            else if (tagName == "transform") {
                Transform t;

                for (pugi::xml_node transChild : child.children()) {
                    std::string transName = transChild.name();
                    Transform step;

                    if (transName == "translate") {
                        float x = transChild.attribute("x").as_float(0.f);
                        float y = transChild.attribute("y").as_float(0.f);
                        float z = transChild.attribute("z").as_float(0.f);

                        step = Transform::Translate(Vector3f(x, y, z));
                    }
                    else if (transName == "scale") {
                        float value = transChild.attribute("value").as_float(0.f);
                        if (value != 0.0f) {
                            // Uniform scale
                            step = Transform::Scale(value, value, value);
                        } else {
                            // Non-uniform
                            float x = transChild.attribute("x").as_float(1.f);
                            float y = transChild.attribute("y").as_float(1.f);
                            float z = transChild.attribute("z").as_float(1.f);
                            step = Transform::Scale(x, y, z);
                        }
                    }
                    else if (transName == "rotate") {
                        float angle = transChild.attribute("angle").as_float();
                        float x = transChild.attribute("x").as_float(0.f);
                        float y = transChild.attribute("y").as_float(0.f);
                        float z = transChild.attribute("z").as_float(0.f);
                        step = Transform::Rotate(angle, Vector3f(x, y, z));
                    }
                    else if (transName == "lookat") {
                        Vector3f originV = parseVector(transChild.attribute("origin").value());
                        Point3f origin = Point3f(originV.x(), originV.y(), originV.z());

                        Vector3f targetV = parseVector(transChild.attribute("target").value());
                        Point3f target = Point3f(targetV.x(), targetV.y(), targetV.z());

                        Vector3f up     = parseVector(transChild.attribute("up").value());
                        step = Transform::LookAt(origin, target, up);
                    }

                    t = step * t;
                }
                props.setTransform(name, t);
            }
            else {
                childrenNodes.push_back(child);
            }
        }

        std::string type = node.attribute("type").value();
        // Fall back on tag name (e.g. scene, primitive)
        if (type.empty()) {
            type = node.name();
        }

        std::unique_ptr<GndObject> obj = GndFactory::getInstance()->createInstance(type, props);

        if (!obj) {
            throw std::runtime_error("Could not create instance of type: " + type);
        }

        // Recursive call on child nodes
        for (pugi::xml_node childNode : childrenNodes) {
            std::unique_ptr<GndObject> childObj = parseObject(childNode);

            obj->addChild(std::shared_ptr<GndObject>(std::move(childObj)));
        }

        obj->activate();

        return obj;
    }

    std::unique_ptr<GndObject> Parser::loadFromXML(const std::string& filename) {
        FileResolver::setBasePath(filename);
        FileResolver::setOutputPath(filename);
        FileResolver::setOutputName(filename);

        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(filename.c_str());

        if (!result) {
            throw std::runtime_error("XML parsing failed: " + std::string(result.description()));
        }

        pugi::xml_node root = doc.document_element();
        return parseObject(root);
    }

}
