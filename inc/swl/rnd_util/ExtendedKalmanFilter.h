#if !defined(__SWL_RND_UTIL__EXTENDED_KALMAN_FILTER__H_)
#define __SWL_RND_UTIL__EXTENDED_KALMAN_FILTER__H_ 1


#include "swl/rnd_util/ExportRndUtil.h"
#include <gsl/gsl_blas.h>
#include <vector>


#ifdef __cplusplus
extern "C" {
#endif

//typedef struct gsl_vector;
//typedef struct gsl_matrix;
typedef struct gsl_permutation_struct gsl_permutation;

#ifdef __cplusplus
}
#endif


namespace swl {

//--------------------------------------------------------------------------
//

class SWL_RND_UTIL_API ExtendedKalmanFilter
{
public:
	//typedef ExtendedKalmanFilter base_type;

protected:
	ExtendedKalmanFilter(const gsl_vector *x0, const gsl_matrix *P0, const size_t stateDim, const size_t inputDim, const size_t outputDim);
public:
	virtual ~ExtendedKalmanFilter();

private:
	ExtendedKalmanFilter(const ExtendedKalmanFilter &rhs);
	ExtendedKalmanFilter & operator=(const ExtendedKalmanFilter &rhs);

public:
	// for continuous extended Kalman filter
	bool updateTime(const double time);
	bool updateMeasurement(const double time);
	// for discrete extended Kalman filter
	bool updateTime(const size_t step);
	bool updateMeasurement(const size_t step);

	const gsl_vector * getEstimatedState() const  {  return x_hat_;  }
	//const gsl_vector * getEstimatedMeasurement() const  {  return y_hat_;  }
	const gsl_matrix * getStateErrorCovarianceMatrix() const  {  return P_;  }
	const gsl_matrix * getKalmanGain() const  {  return K_;  }

	size_t getStateDim() const  {  return stateDim_;  }
	size_t getInputDim() const  {  return inputDim_;  }
	size_t getOutputDim() const  {  return outputDim_;  }

private:
	// for continuous Kalman filter
	virtual gsl_matrix * doGetSystemMatrix(const size_t step, const gsl_vector *state) const = 0;  // A
	// for discrete Kalman filter
	virtual gsl_matrix * doGetStateTransitionMatrix(const size_t step, const gsl_vector *state) const = 0;  // Phi
	virtual gsl_matrix * doGetOutputMatrix(const size_t step, const gsl_vector *state) const = 0;  // C == Cd

	//virtual gsl_matrix * doGetProcessNoiseCouplingMatrix(const size_t step) const = 0;  // W
	//virtual gsl_matrix * doGetMeasurementNoiseCouplingMatrix(const size_t step) const = 0;  // V
	virtual gsl_matrix * doGetProcessNoiseCovarianceMatrix(const size_t step) const = 0;  // Q or Qd = W * Q * W^T
	virtual gsl_matrix * doGetMeasurementNoiseCovarianceMatrix(const size_t step) const = 0;  // R or Rd = V * R * V^T

	virtual gsl_vector * doEvaluatePlantEquation(const size_t step, const gsl_vector *state) const = 0;  // f = f(k, x(k), u(k), 0)
	virtual gsl_vector * doEvaluateMeasurementEquation(const size_t step, const gsl_vector *state) const = 0;  // h = h(k, x(k), u(k), 0)

	virtual gsl_vector * doGetMeasurement(const size_t step, const gsl_vector *state) const = 0;

protected:
	// estimated state vector
	gsl_vector *x_hat_;
	// estimated measurement vector
	//gsl_vector *y_hat_;
	// state error covariance matrix
	gsl_matrix *P_;
	// Kalman gain
	gsl_matrix *K_;

	const size_t stateDim_;
	const size_t inputDim_;
	const size_t outputDim_;

private:
	// residual = y_tilde - y_hat
	gsl_vector *residual_;

	gsl_matrix *RR_;
	gsl_matrix *invRR_;
	gsl_matrix *PCt_;
	gsl_permutation *permutation_;

	// for temporary computation
	gsl_vector *v_;
	gsl_matrix *M_;
	gsl_matrix *M2_;
};

}  // namespace swl


#endif  // __SWL_RND_UTIL__EXTENDED_KALMAN_FILTER__H_
