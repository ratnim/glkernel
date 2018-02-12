#include "PngExporter.h"

#include "helper.h"

#include <cppassist/logging/logging.h>

void PngExporter::exportKernel() {
    png_bytepp pngData;
    int colorType;
    png_uint_32 width;
    png_uint_32 height;
    if (m_kernel.hasType<glkernel::kernel4>())
    {
        colorType = PNG_COLOR_TYPE_RGBA;
        const auto kernel = m_kernel.value<glkernel::kernel4>();
        width = kernel.width();
        height = kernel.height();
        pngData = toPng(kernel, 4);
    }
    else if (m_kernel.hasType<glkernel::kernel3>())
    {
        colorType = PNG_COLOR_TYPE_RGB;
        const auto kernel = m_kernel.value<glkernel::kernel3>();
        width = kernel.width();
        height = kernel.height();
        pngData = toPng(kernel, 3);
    }
    else if (m_kernel.hasType<glkernel::kernel2>())
    {
        colorType = PNG_COLOR_TYPE_GA;
        const auto kernel = m_kernel.value<glkernel::kernel2>();
        width = kernel.width();
        height = kernel.height();
        pngData = toPng(kernel, 2);
    }
    else if (m_kernel.hasType<glkernel::kernel1>())
    {
        colorType = PNG_COLOR_TYPE_GRAY;
        const auto kernel = m_kernel.value<glkernel::kernel1>();
        width = kernel.width();
        height = kernel.height();
        pngData = toPng(kernel, 1);
    }
    else
    {
        cppassist::error() << "Unknown kernel type found. Aborting...";
        return;
    }

    // TODO multiply height and width with depth for 3D kernels
    writeToFile(pngData, colorType, height, width);
}


// TODO support 3D kernels with depth
template <typename T>
png_bytepp PngExporter::toPng(const glkernel::tkernel<T> & kernel, const int channels)
{
    const auto minmax = findMinMaxElements(kernel);
    const auto min = minmax.first;
    const auto max = minmax.second;

    // TODO log scaling factor, error rate, variance, stddeviation
    cppassist::info() << "Scaling range of [" << min << ", " << max << "] to range [0, 1]";


    // memory for all rows:
    auto rows = (png_bytepp) malloc(kernel.height() * sizeof(png_bytep));
    // memory for one row: amount of channes * amount of pixels * png byte size
    for(int y = 0; y < kernel.height(); ++y) {
        // we are using 16 bit depth, so we need 2 bytes per channel
        rows[y] = (png_bytep) malloc(channels * kernel.width() * 2 * sizeof(png_byte));
    }

    for (auto y = 0; y < kernel.height(); ++y)
    {
        for (auto x = 0; x < kernel.width(); ++x)
        {
            auto value = kernel.value(x, y, 0);
            auto normalizedValue = (value - min) / (max - min);
            auto scaledValue = normalizedValue * static_cast<float>(std::numeric_limits<uint16_t>::max());
            auto roundedValue = glm::round(scaledValue);

            writeData<T>(roundedValue, rows[y], x);
        }
    }

    return rows;
}


// mostly taken from http://www.labbookpages.co.uk/software/imgProc/libPNG.html
void PngExporter::writeToFile(png_bytepp data, const int colorType, const png_uint_32 height, const png_uint_32 width) {

    png_structp png_ptr = nullptr;
    png_infop info_ptr = nullptr;

    // create file
    FILE *fp = fopen(m_outFileName.c_str(), "wb");

    if (!fp)
    {
        cppassist::error() << "File " << m_outFileName << " could not be opened for writing";
        goto cleanup;
    }


    // initialize write structure
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr)
    {
        cppassist::error() << "png_create_write_struct failed";
        goto cleanup;
    }

    // initialize info structure
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        cppassist::error() << "png_create_info_struct failed";
        goto cleanup;
    }

    // setjmp sets jump point to jump back to upon exception
    // this allows to find out where the exception occurred, print a corresponding error message, and clean up
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        cppassist::error() << "Error during init_io";
        goto cleanup;
    }

    png_init_io(png_ptr, fp);

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        cppassist::error() << "Error during writing header";
        goto cleanup;
    }

    // write png header
    png_set_IHDR(png_ptr,
                 info_ptr,
                 width,
                 height,
                 16, // bit depth
                 colorType,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        cppassist::error() << "Error during writing bytes";
        return;
    }

    // write png data
    png_write_image(png_ptr, (png_bytepp) data);

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        cppassist::error() << "Error during writing bytes";
        return;
    }

    png_write_end(png_ptr, nullptr);

    cleanup:

    if (fp) fclose(fp);
    if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr) png_destroy_write_struct(&png_ptr, (png_infopp) nullptr);

    for (size_t y = 0; y < height; ++y)
        free(data[y]);
    free(data);
}
