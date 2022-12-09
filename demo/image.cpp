#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <algorithm>
#include <functional>
#include <thread>
#include "stb_image.h"
#include "stb_image_write.h"
#include "image.h"

Image::Image()
{
    width = 0;
    height = 0;
    channels = 0;
    size = 0;
    data = nullptr;
}

Image::Image(const char *filename)
{
    if (load(filename))
    {
        std::cout << "Image loaded successfully" << std::endl;
    }
    else
    {
        std::cout << "Image failed to load" << std::endl;
    }
}

Image::Image(int width, int height, int channels)
{
    this->width = width;
    this->height = height;
    this->channels = channels;
    this->size = width * height * channels;
    this->data = new unsigned char[size];
}

Image::Image(int data, int width, int height, int channels)
{
    this->width = width;
    this->height = height;
    this->channels = channels;
    this->size = width * height * channels;
    this->data = new unsigned char[size];
    memcpy(this->data, (unsigned char *)data, size);
    for (int i = 0; i < 200; i++)
    {
        std::cout << (int)this->data[i] << " ";
    }
    std::cout << std::endl;
}

Image::Image(unsigned char *img_data, int width, int height, int channels)
{
    this->width = width;
    this->height = height;
    this->channels = channels;
    this->size = width * height * channels;
    this->data = new unsigned char[size];
    memcpy(this->data, img_data, size);
}

Image::Image(const Image &other)
{
    this->width = other.width;
    this->height = other.height;
    this->channels = other.channels;
    this->size = other.size;
    this->data = new unsigned char[size];
    memcpy(this->data, other.data, size);
}

Image::~Image()
{
    stbi_image_free(data);
}

bool Image::load(const char *filename)
{
    std::cout << "Loading image: " << filename << std::endl;

    data = stbi_load(filename, &width, &height, &channels, 0);
    if (data == nullptr)
    {
        return false;
    }
    size = width * height * channels;
    return true;
}

void Image::save(const char *filename)
{
    std::cout << "Saving image: " << filename << std::endl;
    stbi_write_jpg(filename, width, height, channels, data, 100);
}

void Image::convolution(const double *kernel, int kernelSize, unsigned char *out_img)
{
    int halfKernelSize = kernelSize / 2;

    int kernelArea = kernelSize * kernelSize;
    int kernelSum = 0;
    for (int i = 0; i < kernelArea; i++)
    {
        kernelSum += kernel[i];
    }

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            for (int c = 0; c < channels; c++)
            {
                double sum = 0.0;
                for (int i = -halfKernelSize; i <= halfKernelSize; i++)
                {
                    for (int j = -halfKernelSize; j <= halfKernelSize; j++)
                    {
                        int x2 = x + j;
                        int y2 = y + i;
                        if (x2 < 0 || x2 >= width || y2 < 0 || y2 >= height)
                        {
                            continue;
                        }
                        int index = (y2 * width + x2) * channels + c;
                        sum += data[index] * kernel[(i + halfKernelSize) * kernelSize + j + halfKernelSize];
                    }
                }

                if (kernelSum != 0)
                {
                    sum /= kernelSum;
                }

                int index = (y * width + x) * channels + c;
                out_img[index] = (uint8_t)sum;
            }
        }
    }

    int min = 255;
    int max = 0;
    for (int i = 0; i < size; i++)
    {
        if (out_img[i] < min)
        {
            min = out_img[i];
        }
        if (out_img[i] > max)
        {
            max = out_img[i];
        }
    }

    for (int i = 0; i < size; i++)
    {
        out_img[i] = (out_img[i] - min) * 255 / (max - min);
    }
    // std::cout << minPixel << std::endl << maxPixel << std::endl;
}

Image Image::convolution_singlethread(const double *kernel, int kernelSize)
{
    Image res(width, height, channels);
    convolution(kernel, kernelSize, res.data);
    return res;
}

Image Image::convolution_multithread(const double *kernel, int kernelSize)
{
    int partWidth = width / 2;
    int partHeight = height / 2;
    int partSize = partWidth * partHeight * channels;
    unsigned char *part1 = new unsigned char[partSize];
    unsigned char *part2 = new unsigned char[partSize];
    unsigned char *part3 = new unsigned char[partSize];
    unsigned char *part4 = new unsigned char[partSize];

    for (int j = 0; j < partWidth; j++)
    {
        for (int i = 0; i < partHeight; i++)
        {
            for (int k = 0; k < channels; k++)
            {
                part1[(i * partWidth + j) * channels + k] = this->getPixel(i, j, k);
                part2[(i * partWidth + j) * channels + k] = this->getPixel(i, j + partWidth, k);
                part3[(i * partWidth + j) * channels + k] = this->getPixel(i + partHeight, j, k);
                part4[(i * partWidth + j) * channels + k] = this->getPixel(i + partHeight, j + partWidth, k);
            }
        }
    }

    Image image1(part1, partWidth, partHeight, channels);
    Image image2(part2, partWidth, partHeight, channels);
    Image image3(part3, partWidth, partHeight, channels);
    Image image4(part4, partWidth, partHeight, channels);

    auto fptr = (void(Image::*)(const double *, int, unsigned char *)) & Image::convolution;
    std::thread t1(fptr, image1, kernel, kernelSize, std::ref(part1));
    std::thread t2(fptr, image2, kernel, kernelSize, std::ref(part2));
    std::thread t3(fptr, image3, kernel, kernelSize, std::ref(part3));
    std::thread t4(fptr, image4, kernel, kernelSize, std::ref(part4));

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    // std::cout << "Image parts processed" << std::endl;

    unsigned char *combined = new unsigned char[size];

    for (int j = 0; j < partWidth; j++)
    {
        for (int i = 0; i < partHeight; i++)
        {
            for (int k = 0; k < channels; k++)
            {
                int index = (i * partWidth + j) * channels + k;
                combined[(i * width + j) * channels + k] = part1[index];
                combined[(i * width + j + partWidth) * channels + k] = part2[index];
                combined[((i + partHeight) * width + j) * channels + k] = part3[index];
                combined[((i + partHeight) * width + j + partWidth) * channels + k] = part4[index];
            }
        }
    }

    Image combinedImage(combined, width, height, channels);

    delete[] part1;
    delete[] part2;
    delete[] part3;
    delete[] part4;
    delete[] combined;

    return combinedImage;
}

int Image::getDataPtr()
{
    // for debugging purposes
    // std::cout << "Data pointer: " << (int)(size_t)data << std::endl;
    // for (int i = 0; i < 20; i++)
    // {
    //     std::cout << (int)data[i] << " ";
    // }
    // std::cout << std::endl;
    // for (int i = size - 20; i < size; i++)
    // {
    //     std::cout << (int)data[i] << " ";
    // }

    return (int)(size_t)data;
}

int Image::getSize()
{
    // std::cout << "Data size: " << size << std::endl;
    return size;
}

int Image::getWidth()
{
    // std::cout << "Image width: " << width << std::endl;
    // std::cout << "Image height: " << height << std::endl;
    return width;
}

int Image::getHeight()
{
    return height;
}

unsigned char Image::getPixel(int i, int j, int c)
{
    return data[(i * width + j) * channels + c];
}

// = operator
Image &Image::operator=(const Image &other)
{
    if (this != &other)
    {
        width = other.width;
        height = other.height;
        channels = other.channels;
        size = other.size;
        data = new unsigned char[size];
        memcpy(data, other.data, size);
    }
    return *this;
}