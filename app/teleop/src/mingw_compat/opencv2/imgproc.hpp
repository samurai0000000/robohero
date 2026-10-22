/*
 * imgproc.hpp
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_TELEOP_MINGW_OPENCV_IMGPROC_HPP
#define ROBOHERO_TELEOP_MINGW_OPENCV_IMGPROC_HPP

#include <opencv2/core.hpp>

namespace cv {

enum {
    INTER_NEAREST = 0,
    INTER_LINEAR = 1,
    INTER_CUBIC = 2,
    INTER_AREA = 3
};

enum {
    COLOR_BGR2RGB = 4,
    COLOR_RGB2BGR = 4
};

enum {
    LINE_8 = 8,
    LINE_AA = 16
};

enum {
    FONT_HERSHEY_SIMPLEX = 0
};

void cvtColor(const Mat &src, Mat &dst, int code);

void resize(const Mat &src,
            Mat &dst,
            const Size &dsize,
            double fx = 0.0,
            double fy = 0.0,
            int interpolation = INTER_LINEAR);

void line(Mat &img,
          const Point &pt1,
          const Point &pt2,
          const Scalar &color,
          int thickness = 1,
          int lineType = LINE_8,
          int shift = 0);

void rectangle(Mat &img,
               const Rect &rec,
               const Scalar &color,
               int thickness = 1,
               int lineType = LINE_8,
               int shift = 0);

void circle(Mat &img,
            const Point &center,
            int radius,
            const Scalar &color,
            int thickness = 1,
            int lineType = LINE_8,
            int shift = 0);

void putText(Mat &img,
             const std::string &text,
             const Point &org,
             int fontFace,
             double fontScale,
             const Scalar &color,
             int thickness = 1,
             int lineType = LINE_8,
             bool bottomLeftOrigin = false);

} // namespace cv

#endif // ROBOHERO_TELEOP_MINGW_OPENCV_IMGPROC_HPP

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
