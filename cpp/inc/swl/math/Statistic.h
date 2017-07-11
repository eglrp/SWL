#if !defined(__SWL_MATH__STATISTIC__H_)
#define __SWL_MATH__STATISTIC__H_ 1


#include "swl/math/ExportMath.h"
#include <Eigen/Core>
#include <vector>


namespace swl {

//-----------------------------------------------------------------------------------------
// Statistic.

struct SWL_MATH_API Statistic
{
public:
	///
	static double mean(const std::vector<double> &sample);
	static double standardDeviation(const std::vector<double> &sample, const double mean = 0.0);
	static double sampleStandardDeviation(const std::vector<double> &sample, const double mean = 0.0);
	static double skewness(const std::vector<double> &sample, const double mean = 0.0, const double sd = 1.0);
	static double kurtosis(const std::vector<double> &sample, const double mean = 0.0, const double sd = 1.0);

	///
	static double sampleVariance(const Eigen::VectorXd &D);
	static Eigen::VectorXd sampleVariance(const Eigen::MatrixXd &D);
	static Eigen::MatrixXd sampleCovarianceMatrix(const Eigen::MatrixXd &D);
};

}  // namespace swl


#endif  // __SWL_MATH__STATISTIC__H_
