#include "swl/Config.h"
#include "swl/rnd_util/HmmWithVonMisesMixtureObservations.h"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_roots.h>
#include <gsl/gsl_errno.h>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <stdexcept>


#if defined(_DEBUG) && defined(__SWL_CONFIG__USE_DEBUG_NEW)
#include "swl/ResourceLeakageCheck.h"
#define new DEBUG_NEW
#endif


namespace swl {
	
// [ref] swl/rnd_util/HmmWithVonMisesObservations.cpp
double kappa_objective_function(double x, void *params);
bool one_dim_root_finding_using_f(const double A, const double upper, double &kappa);

HmmWithVonMisesMixtureObservations::HmmWithVonMisesMixtureObservations(const size_t K, const size_t C)
: base_type(K, 1), HmmWithMixtureObservations(C, K), mus_(K, C), kappas_(K, C)  // 0-based index
{
}

HmmWithVonMisesMixtureObservations::HmmWithVonMisesMixtureObservations(const size_t K, const size_t C, const dvector_type &pi, const dmatrix_type &A, const dmatrix_type &alphas, const dmatrix_type &mus, const dmatrix_type &kappas)
: base_type(K, 1, pi, A), HmmWithMixtureObservations(C, K, alphas), mus_(mus), kappas_(kappas)
{
}

HmmWithVonMisesMixtureObservations::~HmmWithVonMisesMixtureObservations()
{
}

void HmmWithVonMisesMixtureObservations::doEstimateObservationDensityParametersInMStep(const size_t N, const unsigned int state, const dmatrix_type &observations, dmatrix_type &gamma, const double denominatorA)
{
	// reestimate symbol prob in each state

	size_t c, n;
	double numerator, denominator;

	// E-step
	// TODO [check] >> frequent memory reallocation may make trouble
	dmatrix_type zeta(N, C_, 0.0);
	{
		double val;
		for (n = 0; n < N; ++n)
		{
			const boost::numeric::ublas::matrix_row<const dmatrix_type> obs(observations, n);

			denominator = 0.0;
			for (c = 0; c < C_; ++c)
			{
				// FIXME [fix] >>
				//val = alphas_(state, c) * doEvaluateEmissionProbability(state, obs);  // error !!!
				val = alphas_(state, c) * 0.5 * std::exp(kappas_(state, c) * std::cos(obs[0] - mus_(state, c))) / (boost::math::constants::pi<double>() * boost::math::cyl_bessel_i(0.0, kappas_(state, c)));

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
		numerator = denominator = 0.0;
		for (n = 0; n < N; ++n)
		{
			numerator += zeta(n, c) * std::sin(observations(n, 0));
			denominator += zeta(n, c) * std::cos(observations(n, 0));
		}

		double &mu = mus_(state, c);

		// TODO [check] >> check the range of each mu, [0, 2 * pi)
		//mu = std::atan2(numerator, denominator);
		mu = std::atan2(numerator, denominator) + boost::math::constants::pi<double>();
		//mu = 0.001 + 0.999 * std::atan2(numerator, denominator) + boost::math::constants::pi<double>();

		//
		numerator = 0.0;
		for (n = 0; n < N; ++n)
			numerator += zeta(n, c) * std::cos(observations(n, 0) - mu);

		const double A = 0.001 + 0.999 * numerator / sumZeta;
		// FIXME [modify] >> upper bound has to be adjusted
		const double ub = 10000.0;  // kappa >= 0.0
		const bool retval = one_dim_root_finding_using_f(A, ub, kappas_(state, c));
		assert(retval);
	}

	// POSTCONDITIONS [] >>
	//	-. all concentration parameters have to be greater than or equal to 0.
}

void HmmWithVonMisesMixtureObservations::doEstimateObservationDensityParametersInMStep(const std::vector<size_t> &Ns, const unsigned int state, const std::vector<dmatrix_type> &observationSequences, const std::vector<dmatrix_type> &gammas, const size_t R, const double denominatorA)
{
	// reestimate symbol prob in each state

	size_t c, n, r;
	double numerator, denominator;

	// E-step
	// TODO [check] >> frequent memory reallocation may make trouble
	std::vector<dmatrix_type> zetas;
	zetas.reserve(R);
	for (r = 0; r < R; ++r)
		zetas.push_back(dmatrix_type(Ns[r], C_, 0.0));

	{
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
					val = alphas_(state, c) * 0.5 * std::exp(kappas_(state, c) * std::cos(obs[0] - mus_(state, c))) / (boost::math::constants::pi<double>() * boost::math::cyl_bessel_i(0.0, kappas_(state, c)));

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
		numerator = 0.0;
		for (r = 0; r < R; ++r)
		{
			const dmatrix_type &observationr = observationSequences[r];
			const dmatrix_type &zetar = zetas[r];

			numerator = denominator = 0.0;
			for (n = 0; n < Ns[r]; ++n)
			{
				numerator += zetar(n, c) * std::sin(observationr(n, 0));
				denominator += zetar(n, c) * std::cos(observationr(n, 0));
			}
		}

		double &mu = mus_(state, c);

		// TODO [check] >> check the range of each mu, [0, 2 * pi)
		//mu = std::atan2(numerator, denominator);
		mu = std::atan2(numerator, denominator) + boost::math::constants::pi<double>();
		//mu = 0.001 + 0.999 * std::atan2(numerator, denominator) + boost::math::constants::pi<double>();

		//
		numerator = 0.0;
		for (r = 0; r < R; ++r)
		{
			const dmatrix_type &observationr = observationSequences[r];
			const dmatrix_type &zetar = zetas[r];

			for (n = 0; n < Ns[r]; ++n)
				numerator += zetar(n, c) * std::cos(observationr(n, 0) - mu);
		}

		const double A = 0.001 + 0.999 * numerator / sumZeta;
		// FIXME [modify] >> upper bound has to be adjusted
		const double ub = 10000.0;  // kappa >= 0.0
		const bool retval = one_dim_root_finding_using_f(A, ub, kappas_(state, c));
		assert(retval);
	}

	// POSTCONDITIONS [] >>
	//	-. all concentration parameters have to be greater than or equal to 0.
}

double HmmWithVonMisesMixtureObservations::doEvaluateEmissionProbability(const unsigned int state, const boost::numeric::ublas::matrix_row<const dmatrix_type> &observation) const
{
	double sum = 0.0;
	for (size_t c = 0; c < C_; ++c)
	{
		// each observation are expressed as a random angle, 0 <= observation[0] < 2 * pi. [rad].
		sum += alphas_(state, c) * 0.5 * std::exp(kappas_(state, c) * std::cos(observation[0] - mus_(state, c))) / (boost::math::constants::pi<double>() * boost::math::cyl_bessel_i(0.0, kappas_(state, c)));
	}

	return sum;
}

void HmmWithVonMisesMixtureObservations::doGenerateObservationsSymbol(const unsigned int state, boost::numeric::ublas::matrix_row<dmatrix_type> &observation, const unsigned int seed /*= (unsigned int)-1*/) const
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
	double dx = 0.0, dy = 0.0;
	{
		if ((unsigned int)-1 != seed)
		{
			// random number generator algorithms
			gsl_rng_default = gsl_rng_mt19937;
			//gsl_rng_default = gsl_rng_taus;
			gsl_rng_default_seed = (unsigned long)std::time(NULL);
		}

		const gsl_rng_type *T = gsl_rng_default;
		gsl_rng *r = gsl_rng_alloc(T);

		// FIXME [fix] >> mus_(state, component) & kappas_(state, component) have to be reflected
		throw std::runtime_error("not correctly working");

		// dx^2 + dy^2 = 1
		gsl_ran_dir_2d(r, &dx, &dy);

		gsl_rng_free(r);
	}

	// TODO [check] >> check the range of each observation, [0, 2 * pi)
	//observation[0] = std::atan2(dy, dx);
	observation[0] = std::atan2(dy, dx) + boost::math::constants::pi<double>();
	//observation[0] = 0.001 + 0.999 * std::atan2(dy, dx) + boost::math::constants::pi<double>();
}

bool HmmWithVonMisesMixtureObservations::doReadObservationDensity(std::istream &stream)
{
	if (1 != D_) return false;

	std::string dummy;
	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "von") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "von") != 0)
#endif
		return false;

	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "Mises") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "Mises") != 0)
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
	if (strcasecmp(dummy.c_str(), "kappa:") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "kappa:") != 0)
#endif
		return false;

	// K x C
	for (k = 0; k < K_; ++k)
		for (c = 0; c < C_; ++c)
			stream >> kappas_(k, c);

	return true;
}

bool HmmWithVonMisesMixtureObservations::doWriteObservationDensity(std::ostream &stream) const
{
	stream << "von Mises mixture:" << std::endl;

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
	stream << "kappa:" << std::endl;
	for (k = 0; k < K_; ++k)
	{
		for (c = 0; c < C_; ++c)
			stream << kappas_(k, c) << ' ';
		stream << std::endl;
	}

	return true;
}

void HmmWithVonMisesMixtureObservations::doInitializeObservationDensity()
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
			kappas_(k, c) = ((double)std::rand() / RAND_MAX) * (ub - lb) + lb;
		}
		for (c = 0; c < C_; ++c)
			alphas_(k, c) /= sum;
	}
}

}  // namespace swl
