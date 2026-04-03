#include <gianduia/core/denoiser.h>
#include <OpenImageDenoise/oidn.hpp>
#include <iostream>

namespace gnd {
    void Denoiser::execute(Bitmap* film) {
        if (!film) return;

        std::cout << "Denoising image with OIDN..." << std::endl;

        oidn::DeviceRef device = oidn::newDevice();
        device.commit();

        oidn::FilterRef filter = device.newFilter("RT");

        int width = film->width();
        int height = film->height();
        float* colorPtr  = film->data();
        float* albedoPtr = film->albedoData();
        float* normalPtr = film->normalData();

        filter.setImage("color",  colorPtr, oidn::Format::Float3, width, height);

        if (albedoPtr) {
            filter.setImage("albedo", albedoPtr, oidn::Format::Float3, width, height);
        }

        if (normalPtr) {
            filter.setImage("normal", normalPtr, oidn::Format::Float3, width, height);
        }

        filter.setImage("output", colorPtr, oidn::Format::Float3, width, height);

        filter.set("hdr", true);

        filter.set("cleanAux", false);

        filter.commit();
        filter.execute();

        const char* errorMessage;
        if (device.getError(errorMessage) != oidn::Error::None) {
            std::cerr << "OIDN Error: " << errorMessage << std::endl;
        } else {
            std::cout << "Denoising complete!" << std::endl;
        }
    }
}