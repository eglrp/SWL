#if !defined(__SWL_RND_UTIL__HMM_WITH_MULTIVARIATE_GAUSSIAN_MIXTURE_OBSERVATIONS__H_)
#define __SWL_RND_UTIL__HMM_WITH_MULTIVARIATE_GAUSSIAN_MIXTURE_OBSERVATIONS__H_ 1


#include "swl/rnd_util/CDHMM.h"
#include "swl/rnd_util/HmmWithMixtureObservations.h"


namespace swl {

//--------------------------------------------------------------------------
// continuous density HMM with multivariate Gaussian mixture observation densities

class SWL_RND_UTIL_API HmmWithMultivariateGaussianMixtureObservations: public CDHMM, HmmWithMixtureObservations
{
public:
	typedef CDHMM base_type;
	//typedef HmmWithMixtureObservations base_type;

public:
	HmmWithMultivariateGaussianMixtureObservations(const size_t K, const size_t D, const size_t C);
	HmmWithMultivariateGaussianMixtureObservations(const size_t K, const size_t D, const size_t C, const std::vector<double> &pi, const boost::multi_array<double, 2> &A, const std::vector<double> &alpha, const boost::multi_array<double, 3> &mus, const boost::multi_array<double, 4> &sigmas);
	virtual ~HmmWithMultivariateGaussianMixtureObservations();

private:
	HmmWithMultivariateGaussianMixtureObservations(const HmmWithMultivariateGaussianMixtureObservations &rhs);
	HmmWithMultivariateGaussianMixtureObservations & operator=(const HmmWithMultivariateGaussianMixtureObservations &rhs);

public:
	/*virtual*/ bool estimateParameters(const size_t N, const boost::multi_array<double, 2> &observations, const double terminationTolerance, boost::multi_array<double, 2> &alpha, boost::multi_array<double, 2> &beta, boost::multi_array<double, 2> &gamma, size_t &numIteration, double &initLogProbability, double &finalLogProbability);

	//
	boost::multi_array<double, 3> & getMean()  {  return mus_;  }
	const boost::multi_array<double, 3> & getMean() const  {  return mus_;  }
	boost::multi_array<double, 4>& getCovarianceMatrix()  {  return  sigmas_;  }
	const boost::multi_array<double, 4> & getCovarianceMatrix() const  {  return  sigmas_;  }

protected:
	// if state == 0, hidden state = [ 1 0 0 ... 0 0 ]
	// if state == 1, hidden state = [ 0 1 0 ... 0 0 ]
	// ...
	// if state == N-1, hidden state = [ 0 0 0 ... 0 1 ]
	/*virtual*/ double evaluateEmissionProbability(const unsigned int state, const boost::multi_array<double, 2>::const_array_view<1>::type &observation) const;
	// if seed != -1, the seed value is set
	/*virtual*/ void generateObservationsSymbol(const unsigned int state, boost::multi_array<double, 2>::array_view<1>::type &observation, const unsigned int seed = (unsigned int)-1) const;

	//
	/*virtual*/ bool readObservationDensity(std::istream &stream);
	/*virtual*/ bool writeObservationDensity(std::ostream &stream) const;
	/*virtual*/ void initializeObservationDensity();

private:
	boost::multi_array<double, 3> mus_;  // the sets of mean vectors of each components in the multivariate Gaussian mixture distribution
	boost::multi_array<double, 4> sigmas_;  // the sets of covariance matrices of each components in the multivariate Gaussian mixture distribution
};

}  // namespace swl


#endif  // __SWL_RND_UTIL__HMM_WITH_MULTIVARIATE_GAUSSIAN_MIXTURE_OBSERVATIONS__H_
