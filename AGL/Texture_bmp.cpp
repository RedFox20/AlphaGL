#include "Texture.h"
#include <rpp/file_io.h>
#include <rpp/debugging.h>

namespace AGL
{
    ////////////////////////////////////////////////////////////////////////////////

    //// ---- BMP format structures ---- ////
#pragma pack(push)
#pragma pack(1) // make sure no struct alignment packing is made
    struct CIEXYZTRIPLE { struct CIEXYZ { long x, y, z; } r, g, b; };
    struct RGBQUAD { uint8_t b, g, r, x; };
    struct BitmapFileHeader { uint16_t Type; uint Size, Reserved, OffBits; };
    struct BitmapInfoHeader { uint Size; int Width, Height; uint16_t Planes, BitCount; uint Compression, SizeImage; uint XPelsPerMeter, YPelsPerMeter; uint ClrUsed, ClrImportant; };
    struct BitmapV5InfoHeader { BitmapInfoHeader BIH; uint RedMask, GreenMask, BlueMask, AlphaMask, CSType; CIEXYZTRIPLE EndPoints; uint GammaRed, GammaGreen, GammaBlue, Intent, ProfileData, ProfileSize, Reserved; };
#pragma pack(pop)

    static bool savePaddedBMP(strview fileName, const void* paddedData, int width, int height, int channels)
    {
        rpp::file file = { fileName, rpp::file::CREATENEW };
        if (!file) {
            LogError("Failed to create BMP file '%s'", fileName.to_cstr());
            return false;
        }

        int paddedSize = AlignRowTo4(width, channels) * height;

        // 8-bit data requires a color table (!)
        if (channels == 1)
        {
            RGBQUAD colors[256]; // we need to create a color table for an 8-bit image:
            const auto setColorTableBGRX = [](RGBQUAD (&col)[256], int i) {
                auto v = uint8_t(i);
                col[i].b = v, col[i].g = v, col[i].r = v, col[i].x = 0;
            };
            for(int i = 0; i < 256; i += 4) { // unrolled for win
                setColorTableBGRX(colors, i+0);
                setColorTableBGRX(colors, i+1);
                setColorTableBGRX(colors, i+2);
                setColorTableBGRX(colors, i+3);
            }
            constexpr uint HSize = sizeof(BitmapFileHeader) + sizeof(BitmapV5InfoHeader) + sizeof(colors);
            BitmapFileHeader bmfV5 = { 19778, HSize + paddedSize, 0, HSize,	};
            BitmapV5InfoHeader bmiV5 = { {
                                             sizeof(BitmapV5InfoHeader), // size of the V5 struct
                                             width, height,  // height of the image - negative for top-down bitmap
                                             uint16_t(1), uint16_t(8u), 0u,	// 1 plane, 8bpp, BI_RGB(uncompressed)
                                             uint(paddedSize),			// image data size
                                             3780u, 3780u,				// X/YPelsPerMeter
                                             256u, 0u					// 256 colors in the color table, all colors
                                         }, 0u, // set rest to 0
            };
            bmiV5.CSType = 0x01; // device dependent RGB colorspace
            bmiV5.Intent = 8;    // LCS_GM_ABS_COLORIMETRIC - use nearest palette match
            file.write(&bmfV5,  sizeof(bmfV5));
            file.write(&bmiV5,  sizeof(bmiV5));
            file.write(&colors, sizeof(colors));
        }
        else
        {
            BitmapFileHeader bmf = { 19778u, uint(54 + paddedSize), 0, 54, };
            BitmapInfoHeader bmi = {
                    sizeof(BitmapInfoHeader),	// size of this struct
                    width, height,					 // height of the image - negative for top-down bitmap
                    uint16_t(1), uint16_t(channels*8), 0u,// 1 plane, number of bits: 8/24/32, BI_RGB(uncompressed)
                    uint(paddedSize),				 // size of image
                    3780u, 3780u,					 // X/YPelsPerMeter
                    0u, 0u,							 // No colortable
            };
            file.write(&bmf, sizeof(bmf));
            file.write(&bmi, sizeof(bmi));
        }
        file.write(paddedData, paddedSize);
        return true;
    }

    bool Texture::saveAsBMP(strview fileName, const void* data, int width, int height, int channels)
    {
        uint8_t* paddedData = nullptr;
        int unpaddedStride = channels * width;
        if ((unpaddedStride & 3) != 0) // do we need to align the rows? (BMP requirement)
        {
            int paddedStride = AlignRowTo4(width, channels);
            int paddedSize = paddedStride * height;
            paddedData = (uint8_t*)malloc(size_t(paddedSize));

            uint8_t* dst = paddedData;
            const uint8_t* src = (const uint8_t*)data;
            for (int i = 0; i < height; i++) // for each row in the image
            {
                // just copy the padded amount straight away
                // the padded data will be somewhat garbage, since it's taken from the next row
                memcpy(dst, src, size_t(paddedStride));
                dst += paddedStride; // advance dst by padded rows
                src += unpaddedStride; // advance src by raw data size
            }
            data = paddedData; // just overwrite the original data pointer (since we already made a copy)
        }

        bool success = savePaddedBMP(fileName, data, width, height, channels);
        if (paddedData) free(paddedData); // clean up if we used paddedData
        return success;
    }

    bool Texture::saveAsBMP(strview fileName)
    {
        if (!glTexture)
            return false;

        constexpr bool useBGR = true;
        vector<uint8_t> image = this->getTextureData(useBGR);
        return savePaddedBMP(fileName, image.data(), glWidth, glHeight, glChannels);
    }


    ////////////////////////////////////////////////////////////////////////////////
    ////////// BMP loading

    struct binary_reader
    {
        uint8_t* buf, *ptr;
        binary_reader(const void* data, int size) : buf((uint8_t*)data), ptr(buf) {}
        template<class T> T* read() {
            T* result = (T*)ptr;
            ptr += sizeof(T);
            return result;
        }
        void set(int offset) { ptr = buf + offset; }
        void read(uint8_t* dst, int size) const { memcpy(dst, ptr, size_t(size)); }
    };

    bool Bitmap::loadBMP(const void* imageData, int numBytes)
    {
        clear();

        binary_reader reader = { imageData, numBytes };
        auto& bmh = *reader.read<BitmapFileHeader>();
        if (bmh.Type != 19778) { // read BitmapFileHeader for OffBits
            return false;
        }

        auto& bmi = *reader.read<BitmapInfoHeader>(); // read BitmapInfoHeader for size info
        int nchannels = bmi.BitCount >> 3;
        if (!(1 <= nchannels && nchannels <= 4)) {
            LogError("Corrupted BMP, invalid number of channels: %d", nchannels);
            return false;
        }

        auto* img = (uint8_t*)malloc(bmi.SizeImage); // allocate enough for the entire image
        reader.set(bmh.OffBits);  // seek to start of image data
        reader.read(img, bmi.SizeImage);
        Data     = img;
        Width    = bmi.Width;
        Height   = bmi.Height;
        Channels = nchannels;
        Stride   = bmi.SizeImage / bmi.Height;
        Owns     = true;
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////////
}
