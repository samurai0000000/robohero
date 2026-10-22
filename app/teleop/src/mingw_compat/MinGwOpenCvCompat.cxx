/*
 * MinGwOpenCvCompat.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

#if defined(_WIN32) && !defined(ROBOHERO_OPENCV_DECODE_ONLY)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mmsystem.h>
#include <vfw.h>
#endif

namespace cv {

enum VideoPixelFormat : uint32_t {
    VIDEO_FMT_BI_RGB = 0u,
    VIDEO_FMT_D3D_RGB24 = 20u,
    VIDEO_FMT_D3D_ARGB32 = 21u,
    VIDEO_FMT_D3D_RGB32 = 22u,
    VIDEO_FMT_YUY2 = 0x32595559u, // 'YUY2'
    VIDEO_FMT_YVYU = 0x55595659u, // 'YVYU'
    VIDEO_FMT_UYVY = 0x59565955u, // 'UYVY'
    VIDEO_FMT_NV12 = 0x3231564Eu, // 'NV12'
    VIDEO_FMT_YV12 = 0x32315659u, // 'YV12'
    VIDEO_FMT_I420 = 0x30323449u, // 'I420'
    VIDEO_FMT_IYUV = 0x56555949u, // 'IYUV'
    VIDEO_FMT_MJPG = 0x47504A4Du  // 'MJPG'
};

namespace {

uint8_t clampByte(double v)
{
    if (v <= 0.0) {
        return 0;
    }
    if (v >= 255.0) {
        return 255;
    }
    return static_cast<uint8_t>(std::lround(v));
}

inline void yuvToBgrPixel(int yVal, int uVal, int vVal,
                          uint8_t &bOut, uint8_t &gOut, uint8_t &rOut)
{
    int c = yVal - 16;
    if (c < 0) {
        c = 0;
    }
    const int d = uVal - 128;
    const int e = vVal - 128;
    const int r = (298 * c + 409 * e + 128) >> 8;
    const int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    const int b = (298 * c + 516 * d + 128) >> 8;
    bOut = static_cast<uint8_t>(std::clamp(b, 0, 255));
    gOut = static_cast<uint8_t>(std::clamp(g, 0, 255));
    rOut = static_cast<uint8_t>(std::clamp(r, 0, 255));
}

#ifndef ROBOHERO_OPENCV_DECODE_ONLY
QColor scalarToQColor(const Scalar &bgr)
{
    return QColor(static_cast<int>(clampByte(bgr[2])),
                  static_cast<int>(clampByte(bgr[1])),
                  static_cast<int>(clampByte(bgr[0])));
}

QImage matToRgbQImage(const Mat &src)
{
    if (src.empty()) {
        return QImage();
    }
    QImage img(src.cols, src.rows, QImage::Format_RGB888);
    for (int y = 0; y < src.rows; ++y) {
        const uint8_t *srow = src.ptr<uint8_t>(y);
        uint8_t *drow = img.scanLine(y);
        for (int x = 0; x < src.cols; ++x) {
            drow[x * 3 + 0] = srow[x * 3 + 2];
            drow[x * 3 + 1] = srow[x * 3 + 1];
            drow[x * 3 + 2] = srow[x * 3 + 0];
        }
    }
    return img;
}
#endif

void rgbQImageToMat(const QImage &img, Mat &dst)
{
    if (img.isNull()) {
        return;
    }
    QImage rgb = (img.format() == QImage::Format_RGB888)
                     ? img
                     : img.convertToFormat(QImage::Format_RGB888);
    if (dst.empty() || dst.rows != rgb.height() || dst.cols != rgb.width()) {
        dst.create(rgb.height(), rgb.width(), CV_8UC3);
    }
    for (int y = 0; y < rgb.height(); ++y) {
        const uint8_t *srow = rgb.constScanLine(y);
        uint8_t *drow = dst.ptr<uint8_t>(y);
        for (int x = 0; x < rgb.width(); ++x) {
            drow[x * 3 + 0] = srow[x * 3 + 2];
            drow[x * 3 + 1] = srow[x * 3 + 1];
            drow[x * 3 + 2] = srow[x * 3 + 0];
        }
    }
}

size_t resolveRowStride(size_t minRowBytes, int32_t explicitStride,
                        size_t dataLen, int height, size_t defaultAlignedStride = 0)
{
    const size_t h = static_cast<size_t>(std::max(1, height));
    if (explicitStride != 0) {
        const size_t absStride = static_cast<size_t>(
            std::llabs(static_cast<long long>(explicitStride)));
        if (absStride >= minRowBytes && absStride * h <= dataLen) {
            return absStride;
        }
    }
    if (defaultAlignedStride >= minRowBytes && defaultAlignedStride * h <= dataLen) {
        return defaultAlignedStride;
    }
    return minRowBytes;
}

} // namespace

bool decodeRawVideoFrameToBgr(const uint8_t *data,
                              size_t dataLen,
                              int width,
                              int height,
                              uint32_t formatCode,
                              uint16_t bitCount,
                              int32_t stride,
                              bool bottomUp,
                              Mat &outFrame)
{
    if (data == nullptr || dataLen == 0 || width <= 0 || height <= 0) {
        return false;
    }

    // 1. Packed 4:2:2 YUV formats (YUY2, YVYU, UYVY)
    if (formatCode == VIDEO_FMT_YUY2 ||
        formatCode == VIDEO_FMT_YVYU ||
        formatCode == VIDEO_FMT_UYVY) {
        const size_t minRowBytes = static_cast<size_t>(width) * 2;
        const size_t srcStride = resolveRowStride(minRowBytes, stride, dataLen, height);
        if (dataLen < srcStride * static_cast<size_t>(height)) {
            return false;
        }

        Mat frame(height, width, CV_8UC3);
        for (int y = 0; y < height; ++y) {
            const int sy = bottomUp ? (height - 1 - y) : y;
            const uint8_t *srow = data + static_cast<size_t>(sy) * srcStride;
            uint8_t *drow = frame.ptr<uint8_t>(y);
            int x = 0;
            for (; x + 1 < width; x += 2) {
                int y0 = 0;
                int u = 128;
                int y1 = 0;
                int v = 128;
                if (formatCode == VIDEO_FMT_YUY2) {
                    y0 = srow[0];
                    u = srow[1];
                    y1 = srow[2];
                    v = srow[3];
                } else if (formatCode == VIDEO_FMT_YVYU) {
                    y0 = srow[0];
                    v = srow[1];
                    y1 = srow[2];
                    u = srow[3];
                } else {
                    u = srow[0];
                    y0 = srow[1];
                    v = srow[2];
                    y1 = srow[3];
                }
                yuvToBgrPixel(y0, u, v,
                              drow[x * 3 + 0], drow[x * 3 + 1], drow[x * 3 + 2]);
                yuvToBgrPixel(y1, u, v,
                              drow[(x + 1) * 3 + 0],
                              drow[(x + 1) * 3 + 1],
                              drow[(x + 1) * 3 + 2]);
                srow += 4;
            }
            if (x < width) {
                const int y0 = (formatCode == VIDEO_FMT_UYVY) ? srow[1] : srow[0];
                const int u = (formatCode == VIDEO_FMT_YVYU) ? 128 :
                              ((formatCode == VIDEO_FMT_UYVY) ? srow[0] : srow[1]);
                yuvToBgrPixel(y0, u, 128,
                              drow[x * 3 + 0], drow[x * 3 + 1], drow[x * 3 + 2]);
            }
        }
        outFrame = std::move(frame);
        return true;
    }

    // 2. Semi-planar 4:2:0 NV12 (Y plane followed by interleaved UV plane)
    if (formatCode == VIDEO_FMT_NV12) {
        const size_t minRowBytes = static_cast<size_t>(width);
        const size_t uvRows = static_cast<size_t>((height + 1) / 2);
        size_t srcStride = minRowBytes;
        if (stride != 0) {
            const size_t absStride = static_cast<size_t>(
                std::llabs(static_cast<long long>(stride)));
            if (absStride >= minRowBytes &&
                absStride * (static_cast<size_t>(height) + uvRows) <= dataLen) {
                srcStride = absStride;
            }
        }
        const size_t yPlaneBytes = srcStride * static_cast<size_t>(height);
        const size_t uvPlaneBytes = srcStride * uvRows;
        if (dataLen < yPlaneBytes + uvPlaneBytes) {
            return false;
        }

        const uint8_t *yPlane = data;
        const uint8_t *uvPlane = data + yPlaneBytes;
        Mat frame(height, width, CV_8UC3);
        for (int y = 0; y < height; ++y) {
            const uint8_t *yRow = yPlane + static_cast<size_t>(y) * srcStride;
            const uint8_t *uvRow = uvPlane + static_cast<size_t>(y / 2) * srcStride;
            uint8_t *drow = frame.ptr<uint8_t>(y);
            for (int x = 0; x < width; ++x) {
                const size_t uvIdx = static_cast<size_t>(x & ~1);
                const int u = (uvIdx + 1 < srcStride) ? uvRow[uvIdx] : 128;
                const int v = (uvIdx + 1 < srcStride) ? uvRow[uvIdx + 1] : 128;
                yuvToBgrPixel(yRow[x], u, v,
                              drow[x * 3 + 0], drow[x * 3 + 1], drow[x * 3 + 2]);
            }
        }
        outFrame = std::move(frame);
        return true;
    }

    // 3. Planar 4:2:0 I420 / IYUV / YV12
    if (formatCode == VIDEO_FMT_I420 ||
        formatCode == VIDEO_FMT_IYUV ||
        formatCode == VIDEO_FMT_YV12) {
        const size_t yStride = static_cast<size_t>(width);
        const size_t uvWidth = static_cast<size_t>((width + 1) / 2);
        const size_t uvHeight = static_cast<size_t>((height + 1) / 2);
        const size_t yBytes = yStride * static_cast<size_t>(height);
        const size_t planeBytes = uvWidth * uvHeight;
        if (dataLen < yBytes + 2 * planeBytes) {
            return false;
        }

        const uint8_t *yPlane = data;
        const uint8_t *plane1 = data + yBytes;
        const uint8_t *plane2 = plane1 + planeBytes;
        const uint8_t *uPlane = (formatCode == VIDEO_FMT_YV12) ? plane2 : plane1;
        const uint8_t *vPlane = (formatCode == VIDEO_FMT_YV12) ? plane1 : plane2;

        Mat frame(height, width, CV_8UC3);
        for (int y = 0; y < height; ++y) {
            const uint8_t *yRow = yPlane + static_cast<size_t>(y) * yStride;
            const uint8_t *uRow = uPlane + static_cast<size_t>(y / 2) * uvWidth;
            const uint8_t *vRow = vPlane + static_cast<size_t>(y / 2) * uvWidth;
            uint8_t *drow = frame.ptr<uint8_t>(y);
            for (int x = 0; x < width; ++x) {
                const int u = uRow[x / 2];
                const int v = vRow[x / 2];
                yuvToBgrPixel(yRow[x], u, v,
                              drow[x * 3 + 0], drow[x * 3 + 1], drow[x * 3 + 2]);
            }
        }
        outFrame = std::move(frame);
        return true;
    }

    // 4. 24-bit BGR (BI_RGB with bitCount == 24 or D3DFMT_R8G8B8 == 20)
    if (formatCode == VIDEO_FMT_D3D_RGB24 ||
        (formatCode == VIDEO_FMT_BI_RGB && (bitCount == 24 || bitCount == 0))) {
        const size_t minRowBytes = static_cast<size_t>(width) * 3;
        const size_t alignedRowBytes = (minRowBytes + 3) & ~static_cast<size_t>(3);
        const size_t srcStride = resolveRowStride(
            minRowBytes, stride, dataLen, height, alignedRowBytes);
        if (dataLen < srcStride * static_cast<size_t>(height)) {
            return false;
        }

        Mat frame(height, width, CV_8UC3);
        for (int y = 0; y < height; ++y) {
            const int sy = bottomUp ? (height - 1 - y) : y;
            std::memcpy(frame.ptr<uint8_t>(y),
                        data + static_cast<size_t>(sy) * srcStride,
                        minRowBytes);
        }
        outFrame = std::move(frame);
        return true;
    }

    // 5. 32-bit BGRX / BGRA (BI_RGB with bitCount == 32 or D3DFMT_X8R8G8B8 / A8R8G8B8)
    if (formatCode == VIDEO_FMT_D3D_RGB32 ||
        formatCode == VIDEO_FMT_D3D_ARGB32 ||
        (formatCode == VIDEO_FMT_BI_RGB && bitCount == 32)) {
        const size_t minRowBytes = static_cast<size_t>(width) * 4;
        const size_t srcStride = resolveRowStride(minRowBytes, stride, dataLen, height);
        if (dataLen < srcStride * static_cast<size_t>(height)) {
            return false;
        }

        Mat frame(height, width, CV_8UC3);
        for (int y = 0; y < height; ++y) {
            const int sy = bottomUp ? (height - 1 - y) : y;
            const uint8_t *srow = data + static_cast<size_t>(sy) * srcStride;
            uint8_t *drow = frame.ptr<uint8_t>(y);
            for (int x = 0; x < width; ++x) {
                drow[x * 3 + 0] = srow[x * 4 + 0];
                drow[x * 3 + 1] = srow[x * 4 + 1];
                drow[x * 3 + 2] = srow[x * 4 + 2];
            }
        }
        outFrame = std::move(frame);
        return true;
    }

    // 6. Motion JPEG (MJPG)
    if (formatCode == VIDEO_FMT_MJPG) {
        if (dataLen > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        QImage qimg = QImage::fromData(data, static_cast<int>(dataLen));
        if (qimg.isNull()) {
            return false;
        }
        Mat frame;
        rgbQImageToMat(qimg, frame);
        if (frame.empty()) {
            return false;
        }
        outFrame = std::move(frame);
        return true;
    }

    return false;
}

#ifndef ROBOHERO_OPENCV_DECODE_ONLY

Mat::Mat()
    : rows(0)
    , cols(0)
    , type(CV_8UC3)
    , step(0)
    , data(nullptr)
    , _storage()
{
}

Mat::Mat(int _rows, int _cols, int _type)
    : rows(0)
    , cols(0)
    , type(_type)
    , step(0)
    , data(nullptr)
    , _storage()
{
    create(_rows, _cols, _type);
}

Mat::Mat(int _rows, int _cols, int _type, const Scalar &s)
    : rows(0)
    , cols(0)
    , type(_type)
    , step(0)
    , data(nullptr)
    , _storage()
{
    create(_rows, _cols, _type);
    const uint8_t b = clampByte(s[0]);
    const uint8_t g = clampByte(s[1]);
    const uint8_t r = clampByte(s[2]);
    for (int y = 0; y < rows; ++y) {
        uint8_t *rowPtr = ptr<uint8_t>(y);
        for (int x = 0; x < cols; ++x) {
            rowPtr[x * 3 + 0] = b;
            rowPtr[x * 3 + 1] = g;
            rowPtr[x * 3 + 2] = r;
        }
    }
}

Mat::Mat(int _rows, int _cols, int _type, void *_data, size_t _step)
    : rows(_rows > 0 ? _rows : 0)
    , cols(_cols > 0 ? _cols : 0)
    , type(_type)
    , step(_step > 0 ? _step : static_cast<size_t>(_cols > 0 ? _cols * 3 : 0))
    , data(static_cast<uint8_t *>(_data))
    , _storage()
{
}

void Mat::create(int _rows, int _cols, int _type)
{
    if (_rows <= 0 || _cols <= 0) {
        rows = 0;
        cols = 0;
        type = _type;
        step = 0;
        data = nullptr;
        _storage.reset();
        return;
    }
    rows = _rows;
    cols = _cols;
    type = _type;
    step = static_cast<size_t>(cols) * 3;
    _storage = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(rows) * step, 0);
    data = _storage->data();
}

bool Mat::empty() const noexcept
{
    return rows <= 0 || cols <= 0 || data == nullptr;
}

Mat Mat::clone() const
{
    if (empty()) {
        return Mat();
    }
    Mat out(rows, cols, type);
    const size_t rowBytes = static_cast<size_t>(cols) * 3;
    for (int y = 0; y < rows; ++y) {
        std::memcpy(out.ptr<uint8_t>(y), ptr<uint8_t>(y), rowBytes);
    }
    return out;
}

Mat Mat::operator()(const Rect &roi) const
{
    if (empty()) {
        return Mat();
    }
    int rx = std::clamp(roi.x, 0, cols);
    int ry = std::clamp(roi.y, 0, rows);
    int rw = std::clamp(roi.width, 0, cols - rx);
    int rh = std::clamp(roi.height, 0, rows - ry);
    if (rw <= 0 || rh <= 0) {
        return Mat();
    }
    Mat sub;
    sub.rows = rh;
    sub.cols = rw;
    sub.type = type;
    sub.step = step;
    sub._storage = _storage;
    sub.data = data + static_cast<size_t>(ry) * step + static_cast<size_t>(rx) * 3;
    return sub;
}

void Mat::copyTo(Mat &dst) const
{
    if (empty()) {
        dst = Mat();
        return;
    }
    if (dst.empty() || dst.rows != rows || dst.cols != cols || dst.type != type) {
        dst.create(rows, cols, type);
    }
    const size_t rowBytes = static_cast<size_t>(cols) * 3;
    for (int y = 0; y < rows; ++y) {
        std::memcpy(dst.ptr<uint8_t>(y), ptr<uint8_t>(y), rowBytes);
    }
}

void Mat::copyTo(Mat &&dst) const
{
    Mat lval = std::move(dst);
    copyTo(lval);
}

void flip(const Mat &src, Mat &dst, int flipCode)
{
    if (src.empty()) {
        dst = Mat();
        return;
    }
    Mat temp = src.clone();
    if (dst.empty() || dst.rows != temp.rows || dst.cols != temp.cols || dst.data == src.data) {
        if (dst.data != src.data) {
            dst.create(temp.rows, temp.cols, temp.type);
        }
    }

    const int h = temp.rows;
    const int w = temp.cols;
    for (int y = 0; y < h; ++y) {
        int sy = (flipCode <= 0) ? (h - 1 - y) : y;
        const uint8_t *srow = temp.ptr<uint8_t>(sy);
        uint8_t *drow = dst.ptr<uint8_t>(y);
        for (int x = 0; x < w; ++x) {
            int sx = (flipCode != 0) ? (w - 1 - x) : x;
            drow[x * 3 + 0] = srow[sx * 3 + 0];
            drow[x * 3 + 1] = srow[sx * 3 + 1];
            drow[x * 3 + 2] = srow[sx * 3 + 2];
        }
    }
}

void cvtColor(const Mat &src, Mat &dst, int code)
{
    (void)code;
    if (src.empty()) {
        dst = Mat();
        return;
    }
    Mat out(src.rows, src.cols, src.type);
    for (int y = 0; y < src.rows; ++y) {
        const uint8_t *srow = src.ptr<uint8_t>(y);
        uint8_t *drow = out.ptr<uint8_t>(y);
        for (int x = 0; x < src.cols; ++x) {
            drow[x * 3 + 0] = srow[x * 3 + 2];
            drow[x * 3 + 1] = srow[x * 3 + 1];
            drow[x * 3 + 2] = srow[x * 3 + 0];
        }
    }
    dst = std::move(out);
}

void resize(const Mat &src,
            Mat &dst,
            const Size &dsize,
            double fx,
            double fy,
            int interpolation)
{
    if (src.empty()) {
        dst = Mat();
        return;
    }
    int targetW = dsize.width;
    int targetH = dsize.height;
    if (targetW <= 0 && fx > 0.0) {
        targetW = static_cast<int>(std::lround(src.cols * fx));
    }
    if (targetH <= 0 && fy > 0.0) {
        targetH = static_cast<int>(std::lround(src.rows * fy));
    }
    if (targetW <= 0 || targetH <= 0) {
        dst = Mat();
        return;
    }

    Mat out(targetH, targetW, src.type);
    if (interpolation == INTER_NEAREST) {
        for (int y = 0; y < targetH; ++y) {
            int sy = std::clamp((y * src.rows) / targetH, 0, src.rows - 1);
            const uint8_t *srow = src.ptr<uint8_t>(sy);
            uint8_t *drow = out.ptr<uint8_t>(y);
            for (int x = 0; x < targetW; ++x) {
                int sx = std::clamp((x * src.cols) / targetW, 0, src.cols - 1);
                drow[x * 3 + 0] = srow[sx * 3 + 0];
                drow[x * 3 + 1] = srow[sx * 3 + 1];
                drow[x * 3 + 2] = srow[sx * 3 + 2];
            }
        }
        dst = std::move(out);
        return;
    }

    // Bilinear interpolation
    const double scaleX = static_cast<double>(src.cols) / static_cast<double>(targetW);
    const double scaleY = static_cast<double>(src.rows) / static_cast<double>(targetH);
    for (int y = 0; y < targetH; ++y) {
        double fySrc = (y + 0.5) * scaleY - 0.5;
        int y0 = static_cast<int>(std::floor(fySrc));
        double wy = fySrc - y0;
        int y1 = std::clamp(y0 + 1, 0, src.rows - 1);
        y0 = std::clamp(y0, 0, src.rows - 1);

        const uint8_t *row0 = src.ptr<uint8_t>(y0);
        const uint8_t *row1 = src.ptr<uint8_t>(y1);
        uint8_t *drow = out.ptr<uint8_t>(y);

        for (int x = 0; x < targetW; ++x) {
            double fxSrc = (x + 0.5) * scaleX - 0.5;
            int x0 = static_cast<int>(std::floor(fxSrc));
            double wx = fxSrc - x0;
            int x1 = std::clamp(x0 + 1, 0, src.cols - 1);
            x0 = std::clamp(x0, 0, src.cols - 1);

            for (int c = 0; c < 3; ++c) {
                double v00 = row0[x0 * 3 + c];
                double v01 = row0[x1 * 3 + c];
                double v10 = row1[x0 * 3 + c];
                double v11 = row1[x1 * 3 + c];
                double top = v00 * (1.0 - wx) + v01 * wx;
                double bot = v10 * (1.0 - wx) + v11 * wx;
                drow[x * 3 + c] = clampByte(top * (1.0 - wy) + bot * wy);
            }
        }
    }
    dst = std::move(out);
}

void line(Mat &img,
          const Point &pt1,
          const Point &pt2,
          const Scalar &color,
          int thickness,
          int lineType,
          int shift)
{
    (void)shift;
    if (img.empty()) {
        return;
    }
    QImage qimg = matToRgbQImage(img);
    {
        QPainter painter(&qimg);
        if (lineType == LINE_AA) {
            painter.setRenderHint(QPainter::Antialiasing, true);
        }
        QPen pen(scalarToQColor(color), std::max(1, thickness));
        painter.setPen(pen);
        painter.drawLine(pt1.x, pt1.y, pt2.x, pt2.y);
    }
    rgbQImageToMat(qimg, img);
}

void rectangle(Mat &img,
               const Rect &rec,
               const Scalar &color,
               int thickness,
               int lineType,
               int shift)
{
    (void)shift;
    if (img.empty() || rec.width <= 0 || rec.height <= 0) {
        return;
    }
    QImage qimg = matToRgbQImage(img);
    {
        QPainter painter(&qimg);
        if (lineType == LINE_AA) {
            painter.setRenderHint(QPainter::Antialiasing, true);
        }
        QColor qcol = scalarToQColor(color);
        if (thickness < 0) {
            painter.fillRect(rec.x, rec.y, rec.width, rec.height, qcol);
        } else {
            QPen pen(qcol, std::max(1, thickness));
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(rec.x, rec.y, rec.width, rec.height);
        }
    }
    rgbQImageToMat(qimg, img);
}

void circle(Mat &img,
            const Point &center,
            int radius,
            const Scalar &color,
            int thickness,
            int lineType,
            int shift)
{
    (void)shift;
    if (img.empty() || radius <= 0) {
        return;
    }
    QImage qimg = matToRgbQImage(img);
    {
        QPainter painter(&qimg);
        if (lineType == LINE_AA) {
            painter.setRenderHint(QPainter::Antialiasing, true);
        }
        QColor qcol = scalarToQColor(color);
        if (thickness < 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(qcol);
        } else {
            QPen pen(qcol, std::max(1, thickness));
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
        }
        painter.drawEllipse(QPoint(center.x, center.y), radius, radius);
    }
    rgbQImageToMat(qimg, img);
}

void putText(Mat &img,
             const std::string &text,
             const Point &org,
             int fontFace,
             double fontScale,
             const Scalar &color,
             int thickness,
             int lineType,
             bool bottomLeftOrigin)
{
    (void)fontFace;
    (void)bottomLeftOrigin;
    if (img.empty() || text.empty()) {
        return;
    }
    QImage qimg = matToRgbQImage(img);
    {
        QPainter painter(&qimg);
        if (lineType == LINE_AA) {
            painter.setRenderHint(QPainter::TextAntialiasing, true);
        }
        QFont font("Sans Serif");
        int pxSize = std::max(8, static_cast<int>(std::lround(fontScale * 22.0)));
        font.setPixelSize(pxSize);
        if (thickness >= 2) {
            font.setBold(true);
        }
        painter.setFont(font);
        painter.setPen(scalarToQColor(color));
        painter.drawText(org.x, org.y, QString::fromStdString(text));
    }
    rgbQImageToMat(qimg, img);
}

class VideoCapture::Impl {
public:
    Impl()
        : _opened(false)
        , _reqWidth(640)
        , _reqHeight(480)
        , _reqFps(30.0)
        , _configuredWidth(0)
        , _configuredHeight(0)
        , _configuredFps(0.0)
        , _formatDirty(false)
#ifdef _WIN32
        , _coInitialized(false)
        , _mfStarted(false)
        , _mediaSource(nullptr)
        , _sourceReader(nullptr)
        , _activeSubtype(GUID_NULL)
        , _activeWidth(0)
        , _activeHeight(0)
        , _activeStride(0)
        , _hwndCap(nullptr)
#endif
    {
    }

    ~Impl()
    {
        release();
    }

    bool open(int index, int apiPreference)
    {
        (void)apiPreference;
        release();
        if (std::getenv("ROBOHERO_DISABLE_AUTO_CAMERA") != nullptr) {
            return false;
        }
#ifdef _WIN32
        if (openMf(index)) {
            _opened = true;
            return true;
        }
        releaseMf();
        if (openVfw(index)) {
            _opened = true;
            return true;
        }
        releaseVfw();
        return false;
#else
        (void)index;
        return false;
#endif
    }

    bool isOpened() const
    {
        return _opened;
    }

    bool set(int propId, double value)
    {
        if (propId == CAP_PROP_FRAME_WIDTH && value > 0.0) {
            const int newWidth = static_cast<int>(value);
            if (newWidth != _reqWidth) {
                _reqWidth = newWidth;
                if (_opened && _reqWidth != _configuredWidth) {
                    _formatDirty = true;
                }
            }
            return true;
        }
        if (propId == CAP_PROP_FRAME_HEIGHT && value > 0.0) {
            const int newHeight = static_cast<int>(value);
            if (newHeight != _reqHeight) {
                _reqHeight = newHeight;
                if (_opened && _reqHeight != _configuredHeight) {
                    _formatDirty = true;
                }
            }
            return true;
        }
        if (propId == CAP_PROP_FPS && value > 0.0) {
            if (std::fabs(value - _reqFps) > 0.1) {
                _reqFps = value;
                if (_opened && std::fabs(_reqFps - _configuredFps) > 0.1) {
                    _formatDirty = true;
                }
            }
            return true;
        }
        return false;
    }

    double get(int propId) const
    {
        if (propId == CAP_PROP_FRAME_WIDTH) {
#ifdef _WIN32
            if (_activeWidth > 0) {
                return static_cast<double>(_activeWidth);
            }
#endif
            return static_cast<double>(_reqWidth);
        }
        if (propId == CAP_PROP_FRAME_HEIGHT) {
#ifdef _WIN32
            if (_activeHeight > 0) {
                return static_cast<double>(_activeHeight);
            }
#endif
            return static_cast<double>(_reqHeight);
        }
        if (propId == CAP_PROP_FPS) {
            return _reqFps;
        }
        return 0.0;
    }

    bool read(Mat &image)
    {
        if (!_opened) {
            return false;
        }
#ifdef _WIN32
        if (_sourceReader != nullptr) {
            if (_formatDirty) {
                configureMfMediaType();
            }
            return readMfSample(image);
        }
        if (_hwndCap != nullptr) {
            if (_formatDirty) {
                applyVfwVideoFormat();
            }
            capGrabFrameNoStop(_hwndCap);
            std::lock_guard<std::mutex> lock(_frameMutex);
            if (!_latestFrame.empty()) {
                image = _latestFrame.clone();
                return true;
            }
        }
#else
        (void)image;
#endif
        return false;
    }

    void release()
    {
#ifdef _WIN32
        releaseVfw();
        releaseMf();
        {
            std::lock_guard<std::mutex> lock(_frameMutex);
            _latestFrame = Mat();
        }
#endif
        _opened = false;
        _configuredWidth = 0;
        _configuredHeight = 0;
        _configuredFps = 0.0;
        _formatDirty = false;
    }

private:
#ifdef _WIN32
    static int rankSubtype(uint32_t fourccOrD3d)
    {
        switch (fourccOrD3d) {
        case VIDEO_FMT_YUY2:
        case VIDEO_FMT_D3D_RGB24:
        case VIDEO_FMT_D3D_RGB32:
        case VIDEO_FMT_D3D_ARGB32:
            return 0;
        case VIDEO_FMT_NV12:
        case VIDEO_FMT_UYVY:
        case VIDEO_FMT_YVYU:
        case VIDEO_FMT_I420:
        case VIDEO_FMT_IYUV:
        case VIDEO_FMT_YV12:
            return 1;
        case VIDEO_FMT_MJPG:
            return 2;
        default:
            return 3;
        }
    }

    bool openMf(int index)
    {
        if (index < 0) {
            return false;
        }

        const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(hrCo)) {
            _coInitialized = true;
        } else if (hrCo != RPC_E_CHANGED_MODE) {
            return false;
        }

        if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_FULL))) {
            return false;
        }
        _mfStarted = true;

        IMFAttributes *devAttr = nullptr;
        if (FAILED(MFCreateAttributes(&devAttr, 1)) || devAttr == nullptr) {
            return false;
        }
        if (FAILED(devAttr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                                    MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
            devAttr->Release();
            return false;
        }

        IMFActivate **devices = nullptr;
        UINT32 deviceCount = 0;
        const HRESULT hrEnum = MFEnumDeviceSources(devAttr, &devices, &deviceCount);
        devAttr->Release();
        if (FAILED(hrEnum) || devices == nullptr ||
            static_cast<UINT32>(index) >= deviceCount) {
            if (devices != nullptr) {
                for (UINT32 i = 0; i < deviceCount; ++i) {
                    if (devices[i] != nullptr) {
                        devices[i]->Release();
                    }
                }
                CoTaskMemFree(devices);
            }
            return false;
        }

        const HRESULT hrAct = devices[index]->ActivateObject(
            IID_IMFMediaSource, reinterpret_cast<void **>(&_mediaSource));
        for (UINT32 i = 0; i < deviceCount; ++i) {
            if (devices[i] != nullptr) {
                devices[i]->Release();
            }
        }
        CoTaskMemFree(devices);

        if (FAILED(hrAct) || _mediaSource == nullptr) {
            return false;
        }

        IMFAttributes *readerAttr = nullptr;
        if (SUCCEEDED(MFCreateAttributes(&readerAttr, 2)) && readerAttr != nullptr) {
            readerAttr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        }
        const HRESULT hrReader = MFCreateSourceReaderFromMediaSource(
            _mediaSource, readerAttr, &_sourceReader);
        if (readerAttr != nullptr) {
            readerAttr->Release();
        }
        if (FAILED(hrReader) || _sourceReader == nullptr) {
            return false;
        }

        _sourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        configureMfMediaType();
        return _activeWidth > 0 && _activeHeight > 0;
    }

    void configureMfMediaType()
    {
        if (_sourceReader == nullptr) {
            return;
        }

        IMFMediaType *bestNative = nullptr;
        long long bestResDist = std::numeric_limits<long long>::max();
        int bestRank = 99;
        double bestFpsDist = std::numeric_limits<double>::max();
        UINT32 bestW = 0;
        UINT32 bestH = 0;

        for (DWORD typeIdx = 0; ; ++typeIdx) {
            IMFMediaType *nativeType = nullptr;
            const HRESULT hr = _sourceReader->GetNativeMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, typeIdx, &nativeType);
            if (FAILED(hr) || nativeType == nullptr) {
                break;
            }

            GUID major = GUID_NULL;
            GUID sub = GUID_NULL;
            UINT32 w = 0;
            UINT32 h = 0;
            UINT32 fpsNum = 30;
            UINT32 fpsDen = 1;

            const bool isVideo =
                SUCCEEDED(nativeType->GetGUID(MF_MT_MAJOR_TYPE, &major)) &&
                IsEqualGUID(major, MFMediaType_Video) &&
                SUCCEEDED(nativeType->GetGUID(MF_MT_SUBTYPE, &sub)) &&
                SUCCEEDED(MFGetAttributeSize(nativeType, MF_MT_FRAME_SIZE, &w, &h)) &&
                w > 0 && h > 0;

            if (isVideo) {
                MFGetAttributeRatio(nativeType, MF_MT_FRAME_RATE, &fpsNum, &fpsDen);
                const double fps = (fpsDen > 0)
                                       ? (static_cast<double>(fpsNum) / static_cast<double>(fpsDen))
                                       : 30.0;
                const long long resDist =
                    std::llabs(static_cast<long long>(w) - _reqWidth) +
                    std::llabs(static_cast<long long>(h) - _reqHeight);
                const int rank = rankSubtype(sub.Data1);
                const double fpsDist = std::fabs(fps - _reqFps);

                const bool better =
                    (resDist < bestResDist) ||
                    (resDist == bestResDist && rank < bestRank) ||
                    (resDist == bestResDist && rank == bestRank && fpsDist < bestFpsDist);

                if (better) {
                    if (bestNative != nullptr) {
                        bestNative->Release();
                    }
                    bestNative = nativeType;
                    bestNative->AddRef();
                    bestResDist = resDist;
                    bestRank = rank;
                    bestFpsDist = fpsDist;
                    bestW = w;
                    bestH = h;
                }
            }
            nativeType->Release();
        }

        if (bestNative != nullptr) {
            _sourceReader->SetCurrentMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, bestNative);

            if (bestRank > 2) {
                const GUID fallbackSubtypes[] = {
                    MFVideoFormat_RGB32,
                    MFVideoFormat_RGB24,
                    MFVideoFormat_YUY2,
                    MFVideoFormat_NV12
                };
                for (const GUID &targetSub : fallbackSubtypes) {
                    IMFMediaType *outType = nullptr;
                    if (SUCCEEDED(MFCreateMediaType(&outType)) && outType != nullptr) {
                        outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                        outType->SetGUID(MF_MT_SUBTYPE, targetSub);
                        if (bestW > 0 && bestH > 0) {
                            MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, bestW, bestH);
                        }
                        const HRESULT hrSet = _sourceReader->SetCurrentMediaType(
                            MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outType);
                        outType->Release();
                        if (SUCCEEDED(hrSet)) {
                            break;
                        }
                    }
                }
            }
            bestNative->Release();
        }

        updateActiveMfMediaType();
        _configuredWidth = _reqWidth;
        _configuredHeight = _reqHeight;
        _configuredFps = _reqFps;
        _formatDirty = false;
    }

    void updateActiveMfMediaType()
    {
        if (_sourceReader == nullptr) {
            return;
        }
        IMFMediaType *curType = nullptr;
        if (SUCCEEDED(_sourceReader->GetCurrentMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, &curType)) &&
            curType != nullptr) {
            GUID sub = GUID_NULL;
            if (SUCCEEDED(curType->GetGUID(MF_MT_SUBTYPE, &sub))) {
                _activeSubtype = sub;
            }
            UINT32 w = 0;
            UINT32 h = 0;
            if (SUCCEEDED(MFGetAttributeSize(curType, MF_MT_FRAME_SIZE, &w, &h)) &&
                w > 0 && h > 0) {
                _activeWidth = static_cast<int>(w);
                _activeHeight = static_cast<int>(h);
            }
            UINT32 strideU32 = 0;
            if (SUCCEEDED(curType->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideU32))) {
                _activeStride = static_cast<int32_t>(strideU32);
            } else {
                _activeStride = 0;
            }
            curType->Release();
        }
    }

    bool readMfSample(Mat &image)
    {
        for (int attempt = 0; attempt < 30; ++attempt) {
            DWORD streamIndex = 0;
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            IMFSample *sample = nullptr;

            const HRESULT hr = _sourceReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0,
                &streamIndex,
                &flags,
                &timestamp,
                &sample);
            if (FAILED(hr)) {
                if (sample != nullptr) {
                    sample->Release();
                }
                return false;
            }

            if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0) {
                updateActiveMfMediaType();
            }

            if ((flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM)) != 0) {
                if (sample != nullptr) {
                    sample->Release();
                }
                return false;
            }

            if (sample == nullptr) {
                Sleep(10);
                continue;
            }

            Mat decoded;
            IMFMediaBuffer *buffer = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer)) && buffer != nullptr) {
                BYTE *rawPtr = nullptr;
                DWORD maxLen = 0;
                DWORD curLen = 0;
                if (SUCCEEDED(buffer->Lock(&rawPtr, &maxLen, &curLen)) &&
                    rawPtr != nullptr && curLen > 0) {
                    const int w = (_activeWidth > 0) ? _activeWidth : _reqWidth;
                    const int h = (_activeHeight > 0) ? _activeHeight : _reqHeight;
                    const bool bottomUp =
                        (_activeStride < 0) &&
                        (_activeSubtype.Data1 == VIDEO_FMT_D3D_RGB24 ||
                         _activeSubtype.Data1 == VIDEO_FMT_D3D_RGB32 ||
                         _activeSubtype.Data1 == VIDEO_FMT_D3D_ARGB32 ||
                         _activeSubtype.Data1 == VIDEO_FMT_BI_RGB);

                    decodeRawVideoFrameToBgr(
                        reinterpret_cast<const uint8_t *>(rawPtr),
                        static_cast<size_t>(curLen),
                        w,
                        h,
                        _activeSubtype.Data1,
                        0,
                        _activeStride,
                        bottomUp,
                        decoded);
                    buffer->Unlock();
                }
                buffer->Release();
            }
            sample->Release();

            if (!decoded.empty()) {
                if (_reqWidth > 0 && _reqHeight > 0 &&
                    (decoded.cols != _reqWidth || decoded.rows != _reqHeight)) {
                    Mat resized;
                    resize(decoded, resized, Size(_reqWidth, _reqHeight), 0.0, 0.0, INTER_LINEAR);
                    if (!resized.empty()) {
                        decoded = std::move(resized);
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(_frameMutex);
                    _latestFrame = decoded;
                }
                image = decoded.clone();
                return true;
            }
        }

        std::lock_guard<std::mutex> lock(_frameMutex);
        if (!_latestFrame.empty()) {
            image = _latestFrame.clone();
            return true;
        }
        return false;
    }

    void releaseMf()
    {
        if (_sourceReader != nullptr) {
            _sourceReader->Release();
            _sourceReader = nullptr;
        }
        if (_mediaSource != nullptr) {
            _mediaSource->Shutdown();
            _mediaSource->Release();
            _mediaSource = nullptr;
        }
        if (_mfStarted) {
            MFShutdown();
            _mfStarted = false;
        }
        if (_coInitialized) {
            CoUninitialize();
            _coInitialized = false;
        }
        _activeSubtype = GUID_NULL;
        _activeWidth = 0;
        _activeHeight = 0;
        _activeStride = 0;
    }

    bool openVfw(int index)
    {
        _hwndCap = capCreateCaptureWindowA(
            "RoboHeroTeleopVfwCapture",
            WS_POPUP,
            0, 0, _reqWidth, _reqHeight,
            nullptr,
            0);
        if (_hwndCap == nullptr) {
            return false;
        }
        capSetUserData(_hwndCap, this);
        capSetCallbackOnFrame(_hwndCap, &Impl::onVfwFrameCallback);
        if (!capDriverConnect(_hwndCap, index)) {
            DestroyWindow(_hwndCap);
            _hwndCap = nullptr;
            return false;
        }
        applyVfwVideoFormat();
        return true;
    }

    void applyVfwVideoFormat()
    {
        if (_hwndCap == nullptr) {
            return;
        }
        const DWORD fmtSize = capGetVideoFormatSize(_hwndCap);
        if (fmtSize >= sizeof(BITMAPINFOHEADER)) {
            std::vector<uint8_t> fmtBuf(fmtSize, 0);
            if (capGetVideoFormat(_hwndCap, fmtBuf.data(), fmtSize)) {
                auto *bmi = reinterpret_cast<BITMAPINFO *>(fmtBuf.data());
                const bool bottomUp = (bmi->bmiHeader.biHeight > 0);
                bmi->bmiHeader.biWidth = _reqWidth;
                bmi->bmiHeader.biHeight = bottomUp ? _reqHeight : -_reqHeight;
                if (bmi->bmiHeader.biCompression == BI_RGB &&
                    bmi->bmiHeader.biBitCount > 0) {
                    const size_t rowStride =
                        ((static_cast<size_t>(_reqWidth) * (bmi->bmiHeader.biBitCount / 8) + 3) &
                         ~static_cast<size_t>(3));
                    bmi->bmiHeader.biSizeImage =
                        static_cast<DWORD>(rowStride * static_cast<size_t>(_reqHeight));
                } else if (bmi->bmiHeader.biBitCount > 0) {
                    bmi->bmiHeader.biSizeImage = static_cast<DWORD>(
                        (static_cast<size_t>(_reqWidth) *
                         static_cast<size_t>(_reqHeight) *
                         bmi->bmiHeader.biBitCount) / 8);
                }
                capSetVideoFormat(_hwndCap, bmi, fmtSize);
            }
        }
        _configuredWidth = _reqWidth;
        _configuredHeight = _reqHeight;
        _configuredFps = _reqFps;
        _formatDirty = false;
    }

    void releaseVfw()
    {
        if (_hwndCap != nullptr) {
            capSetCallbackOnFrame(_hwndCap, nullptr);
            capDriverDisconnect(_hwndCap);
            DestroyWindow(_hwndCap);
            _hwndCap = nullptr;
        }
    }

    static LRESULT CALLBACK onVfwFrameCallback(HWND hWnd, LPVIDEOHDR lpVHdr)
    {
        auto *self = reinterpret_cast<Impl *>(capGetUserData(hWnd));
        if (self == nullptr || lpVHdr == nullptr || lpVHdr->lpData == nullptr) {
            return 0;
        }
        const DWORD fmtSize = capGetVideoFormatSize(hWnd);
        if (fmtSize < sizeof(BITMAPINFOHEADER)) {
            return 0;
        }
        std::vector<uint8_t> fmtBuf(fmtSize, 0);
        capGetVideoFormat(hWnd, fmtBuf.data(), fmtSize);
        const auto *bmi = reinterpret_cast<const BITMAPINFO *>(fmtBuf.data());
        const int w = bmi->bmiHeader.biWidth;
        const int h = std::abs(bmi->bmiHeader.biHeight);
        if (w <= 0 || h <= 0) {
            return 0;
        }

        const bool bottomUp =
            (bmi->bmiHeader.biHeight > 0) &&
            (bmi->bmiHeader.biCompression == BI_RGB);
        const size_t availBytes = (lpVHdr->dwBytesUsed > 0)
                                      ? static_cast<size_t>(lpVHdr->dwBytesUsed)
                                      : static_cast<size_t>(lpVHdr->dwBufferLength);

        Mat frame;
        if (decodeRawVideoFrameToBgr(
                lpVHdr->lpData,
                availBytes,
                w,
                h,
                bmi->bmiHeader.biCompression,
                bmi->bmiHeader.biBitCount,
                0,
                bottomUp,
                frame) &&
            !frame.empty()) {
            std::lock_guard<std::mutex> lock(self->_frameMutex);
            self->_latestFrame = std::move(frame);
        }
        return 1;
    }
#endif

    bool _opened;
    int _reqWidth;
    int _reqHeight;
    double _reqFps;
    int _configuredWidth;
    int _configuredHeight;
    double _configuredFps;
    bool _formatDirty;
#ifdef _WIN32
    bool _coInitialized;
    bool _mfStarted;
    IMFMediaSource *_mediaSource;
    IMFSourceReader *_sourceReader;
    GUID _activeSubtype;
    int _activeWidth;
    int _activeHeight;
    int32_t _activeStride;
    HWND _hwndCap;
    std::mutex _frameMutex;
    Mat _latestFrame;
#endif
};

VideoCapture::VideoCapture()
    : _impl(std::make_unique<Impl>())
{
}

VideoCapture::~VideoCapture() = default;

bool VideoCapture::open(int index, int apiPreference)
{
    return _impl->open(index, apiPreference);
}

bool VideoCapture::isOpened() const
{
    return _impl->isOpened();
}

bool VideoCapture::set(int propId, double value)
{
    return _impl->set(propId, value);
}

double VideoCapture::get(int propId) const
{
    return _impl->get(propId);
}

bool VideoCapture::read(Mat &image)
{
    return _impl->read(image);
}

void VideoCapture::release()
{
    _impl->release();
}

#endif // ROBOHERO_OPENCV_DECODE_ONLY

} // namespace cv

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
