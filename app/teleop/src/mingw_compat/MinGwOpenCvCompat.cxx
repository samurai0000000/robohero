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
#include <cstdlib>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <vfw.h>
#endif

namespace cv {

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

} // namespace

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
#ifdef _WIN32
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
        _opened = true;
        return true;
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
            _reqWidth = static_cast<int>(value);
            return true;
        }
        if (propId == CAP_PROP_FRAME_HEIGHT && value > 0.0) {
            _reqHeight = static_cast<int>(value);
            return true;
        }
        if (propId == CAP_PROP_FPS && value > 0.0) {
            _reqFps = value;
            return true;
        }
        return false;
    }

    double get(int propId) const
    {
        if (propId == CAP_PROP_FRAME_WIDTH) {
            return static_cast<double>(_reqWidth);
        }
        if (propId == CAP_PROP_FRAME_HEIGHT) {
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
        if (_hwndCap != nullptr) {
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
        if (_hwndCap != nullptr) {
            capSetCallbackOnFrame(_hwndCap, nullptr);
            capDriverDisconnect(_hwndCap);
            DestroyWindow(_hwndCap);
            _hwndCap = nullptr;
        }
#endif
        _opened = false;
    }

private:
#ifdef _WIN32
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
        const bool bottomUp = (bmi->bmiHeader.biHeight > 0);
        if (w <= 0 || h <= 0) {
            return 0;
        }

        if (bmi->bmiHeader.biCompression == BI_RGB && bmi->bmiHeader.biBitCount == 24) {
            const size_t srcStride = ((static_cast<size_t>(w) * 3 + 3) & ~static_cast<size_t>(3));
            if (lpVHdr->dwBytesUsed >= srcStride * static_cast<size_t>(h)) {
                Mat frame(h, w, CV_8UC3);
                for (int y = 0; y < h; ++y) {
                    int sy = bottomUp ? (h - 1 - y) : y;
                    std::memcpy(frame.ptr<uint8_t>(y),
                                lpVHdr->lpData + static_cast<size_t>(sy) * srcStride,
                                static_cast<size_t>(w) * 3);
                }
                std::lock_guard<std::mutex> lock(self->_frameMutex);
                self->_latestFrame = std::move(frame);
            }
        }
        return 1;
    }
#endif

    bool _opened;
    int _reqWidth;
    int _reqHeight;
    double _reqFps;
#ifdef _WIN32
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
