#include <gianduia/core/bitmap.h>
#include <iostream>

#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <Imath/ImathBox.h>
#include <exception>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "gianduia/core/fileResolver.h"

namespace gnd {

    Bitmap::Bitmap(const std::string& exrAbsolutePath) {
        try {
            Imf::InputFile file(exrAbsolutePath.c_str());
            Imath::Box2i dw = file.header().dataWindow();

            m_width = dw.max.x - dw.min.x + 1;
            m_height = dw.max.y - dw.min.y + 1;

            m_pixels.resize(m_width * m_height, Color3f(0.0f));
            m_accumData = std::make_unique<AccumPixel[]>(m_width * m_height);

            Imf::FrameBuffer frameBuffer;
            char* base = (char*)m_pixels.data();

            int dx = dw.min.x;
            int dy = dw.min.y;
            size_t xStride = sizeof(Color3f);
            size_t yStride = m_width * sizeof(Color3f);
            base = base - dx * xStride - dy * yStride;

            frameBuffer.insert("R", Imf::Slice(Imf::FLOAT, base, xStride, yStride));
            frameBuffer.insert("G", Imf::Slice(Imf::FLOAT, base + sizeof(float), xStride, yStride));
            frameBuffer.insert("B", Imf::Slice(Imf::FLOAT, base + 2 * sizeof(float), xStride, yStride));

            file.setFrameBuffer(frameBuffer);
            file.readPixels(dw.min.y, dw.max.y);

            m_filter = std::make_shared<GaussianFilter>(PropertyList());

        } catch (const std::exception &e) {
            throw std::runtime_error("Bitmap: Error while loading EXR path " + exrAbsolutePath + ": " + e.what());
        }
    }

    void Bitmap::savePNG() const {
        std::vector<uint8_t> data(m_width * m_height * 3);

        int index = 0;
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                Color3f linearColor = m_pixels[y * m_width + x];
                Color3f srgbColor = linearColor.toSRGB();

                Color3f final = srgbColor * 255.0f + Color3f(0.5);
                final = final.clamp(0.0f, 255.0f);

                data[index++] = static_cast<uint8_t>(final.r());
                data[index++] = static_cast<uint8_t>(final.g());
                data[index++] = static_cast<uint8_t>(final.b());
            }
        }

        std::string filename = FileResolver::getOutputPath().string() + "/" + FileResolver::getOutputName() + ".png";

        int success = stbi_write_png(filename.c_str(), m_width, m_height, 3, data.data(), m_width * 3);

        if (!success) {
            std::cerr << "Error while saving image: " << filename << std::endl;
        } else {
            std::cout << "PNG image saved successfully: " << filename << std::endl;
        }
    }

    void Bitmap::saveEXR() const {
        std::string filename = FileResolver::getOutputPath().string() + "/" + FileResolver::getOutputName() + ".exr";

        try {
            Imf::Header header(m_width, m_height);

            header.channels().insert("R", Imf::Channel(Imf::FLOAT));
            header.channels().insert("G", Imf::Channel(Imf::FLOAT));
            header.channels().insert("B", Imf::Channel(Imf::FLOAT));

            Imf::OutputFile file(filename.c_str(), header);
            Imf::FrameBuffer frameBuffer;

            char* base = (char*)m_pixels.data();
            size_t xStride = sizeof(Color3f);
            size_t yStride = m_width * sizeof(Color3f);

            frameBuffer.insert("R", Imf::Slice(Imf::FLOAT, base, xStride, yStride));
            frameBuffer.insert("G", Imf::Slice(Imf::FLOAT, base + sizeof(float), xStride, yStride));
            frameBuffer.insert("B", Imf::Slice(Imf::FLOAT, base + 2 * sizeof(float), xStride, yStride));

            file.setFrameBuffer(frameBuffer);
            file.writePixels(m_height);

            std::cout << "EXR image saved successfully: " << filename << std::endl;

        } catch (const std::exception &e) {
            std::cerr << "Error while saving EXR image " << filename << ": " << e.what() << std::endl;
        }
    }

}
