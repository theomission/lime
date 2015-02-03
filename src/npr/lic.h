#ifndef _NPR_LIC_H_
#define _NPR_LIC_H_

#include <cmath>

#include "../core/Point.hpp"

namespace lime {

	namespace npr {
		enum LicAlgo {
			LIC_CLASSIC = 0x01,
			LIC_EULARIAN,
			LIC_RUNGE_KUTTA
		};

		/* Visualize a vector field using line integral convolusion (LIC)
		* @param[out] out: output image
		* @param[in] img: input image to be convoluted
		* @param[in] tangent: cv::Mat of CV_32FC2 depth which specifies a tangent direction to each pixel
		* @param[in] L: convolution length
		* @param[in] algo_type: algorithm type used for LIC
		*   algorighms:
		*      NPR_LIC_CLASSIC: slow but outputs beautiful vector field
		*      NPR_LIC_EULARIAN: fast and stable algorithm (default)
		*      NPR_LIC_RUNGE_KUTTA: second-order line integration
		*/
		inline void lic(cv::OutputArray out, cv::InputArray img, const cv::Mat& tangent, int L, LicAlgo algo_type = LIC_EULARIAN);

		inline void angle2vector(cv::InputArray angle, cv::OutputArray vfield, double scale = 1.0);

		inline void vector2angle(cv::InputArray vfield, cv::OutputArray angle);

	} /* namespace npr */

} /* namespace lime */

#include "lic_detail.h"

#endif /* _NPR_LIC_H_ */