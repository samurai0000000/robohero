/*
 * core.hpp
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_TELEOP_MINGW_OPENCV_CORE_HPP
#define ROBOHERO_TELEOP_MINGW_OPENCV_CORE_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifndef CV_8UC3
#define CV_8UC3 16
#endif

namespace cv {

struct Point {
    int x = 0;
    int y = 0;

    Point() = default;
    Point(int _x, int _y)
        : x(_x)
        , y(_y)
    {
    }
    Point(float _x, float _y)
        : x(static_cast<int>(std::lround(_x)))
        , y(static_cast<int>(std::lround(_y)))
    {
    }
    Point(double _x, double _y)
        : x(static_cast<int>(std::lround(_x)))
        , y(static_cast<int>(std::lround(_y)))
    {
    }
};

struct Size {
    int width = 0;
    int height = 0;

    Size() = default;
    Size(int w, int h)
        : width(w)
        , height(h)
    {
    }
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    Rect() = default;
    Rect(int _x, int _y, int _w, int _h)
        : x(_x)
        , y(_y)
        , width(_w)
        , height(_h)
    {
    }
};

struct Scalar {
    double val[4];

    Scalar(double v0 = 0.0, double v1 = 0.0, double v2 = 0.0, double v3 = 0.0)
        : val{v0, v1, v2, v3}
    {
    }

    double &operator[](int i)
    {
        return val[i];
    }

    double operator[](int i) const
    {
        return val[i];
    }
};

struct Vec3b {
    uint8_t val[3];

    Vec3b()
        : val{0, 0, 0}
    {
    }

    Vec3b(uint8_t v0, uint8_t v1, uint8_t v2)
        : val{v0, v1, v2}
    {
    }

    uint8_t &operator[](int i)
    {
        return val[i];
    }

    const uint8_t &operator[](int i) const
    {
        return val[i];
    }
};

static_assert(sizeof(Vec3b) == 3, "cv::Vec3b must be 3 bytes");

class Mat {
public:
    int rows = 0;
    int cols = 0;
    int type = CV_8UC3;
    size_t step = 0;
    uint8_t *data = nullptr;

    Mat();
    Mat(int _rows, int _cols, int _type);
    Mat(int _rows, int _cols, int _type, const Scalar &s);
    Mat(int _rows, int _cols, int _type, void *_data, size_t _step = 0);
    Mat(const Mat &other) = default;
    Mat(Mat &&other) noexcept = default;
    Mat &operator=(const Mat &other) = default;
    Mat &operator=(Mat &&other) noexcept = default;
    ~Mat() = default;

    void create(int _rows, int _cols, int _type);
    bool empty() const noexcept;
    Mat clone() const;
    Mat operator()(const Rect &roi) const;
    void copyTo(Mat &dst) const;
    void copyTo(Mat &&dst) const;

    template <typename T>
    T *ptr(int y = 0)
    {
        return reinterpret_cast<T *>(data + static_cast<size_t>(y) * step);
    }

    template <typename T>
    const T *ptr(int y = 0) const
    {
        return reinterpret_cast<const T *>(data + static_cast<size_t>(y) * step);
    }

private:
    std::shared_ptr<std::vector<uint8_t>> _storage;
};

void flip(const Mat &src, Mat &dst, int flipCode);

} // namespace cv

#endif // ROBOHERO_TELEOP_MINGW_OPENCV_CORE_HPP

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
