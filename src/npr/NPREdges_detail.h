/******************************************************************************
Copyright 2015 Tatsuya Yatagawa (tatsy)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#ifndef SRC_NPR_NPREDGES_DETAIL_H_
#define SRC_NPR_NPREDGES_DETAIL_H_

#include "VectorField.h"
#include "../npr/lic.h"

namespace lime {

namespace npr {

#pragma region DoGParam

inline DoGParam::DoGParam(double _kappa, double _sigma, double _tau, double _phi, DoGType _dogType)
    : kappa(_kappa)
    , sigma(_sigma)
    , tau(_tau)
    , phi(_phi)
    , dogType(_dogType) {
}

#pragma endregion

namespace {  // NOLINT

void uniformNoise(cv::OutputArray noise, const cv::InputArray gray, int nNoise) {
    cv::Mat  img = gray.getMat();
    cv::Mat& out = noise.getMatRef();

    msg_assert(img.channels() == 1 && img.depth() == CV_32F,
        "input image must be single channel and floating-point-valued");

    const int width  = img.cols;
    const int height = img.rows;

    Random rand = Random::getRNG();

    out = cv::Mat::zeros(height, width, CV_32FC1);
    int count = 0;
    while (count < nNoise) {
        int x = rand.randInt(width);
        int y = rand.randInt(height);
        if (rand.randReal() < img.at<float>(y, x)) continue;
        out.at<float>(y, x) = 1.0f;
        count++;
    }
}

void edgeXDoG(cv::InputArray input, cv::OutputArray output, const DoGParam& param) {
    cv::Mat  gray = input.getMat();
    cv::Mat& edge = output.getMatRef();

    const int width = gray.cols;
    const int height = gray.rows;
    const int dim = gray.channels();

    cv::Mat g1, g2;
    cv::GaussianBlur(gray, g1, cv::Size(0, 0), param.sigma);
    cv::GaussianBlur(gray, g2, cv::Size(0, 0), param.kappa * param.sigma);

    edge = cv::Mat(height, width, CV_32FC1);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double diff = g1.at<float>(y, x) - param.tau * g2.at<float>(y, x);
            if (diff > 0.0) {
                edge.at<float>(y, x) = 1.0f;
            } else {
                edge.at<float>(y, x) = static_cast<float>(1.0 + tanh(param.phi * diff));
            }
        }
    }
}

void gaussWithFlow(cv::InputArray input, cv::OutputArray output, const cv::Mat& vfield,
    int ksize, double sigma_s, double sigma_t) {
    cv::Mat  image = input.getMat();
    cv::Mat& out = output.getMatRef();

    const int width = image.cols;
    const int height = image.rows;
    const int dim = image.channels();
    const int L = static_cast<int>(ksize * 1.5);
    out = cv::Mat::zeros(height, width, CV_MAKETYPE(CV_32F, dim));

    cv::Mat temp = cv::Mat::zeros(height, width, CV_MAKETYPE(CV_32F, dim));

    ompfor(int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double tx = vfield.at<float>(y, x * 2 + 0);
            double ty = vfield.at<float>(y, x * 2 + 1);
            double weight = 0.0;
            for (int t = -ksize; t <= ksize; t++) {
                int xx = static_cast<int>(x - 0.5 * ty * t);
                int yy = static_cast<int>(y + 0.5 * tx * t);
                if (xx >= 0 && yy >= 0 && xx < width && yy < height) {
                    double w = exp(-t * t / sigma_t);
                    for (int c = 0; c < dim; c++) {
                        temp.at<float>(y, x*dim + c) += static_cast<float>(w * image.at<float>(yy, xx*dim + c));
                    }
                    weight += w;
                }
            }

            for (int c = 0; c < dim; c++) {
                temp.at<float>(y, x*dim + c) = temp.at<float>(y, x*dim + c) / static_cast<float>(weight);
            }
        }
    }

    ompfor(int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double weight = 0.0;
            for (int c = 0; c < dim; c++) {
                out.at<float>(y, x*dim + c) = 0.0f;
            }

            for (int pm = -1; pm <= 1; pm += 2) {
                int l = 0;
                double tx = pm * vfield.at<float>(y, x * 2 + 0);
                double ty = pm * vfield.at<float>(y, x * 2 + 1);
                Point2d pt = Point2d(x + 0.5, y + 0.5);
                while (++l < L) {
                    int px = static_cast<int>(ceil(pt.x));
                    int py = static_cast<int>(ceil(pt.y));
                    if (px < 0 || py < 0 || px >= width || py >= height) {
                        break;
                    }

                    double w = exp(-l * l / sigma_s);
                    for (int c = 0; c < dim; c++) {
                        out.at<float>(y, x*dim + c) += static_cast<float>(w * temp.at<float>(py, px*dim + c));
                    }
                    weight += w;

                    double vx = vfield.at<float>(py, px * 2 + 0);
                    double vy = vfield.at<float>(py, px * 2 + 1);
                    if (vx == 0.0f && vy == 0.0f) {
                        break;
                    }

                    double inner = vx * tx + vy * ty;
                    double sx = sign(inner) * vx;
                    double sy = sign(inner) * vy;
                    px = static_cast<int>(ceil(pt.x + 0.5 * sx));
                    py = static_cast<int>(ceil(pt.y + 0.5 * sy));
                    if (px < 0 || py < 0 || px >= width || py >= height) {
                        break;
                    }

                    vx = vfield.at<float>(py, px * 2 + 0);
                    vy = vfield.at<float>(py, px * 2 + 1);
                    inner = vx * tx + vy * ty;
                    tx = sign(inner) * vx;
                    ty = sign(inner) * vy;
                    pt.x += tx;
                    pt.y += ty;
                }
            }

            for (int c = 0; c < dim; c++) {
                if (weight != 0.0) {
                    out.at<float>(y, x*dim + c) = out.at<float>(y, x*dim + c) / static_cast<float>(weight);
                } else {
                    out.at<float>(y, x*dim + c) = temp.at<float>(y, x*dim + c);
                }
            }
        }
    }
}

void edgeFDoG(cv::InputArray input, cv::OutputArray output, const cv::Mat& vfield, const DoGParam& param) {
    cv::Mat  gray = input.getMat();
    cv::Mat& edge = output.getMatRef();

    const int width = gray.cols;
    const int height = gray.rows;
    const int dim = gray.channels();

    if (vfield.empty()) {
        cv::Mat angles;
        npr::calcVectorField(gray, angles, 11);
        npr::angle2vector(angles, vfield, 2.0);
    }

    const int ksize = 10;

    const double alpha = 2.0;
    const double sigma2 = 2.0 * param.sigma * param.sigma;
    cv::Mat g1;
    gaussWithFlow(gray, g1, vfield, ksize, 3.0, sigma2);

    const double ksigma = param.kappa * param.sigma;
    const double ksigma2 = 2.0 * ksigma * ksigma;
    cv::Mat g2;
    gaussWithFlow(gray, g2, vfield, ksize, 3.0, ksigma2);

    edge = cv::Mat(height, width, CV_32FC1);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double diff = 0.0;
            for (int c = 0; c < dim; c++) {
                diff += (g1.at<float>(y, x*dim + c) - param.tau * g2.at<float>(y, x*dim + c));
            }
            diff /= 3.0;

            if (diff > 0.0) {
                edge.at<float>(y, x) = 1.0f;
            } else {
                edge.at<float>(y, x) = 1.0f + static_cast<float>(tanh(param.phi * diff));
            }
        }
    }
}

}  // unnamed namespace

void edgeDoG(cv::InputArray image, cv::OutputArray edge, const DoGParam& param) {
    cv::Mat input = image.getMat();
    msg_assert(input.depth() == CV_32F && input.channels() == 1,
        "Input image must be single channel and floating-point-valued.");

    cv::Mat& outRef = edge.getMatRef();

    switch (param.dogType) {
    case EDGE_XDOG:
        edgeXDoG(input, outRef, param);
        break;

    case EDGE_FDOG:
        edgeFDoG(input, outRef, cv::Mat(), param);
        break;

    default:
        msg_assert(false, "Unknown DoG type is specified.");
    }
}

}  // namespace npr

}  // namespace lime

#endif  // SRC_NPR_NPREDGES_DETAIL_H_
