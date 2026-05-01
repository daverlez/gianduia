#include <gianduia/core/denoiser.h>
#include <OpenImageDenoise/oidn.hpp>
#include <iostream>

namespace gnd {
    void Denoiser::execute(Film* film) {
        if (!film) return;

        std::cout << "Denoising image with OIDN..." << std::endl;

        oidn::DeviceRef device = oidn::newDevice();
        device.commit();

        oidn::FilterRef filter = device.newFilter("RT");

        int width = film->width();
        int height = film->height();
        size_t bufferSize = width * height * 3 * sizeof(float);

        float* colorPtr  = reinterpret_cast<float*>(film->getRadiance().data());
        float* albedoPtr = reinterpret_cast<float*>(film->getAlbedo().data());
        float* normalPtr = reinterpret_cast<float*>(film->getNormal().data());

        oidn::BufferRef colorBuf = device.newBuffer(bufferSize);
        colorBuf.write(0, bufferSize, colorPtr);
        filter.setImage("color", colorBuf, oidn::Format::Float3, width, height);

        if (albedoPtr) {
            oidn::BufferRef albedoBuf = device.newBuffer(bufferSize);
            albedoBuf.write(0, bufferSize, albedoPtr);
            filter.setImage("albedo", albedoBuf, oidn::Format::Float3, width, height);
        }

        if (normalPtr) {
            oidn::BufferRef normalBuf = device.newBuffer(bufferSize);
            normalBuf.write(0, bufferSize, normalPtr);
            filter.setImage("normal", normalBuf, oidn::Format::Float3, width, height);
        }

        filter.setImage("output", colorBuf, oidn::Format::Float3, width, height);

        filter.set("hdr", true);
        filter.set("cleanAux", false);

        filter.commit();
        filter.execute();

        const char* errorMessage;
        if (device.getError(errorMessage) != oidn::Error::None) {
            std::cerr << "OIDN Error: " << errorMessage << std::endl;
        } else {
            colorBuf.read(0, bufferSize, colorPtr);
            std::cout << "Denoising complete!" << std::endl;
        }
    }
}