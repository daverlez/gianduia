#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <concepts>
#include <iostream>

#include "gianduia/math/color.h"
#include "gianduia/math/geometry.h"

#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfFrameBuffer.h>

namespace gnd {

    template <typename T>
    concept IsScalar = std::same_as<T, float> || std::same_as<T, double>;

    template <typename T>
    concept IsVector3 = std::same_as<T, Color3f> || std::same_as<T, Vector3f> || std::same_as<T, Normal3f>;

    template <typename T>
    class Image2D {
    public:
        Image2D() : m_width(0), m_height(0) {}
        Image2D(int width, int height, const T& clearValue = T())
            : m_width(width), m_height(height) {
            m_pixels.resize(width * height, clearValue);
        }

        void resize(int width, int height, const T& clearValue = T()) {
            m_width = width;
            m_height = height;
            m_pixels.assign(width * height, clearValue);
        }

        void clear(const T& clearValue = T()) {
            std::ranges::fill(m_pixels, clearValue);
        }

        void setPixel(int x, int y, const T& value) {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height)
                m_pixels[y * m_width + x] = value;
        }

        const T& getPixel(int x, int y) const {
            return m_pixels[y * m_width + x];
        }

        int width() const { return m_width; }
        int height() const { return m_height; }
        T* data() { return m_pixels.data(); }
        const T* data() const { return m_pixels.data(); }

        void saveEXR(const std::string& filename) const {
            if (m_width == 0 || m_height == 0) return;

            try {
                Imf::Header header(m_width, m_height);
                Imf::FrameBuffer frameBuffer;

                char* base = (char*)m_pixels.data();
                size_t xStride = sizeof(T);
                size_t yStride = m_width * sizeof(T);

                if constexpr (IsVector3<T>) {
                    header.channels().insert("R", Imf::Channel(Imf::FLOAT));
                    header.channels().insert("G", Imf::Channel(Imf::FLOAT));
                    header.channels().insert("B", Imf::Channel(Imf::FLOAT));

                    frameBuffer.insert("R", Imf::Slice(Imf::FLOAT, base, xStride, yStride));
                    frameBuffer.insert("G", Imf::Slice(Imf::FLOAT, base + sizeof(float), xStride, yStride));
                    frameBuffer.insert("B", Imf::Slice(Imf::FLOAT, base + 2 * sizeof(float), xStride, yStride));
                }
                else if constexpr (IsScalar<T>) {
                    header.channels().insert("Y", Imf::Channel(Imf::FLOAT));
                    frameBuffer.insert("Y", Imf::Slice(Imf::FLOAT, base, xStride, yStride));
                }
                else {
                    static_assert(IsVector3<T> || IsScalar<T>, "Image2D: Unsupported type for EXR export.");
                }

                Imf::OutputFile file(filename.c_str(), header);
                file.setFrameBuffer(frameBuffer);
                file.writePixels(m_height);
                
                std::cout << "Saved AOV: " << filename << std::endl;
            } catch (const std::exception &e) {
                std::cerr << "EXR Error: " << e.what() << std::endl;
            }
        }

        bool loadEXR(const std::string& filename) {
            try {
                Imf::InputFile file(filename.c_str());
                Imath::Box2i dw = file.header().dataWindow();

                int newWidth = dw.max.x - dw.min.x + 1;
                int newHeight = dw.max.y - dw.min.y + 1;
                resize(newWidth, newHeight);

                Imf::FrameBuffer frameBuffer;
                char* base = (char*)m_pixels.data();

                int dx = dw.min.x;
                int dy = dw.min.y;
                size_t xStride = sizeof(T);
                size_t yStride = m_width * sizeof(T);

                base = base - dx * xStride - dy * yStride;

                if constexpr (IsVector3<T>) {
                    frameBuffer.insert("R", Imf::Slice(Imf::FLOAT, base, xStride, yStride));
                    frameBuffer.insert("G", Imf::Slice(Imf::FLOAT, base + sizeof(float), xStride, yStride));
                    frameBuffer.insert("B", Imf::Slice(Imf::FLOAT, base + 2 * sizeof(float), xStride, yStride));
                }
                else if constexpr (IsScalar<T>) {
                    frameBuffer.insert("Y", Imf::Slice(Imf::FLOAT, base, xStride, yStride));
                }
                file.setFrameBuffer(frameBuffer);
                file.readPixels(dw.min.y, dw.max.y);

                std::cout << "Loaded AOV: " << filename << std::endl;
                return true;
            } catch (const std::exception &e) {
                std::cerr << "EXR Load Error (" << filename << "): " << e.what() << std::endl;
                return false;
            }
        }

    private:
        int m_width, m_height;
        std::vector<T> m_pixels;
    };
}