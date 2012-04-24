#include "swl/Config.h"
#include "swl/rnd_util/HmmWithUnivariateNormalMixtureObservations.h"
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/math/distributions/normal.hpp>  // for normal distribution
#include <boost/random/normal_distribution.hpp>
#include <boost/random/variate_generator.hpp>
#include <stdexcept>


#if defined(_DEBUG) && defined(__SWL_CONFIG__USE_DEBUG_NEW)
#include "swl/ResourceLeakageCheck.h"
#define new DEBUG_NEW
#endif


namespace swl {

HmmWithUnivariateNormalMixtureObservations::HmmWithUnivariateNormalMixtureObservations(const size_t K, const size_t C)
: base_type(K, 1), HmmWithMixtureObservations(C, K), mus_(K, C, 0.0), sigmas_(K, C, 0.0)  // 0-based index
{
}

HmmWithUnivariateNormalMixtureObservations::HmmWithUnivariateNormalMixtureObservations(const size_t K, const size_t C, const dvector_type &pi, const dmatrix_type &A, const dmatrix_type &alphas, const dmatrix_type &mus, const dmatrix_type &sigmas)
: base_type(K, 1, pi, A), HmmWithMixtureObservations(C, K, alphas), mus_(mus), sigmas_(sigmas)
{
}

HmmWithUnivariateNormalMixtureObservations::~HmmWithUnivariateNormalMixtureObservations()
{
}

void HmmWithUnivariateNormalMixtureObservations::doEstimateObservationDensityParametersInMStep(const size_t N, const unsigned int state, const dmatrix_type &observations, dmatrix_type &gamma, const double denominatorA)
{
	// reestimate symbol prob in each state

	size_t c, n;
	double denominator;

	// E-step
	// TODO [check] >> frequent memory reallocation may make trouble
	dmatrix_type zeta(N, C_, 0.0);
	{
		std::vector<boost::math::normal> pdfs;
		pdfs.reserve(C_);
		for (c = 0; c < C_; ++c)
			pdfs.push_back(boost::math::normal(mus_(state, c), sigmas_(state, c)));

		double val;
		for (n = 0; n < N; ++n)
		{
			const boost::numeric::ublas::matrix_row<const dmatrix_type> obs(observations, n);

			denominator = 0.0;
			for (c = 0; c < C_; ++c)
			{
				// FIXME [fix] >>
				//val = alphas_(state, c) * doEvaluateEmissionProbability(state, obs);  // error !!!
				val = alphas_(state, c) * boost::math::pdf(pdfs[c], obs[0]);

				zeta(n, c) = val;
				denominator += val;
			}

			val = 0.999 * gamma(n, state) / denominator;
			for (c = 0; c < C_; ++c)
				zeta(n, c) = 0.001 + val * zeta(n, c);
		}
	}

	// M-step
	denominator = denominatorA + gamma(N-1, state);
	double sumZeta;
	for (c = 0; c < C_; ++c)
	{
		sumZeta = 0.0;
		for (n = 0; n < N; ++n)
			sumZeta += zeta(n, c);
		alphas_(state, c) = 0.001 + 0.999 * sumZeta / denominator;

		//
		double &mu = mus_(state, c);
		mu = 0.0;
		for (n = 0; n < N; ++n)
			mu += zeta(n, c) * observations(n, 0);
		mu = 0.001 + 0.999 * mu / sumZeta;

		//
		double &sigma = sigmas_(state, c);
		sigma = 0.0;
		for (n = 0; n < N; ++n)
			sigma += zeta(n, c) * (observations(n, 0) - mu) * (observations(n, 0) - mu);
		sigma = 0.001 + 0.999 * sigma / sumZeta;
		assert(sigma > 0.0);
	}

	// POSTCONDITIONS [] >>
	//	-. all standard deviations have to be positive.
}

void HmmWithUnivariateNormalMixtureObservations::doEstimateObservationDensityParametersInMStep(const std::vector<size_t> &Ns, const unsigned int state, const std::vector<dmatrix_type> &observationSequences, const std::vector<dmatrix_type> &gammas, const size_t R, const double denominatorA)
{
	// reestimate symbol prob in each state

	size_t c, n, r;
	double denominator;

	// E-step
	// TODO [check] >> frequent memory reallocation may make trouble
	std::vector<dmatrix_type> zetas;
	zetas.reserve(R);
	for (r = 0; r < R; ++r)
		zetas.push_back(dmatrix_type(Ns[r], C_, 0.0));

	{
		std::vector<boost::math::normal> pdfs;
		pdfs.reserve(C_);
		for (c = 0; c < C_; ++c)
			pdfs.push_back(boost::math::normal(mus_(state, c), sigmas_(state, c)));

		double val;
		for (r = 0; r < R; ++r)
		{
			const dmatrix_type &gammar = gammas[r];
			dmatrix_type &zetar = zetas[r];

			for (n = 0; n < Ns[r]; ++n)
			{
				const boost::numeric::ublas::matrix_row<const dmatrix_type> obs(observationSequences[r], n);

				denominator = 0.0;
				for (c = 0; c < C_; ++c)
				{
					// FIXME [fix] >>
					//val = alphas_(state, c) * doEvaluateEmissionProbability(state, obs);  // error !!!
					val = alphas_(state, c) * boost::math::pdf(pdfs[c], obs[0]);

					zetar(n, c) = val;
					denominator += val;
				}

				val = 0.999 * gammar(n, state) / denominator;
				for (c = 0; c < C_; ++c)
					zetar(n, c) = 0.001 + val * zetar(n, c);
			}
		}
	}

	// M-step
	denominator = denominatorA;
	for (r = 0; r < R; ++r)
		denominator += gammas[r](Ns[r]-1, state);

	double sumZeta;
	const double factor = 0.999 / denominator;
	for (c = 0; c < C_; ++c)
	{
		sumZeta = 0.0;
		for (r = 0; r < R; ++r)
		{
			const dmatrix_type &zetar = zetas[r];

			for (n = 0; n < Ns[r]; ++n)
				sumZeta += zetar(n, c);
		}
		alphas_(state, c) = 0.001 + factor * sumZeta;

		//
		double &mu = mus_(state, c);
		mu = 0.0;
		for (r = 0; r < R; ++r)
		{
			const dmatrix_type &observationr = observationSequences[r];
			const dmatrix_type &zetar = zetas[r];

			for (n = 0; n < Ns[r]; ++n)
				mu += zetar(n, c) * observationr(n, 0);
		}
		mu = 0.001 + 0.999 * mu / sumZeta;

		//
		double &sigma = sigmas_(state, c);
		sigma = 0.0;
		for (r = 0; r < R; ++r)
		{
			const dmatrix_type &observationr = observationSequences[r];
			const dmatrix_type &zetar = zetas[r];

			for (n = 0; n < Ns[r]; ++n)
				sigma += zetar(n, c) * (observationr(n, 0) - mu) * (observationr(n, 0) - mu);
		}
		sigma = 0.001 + 0.999 * sigma / sumZeta;
		assert(sigma > 0.0);
	}

	// POSTCONDITIONS [] >>
	//	-. all standard deviations have to be positive.
}

double HmmWithUnivariateNormalMixtureObservations::doEvaluateEmissionProbability(const unsigned int state, const boost::numeric::ublas::matrix_row<const dmatrix_type> &observation) const
{
	double sum = 0.0;
	for (size_t c = 0; c < C_; ++c)
	{
		//boost::math::normal pdf;  // (default mean = zero, and standard deviation = unity)
		boost::math::normal pdf(mus_(state, c), sigmas_(state, c));

		sum += alphas_(state, c) * boost::math::pdf(pdf, observation[0]);
	}

	return sum;
}

void HmmWithUnivariateNormalMixtureObservations::doGenerateObservationsSymbol(const unsigned int state, boost::numeric::ublas::matrix_row<dmatrix_type> &observation, const unsigned int seed /*= (unsigned int)-1*/) const
{
	// PRECONDITIONS [] >>
	//	-. std::srand() had to be called before this function is called.

	const double prob = (double)std::rand() / RAND_MAX;

	double accum = 0.0;
	unsigned int component = (unsigned int)C_;
	for (size_t c = 0; c < C_; ++c)
	{
		accum += alphas_(state, c);
		if (prob < accum)
		{
			component = (unsigned int)c;
			break;
		}
	}

	// TODO [check] >>
	if ((unsigned int)C_ == component)
		component = (unsigned int)(C_ - 1);

	//
	typedef boost::normal_distribution<> distribution_type;
	typedef boost::variate_generator<base_generator_type &, distribution_type> generator_type;

	if ((unsigned int)-1 != seed)
		baseGenerator_.seed(seed);

	generator_type normal_gen(baseGenerator_, distribution_type(mus_(state, component), sigmas_(state, component)));
	observation[0] = normal_gen();
}

bool HmmWithUnivariateNormalMixtureObservations::doReadObservationDensity(std::istream &stream)
{
	if (1 != D_) return false;

	std::string dummy;
	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "univariate") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "univariate") != 0)
#endif
		return false;

	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "normal") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "normal") != 0)
#endif
		return false;

	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "mixture:") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "mixture:") != 0)
#endif
		return false;

	// TODO [check] >>
	size_t C;
	stream >> dummy >> C;  // the number of mixture components
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "C=") != 0 || C_ != C)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "C=") != 0 || C_ != C)
#endif
		return false;

	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "alpha:") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "alpha:") != 0)
#endif
		return false;

	size_t k, c;

	// K x C
	for (k = 0; k < K_; ++k)
		for (c = 0; c < C_; ++c)
			stream >> alphas_(k, c);

	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "mu:") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "mu:") != 0)
#endif
		return false;

	// K x C
	for (k = 0; k < K_; ++k)
		for (c = 0; c < C_; ++c)
			stream >> mus_(k, c);

	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "sigma:") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "sigma:") != 0)
#endif
		return false;

	// K x C
	for (k = 0; k < K_; ++k)
		for (c = 0; c < C_; ++c)
			stream >> sigmas_(k, c);

	return true;
}

bool HmmWithUnivariateNormalMixtureObservations::doWriteObservationDensity(std::ostream &stream) const
{
	stream << "univariate normal mixture:" << std::endl;

	stream << "C= " << C_ << std::endl;  // the number of mixture components

	size_t k, c;

	// K x C
	stream << "alpha:" << std::endl;
	for (k = 0; k < K_; ++k)
	{
		for (c = 0; c < C_; ++c)
			stream << alphas_(k, c) << ' ';
		stream << std::endl;
	}

	// K x C
	stream << "mu:" << std::endl;
	for (k = 0; k < K_; ++k)
	{
		for (c = 0; c < C_; ++c)
			stream << mus_(k, c) << ' ';
		stream << std::endl;
	}

	// K x C
	stream << "sigma:" << std::endl;
	for (k = 0; k < K_; ++k)
	{
		for (c = 0; c < C_; ++c)
			stream << sigmas_(k, c) << ' ';
		stream << std::endl;
	}

	return true;
}

void HmmWithUnivariateNormalMixtureObservations::doInitializeObservationDensity()
{
	// PRECONDITIONS [] >>
	//	-. std::srand() had to be called before this function is called.

	// FIXME [modify] >> lower & upper bounds have to be adjusted
	const double lb = -10000.0, ub = 10000.0;
	double sum;
	size_t c;
	for (size_t k = 0; k < K_; ++k)
	{
		sum = 0.0;
		for (c = 0; c < C_; ++c)
		{
			alphas_(k, c) = (double)std::rand() / RAND_MAX;
			sum += alphas_(k, c);

			mus_(k, c) = ((double)std::rand() / RAND_MAX) * (ub - lb) + lb;
			sigmas_(k, c) = ((double)std::rand() / RAND_MAX) * (ub - lb) + lb;
		}
		for (c = 0; c < C_; ++c)
			alphas_(k, c) /= sum;
	}
}

}  // namespace swl
