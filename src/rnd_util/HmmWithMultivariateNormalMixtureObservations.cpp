#include "swl/Config.h"
#include "swl/rnd_util/HmmWithMultivariateNormalMixtureObservations.h"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/blas.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/math/constants/constants.hpp>
#include <ctime>
#include <stdexcept>


#if defined(_DEBUG) && defined(__SWL_CONFIG__USE_DEBUG_NEW)
#include "swl/ResourceLeakageCheck.h"
#define new DEBUG_NEW
#endif


namespace swl {

// [ref] swl/rnd_util/HmmWithMultivariateNormalObservations.cpp
double det_and_inv_by_lu(const boost::numeric::ublas::matrix<double> &m, boost::numeric::ublas::matrix<double> &inv);

HmmWithMultivariateNormalMixtureObservations::HmmWithMultivariateNormalMixtureObservations(const size_t K, const size_t D, const size_t C)
: base_type(K, D), HmmWithMixtureObservations(C, K), mus_(boost::extents[K][C]), sigmas_(boost::extents[K][C])  // 0-based index
{
	for (size_t k = 0; k < K; ++k)
		for (size_t c = 0; c < C; ++c)
		{
			mus_[k][c].resize(D);
			sigmas_[k][c].resize(D, D);
		}
}

HmmWithMultivariateNormalMixtureObservations::HmmWithMultivariateNormalMixtureObservations(const size_t K, const size_t D, const size_t C, const dvector_type &pi, const dmatrix_type &A, const dmatrix_type &alphas, const boost::multi_array<dvector_type, 2> &mus, const boost::multi_array<dmatrix_type, 2> &sigmas)
: base_type(K, D, pi, A), HmmWithMixtureObservations(C, K, alphas), mus_(mus), sigmas_(sigmas)
{
}

HmmWithMultivariateNormalMixtureObservations::~HmmWithMultivariateNormalMixtureObservations()
{
}

void HmmWithMultivariateNormalMixtureObservations::doEstimateObservationDensityParametersInMStep(const size_t N, const unsigned int state, const dmatrix_type &observations, dmatrix_type &gamma, const double denominatorA)
{
	// reestimate symbol prob in each state

	size_t c, n;
	double denominator;

	// E-step
	// TODO [check] >> frequent memory reallocation may make trouble
	dmatrix_type zeta(N, C_, 0.0);
	{
		double val;
		dmatrix_type inv(D_, D_);
		for (n = 0; n < N; ++n)
		{
			const boost::numeric::ublas::matrix_row<const dmatrix_type> obs(observations, n);

			denominator = 0.0;
			for (c = 0; c < C_; ++c)
			{
				// FIXME [fix] >>
				//val = alphas_(state, c) * doEvaluateEmissionProbability(state, obs);  // error !!!
				{
					const dmatrix_type &sigma = sigmas_[state][c];
					//if (0 == c)
					//	inv.resize(sigma.size1(), sigma.size2());
					const double det = det_and_inv_by_lu(sigma, inv);

					const dvector_type x_mu(obs - mus_[state][c]);
					val = alphas_(state, c) * std::exp(-0.5 * boost::numeric::ublas::inner_prod(x_mu, boost::numeric::ublas::prod(inv, x_mu))) / std::sqrt(std::pow(2.0 * boost::math::constants::pi<double>(), (double)D_) * det);
				}

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
		dvector_type &mu = mus_[state][c];
		mu.clear();
		for (n = 0; n < N; ++n)
			mu += zeta(n, c) * boost::numeric::ublas::matrix_row<const dmatrix_type>(observations, n);
		//mu = mu * (0.999 / sumZeta) + boost::numeric::ublas::scalar_vector<double>(mu.size(), 0.001);
		mu = mu * (0.999 / sumZeta) + boost::numeric::ublas::scalar_vector<double>(D_, 0.001);

		//
		dmatrix_type &sigma = sigmas_[state][c];
		sigma.clear();
		for (n = 0; n < N; ++n)
			boost::numeric::ublas::blas_2::sr(sigma, gamma(n, state), boost::numeric::ublas::matrix_row<const dmatrix_type>(observations, n) - mu);
		//sigma = sigma * (0.999 / sumZeta) + boost::numeric::ublas::scalar_matrix<double>(sigma.size1(), sigma.size2(), 0.001);
		sigma = sigma * (0.999 / sumZeta) + boost::numeric::ublas::scalar_matrix<double>(D_, D_, 0.001);
	}

	// POSTCONDITIONS [] >>
	//	-. all covariance matrices have to be positive definite.
}

void HmmWithMultivariateNormalMixtureObservations::doEstimateObservationDensityParametersInMStep(const std::vector<size_t> &Ns, const unsigned int state, const std::vector<dmatrix_type> &observationSequences, const std::vector<dmatrix_type> &gammas, const size_t R, const double denominatorA)
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
		double val;
		dmatrix_type inv(D_, D_);
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
					{
						const dmatrix_type &sigma = sigmas_[state][c];
						//if (0 == c)
						//	inv.resize(sigma.size1(), sigma.size2());
						const double det = det_and_inv_by_lu(sigma, inv);

						const dvector_type x_mu(obs - mus_[state][c]);
						val = alphas_(state, c) * std::exp(-0.5 * boost::numeric::ublas::inner_prod(x_mu, boost::numeric::ublas::prod(inv, x_mu))) / std::sqrt(std::pow(2.0 * boost::math::constants::pi<double>(), (double)D_) * det);
					}

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
	const double factor = 0.999 / denominator;

	double sumZeta;
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
		dvector_type &mu = mus_[state][c];
		mu.clear();
		for (r = 0; r < R; ++r)
		{
			const dmatrix_type &observationr = observationSequences[r];
			const dmatrix_type &zetar = zetas[r];

			for (n = 0; n < Ns[r]; ++n)
				mu += zetar(n, c) * boost::numeric::ublas::matrix_row<const dmatrix_type>(observationr, n);
		}
		//mu = mu * (0.999 / sumZeta) + boost::numeric::ublas::scalar_vector<double>(mu.size(), 0.001);
		mu = mu * (0.999 / sumZeta) + boost::numeric::ublas::scalar_vector<double>(D_, 0.001);

		//
		dmatrix_type &sigma = sigmas_[state][c];
		sigma.clear();
		for (r = 0; r < R; ++r)
		{
			const dmatrix_type &observationr = observationSequences[r];
			const dmatrix_type &zetar = zetas[r];

			for (n = 0; n < Ns[r]; ++n)
				boost::numeric::ublas::blas_2::sr(sigma, zetar(n, c), boost::numeric::ublas::matrix_row<const dmatrix_type>(observationr, n) - mu);
		}
		//sigma = sigma * (0.999 / sumZeta) + boost::numeric::ublas::scalar_matrix<double>(sigma.size1(), sigma.size2(), 0.001);
		sigma = sigma * (0.999 / sumZeta) + boost::numeric::ublas::scalar_matrix<double>(D_, D_, 0.001);
	}

	// POSTCONDITIONS [] >>
	//	-. all standard deviations have to be positive.
}

double HmmWithMultivariateNormalMixtureObservations::doEvaluateEmissionProbability(const unsigned int state, const boost::numeric::ublas::matrix_row<const dmatrix_type> &observation) const
{
	double sum = 0.0;
	dmatrix_type inv(D_, D_, 0.0);
	for (size_t c = 0; c < C_; ++c)
	{
		const dmatrix_type &sigma = sigmas_[state][c];
		//if (0 == c)
		//	inv.resize(sigma.size1(), sigma.size2());
		const double det = det_and_inv_by_lu(sigma, inv);

		const dvector_type x_mu(observation - mus_[state][c]);
		sum += alphas_(state, c) * std::exp(-0.5 * boost::numeric::ublas::inner_prod(x_mu, boost::numeric::ublas::prod(inv, x_mu))) / std::sqrt(std::pow(2.0 * boost::math::constants::pi<double>(), (double)D_) * det);
	}

	return sum;

}

void HmmWithMultivariateNormalMixtureObservations::doGenerateObservationsSymbol(const unsigned int state, boost::numeric::ublas::matrix_row<dmatrix_type> &observation, const unsigned int seed /*= (unsigned int)-1*/) const
{
	// PRECONDITIONS [] >>
	//	-. std::srand() had to be called before this function is called.

	// bivariate normal distribution
	if (2 == D_)
	{
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
		if ((unsigned int)-1 != seed)
		{
			// random number generator algorithms
			gsl_rng_default = gsl_rng_mt19937;
			//gsl_rng_default = gsl_rng_taus;
			gsl_rng_default_seed = (unsigned long)std::time(NULL);
		}

		const gsl_rng_type *T = gsl_rng_default;
		gsl_rng *r = gsl_rng_alloc(T);

		//
		const dvector_type &mu = mus_[state][component];
		const dmatrix_type &sigma = sigmas_[state][component];

		const double sigma_x = sigma(0, 0);
		const double sigma_y = sigma(1, 1);
		const double rho = sigma(0, 1) / std::sqrt(sigma_x * sigma_y);  // correlation coefficient

		double x = 0.0, y = 0.0;
		gsl_ran_bivariate_gaussian(r, sigma_x, sigma_y, rho, &x, &y);

		gsl_rng_free(r);

		observation[0] = mu[0] + x;
		observation[1] = mu[1] + y;
	}
	else
	{
		throw std::runtime_error("not yet implemented");
	}
}

bool HmmWithMultivariateNormalMixtureObservations::doReadObservationDensity(std::istream &stream)
{
	std::string dummy;
	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "multivariate") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "multivariate") != 0)
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

	size_t i, k, c, d;

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

	// K x C x D
	for (k = 0; k < K_; ++k)
		for (c = 0; c < C_; ++c)
		{
			dvector_type &mu = mus_[k][c];

			for (d = 0; d < D_; ++d)
				stream >> mu[d];
		}

	stream >> dummy;
#if defined(__GNUC__)
	if (strcasecmp(dummy.c_str(), "sigma:") != 0)
#elif defined(_MSC_VER)
	if (_stricmp(dummy.c_str(), "sigma:") != 0)
#endif
		return false;

	// K x C x (D * D)
	for (k = 0; k < K_; ++k)
		for (c = 0; c < C_; ++c)
		{
			dmatrix_type &sigma = sigmas_[k][c];

			for (d = 0; d < D_; ++d)
				for (i = 0; i < D_; ++i)
					stream >> sigma(d, i);
		}

	return true;
}

bool HmmWithMultivariateNormalMixtureObservations::doWriteObservationDensity(std::ostream &stream) const
{
	stream << "multivariate normal mixture:" << std::endl;

	stream << "C= " << C_ << std::endl;  // the number of mixture components

	size_t i, k, c, d;

	// K x C
	stream << "alpha:" << std::endl;
	for (k = 0; k < K_; ++k)
	{
		for (c = 0; c < C_; ++c)
			stream << alphas_(k, c) << ' ';
		stream << std::endl;
	}

	// K x C x D
	stream << "mu:" << std::endl;
	for (k = 0; k < K_; ++k)
	{
		for (c = 0; c < C_; ++c)
		{
			const dvector_type &mu = mus_[k][c];

			for (d = 0; d < D_; ++d)
				stream << mu[d] << ' ';
			stream << std::endl;
		}
		stream << std::endl;
	}

	// K x C x (D * D)
	stream << "sigma:" << std::endl;
	for (k = 0; k < K_; ++k)
	{
		for (c = 0; c < C_; ++c)
		{
			const dmatrix_type &sigma = sigmas_[k][c];

			for (d = 0; d < D_; ++d)
			{
				for (i = 0; i < D_; ++i)
					stream << sigma(d, i) << ' ';
				stream << "  ";
			}
			stream << std::endl;
		}
		stream << std::endl;
	}

	return true;
}

void HmmWithMultivariateNormalMixtureObservations::doInitializeObservationDensity()
{
	// PRECONDITIONS [] >>
	//	-. std::srand() had to be called before this function is called.

	// FIXME [modify] >> lower & upper bounds have to be adjusted
	const double lb = -10000.0, ub = 10000.0;
	double sum;
	size_t c, d, i;
	for (size_t k = 0; k < K_; ++k)
	{
		sum = 0.0;
		for (c = 0; c < C_; ++c)
		{
			alphas_(k, c) = (double)std::rand() / RAND_MAX;
			sum += alphas_(k, c);

			dvector_type &mu = mus_[k][c];
			dmatrix_type &sigma = sigmas_[k][c];
			for (d = 0; d < D_; ++d)
			{
				mu[d] = ((double)std::rand() / RAND_MAX) * (ub - lb) + lb;
				for (i = 0; i < D_; ++i)
					sigma(d, i) = ((double)std::rand() / RAND_MAX) * (ub - lb) + lb;
			}
		}
		for (c = 0; c < C_; ++c)
			alphas_(k, c) /= sum;
	}
}

}  // namespace swl
