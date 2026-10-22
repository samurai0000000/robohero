/*
 * videoio.hpp
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_TELEOP_MINGW_OPENCV_VIDEOIO_HPP
#define ROBOHERO_TELEOP_MINGW_OPENCV_VIDEOIO_HPP

#include <opencv2/core.hpp>

namespace cv {

enum {
    CAP_ANY = 0,
    CAP_V4L2 = 200,
    CAP_DSHOW = 700
};

enum {
    CAP_PROP_FRAME_WIDTH = 3,
    CAP_PROP_FRAME_HEIGHT = 4,
    CAP_PROP_FPS = 5
};

class VideoCapture {
public:
    VideoCapture();
    ~VideoCapture();

    bool open(int index, int apiPreference = CAP_ANY);
    bool isOpened() const;
    bool set(int propId, double value);
    double get(int propId) const;
    bool read(Mat &image);
    void release();

private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace cv

#endif // ROBOHERO_TELEOP_MINGW_OPENCV_VIDEOIO_HPP

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
