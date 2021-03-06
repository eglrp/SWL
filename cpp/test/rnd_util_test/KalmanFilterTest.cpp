//#include "stdafx.h"
#include "swl/Config.h"
#include "swl/rnd_util/DiscreteLinearStochasticSystem.h"
#include "swl/rnd_util/KalmanFilter.h"
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/variate_generator.hpp>
#include <fstream>
#include <cmath>
#include <ctime>
#include <cassert>


#if defined(_DEBUG) && defined(__SWL_CONFIG__USE_DEBUG_NEW)
#include "swl/ResourceLeakageCheck.h"
#define new DEBUG_NEW
#endif


void output_data_to_file(std::ostream &stream, const std::string &variable_name, const std::vector<double> &data);

namespace {
namespace local {

// "Kalman Filtering: Theory and Practice Using MATLAB" Example 4.1 (pp. 123)
class SimpleLinearSystem: public swl::DiscreteLinearStochasticSystem
{
public:
	typedef swl::DiscreteLinearStochasticSystem base_type;

private:
	static const double Ts;
	static const double Q;
	static const double R;

public:
	SimpleLinearSystem(const size_t stateDim, const size_t inputDim, const size_t outputDim)
	: base_type(stateDim, inputDim, outputDim, (size_t)-1, (size_t)-1),
	  Phi_(NULL), C_(NULL), /*W_(NULL), V_(NULL),*/ Qd_(NULL), Rd_(NULL), Bu_(NULL), Du_(NULL), y_tilde_(NULL)
	{
		// Phi = exp(A * Ts) = [ 1 ]
		Phi_ = gsl_matrix_alloc(stateDim_, stateDim_);
		gsl_matrix_set(Phi_, 0, 0, 1.0);

		// C = [ 1 ]
		C_ = gsl_matrix_alloc(outputDim_, stateDim_);
		gsl_matrix_set(C_, 0, 0, 1.0);

		// W = [ 1 ]
		//W_ = gsl_matrix_alloc(stateDim_, inputDim_);
		//gsl_matrix_set_identity(W_);

		// V = [ 1 ]
		//V_ = gsl_matrix_alloc(outputDim_, outputDim_);
		//gsl_matrix_set_identity(V_);

		// Qd = W * Q * W^T
		Qd_ = gsl_matrix_alloc(stateDim_, stateDim_);
		gsl_matrix_set(Qd_, 0, 0, Q);

		// Rd = V * R * V^T
		Rd_ = gsl_matrix_alloc(outputDim_, outputDim_);
		gsl_matrix_set(Rd_, 0, 0, R);

		// no control input
		Bu_ = gsl_vector_alloc(stateDim_);
		gsl_vector_set_zero(Bu_);

		// no measurement input
		Du_ = gsl_vector_alloc(outputDim_);
		gsl_vector_set_zero(Du_);

		//
		y_tilde_ = gsl_vector_alloc(outputDim_);
		gsl_vector_set_zero(y_tilde_);
	}
	~SimpleLinearSystem()
	{
		gsl_matrix_free(Phi_);  Phi_ = NULL;
		gsl_matrix_free(C_);  C_ = NULL;
		//gsl_matrix_free(W_);  W_ = NULL;
		//gsl_matrix_free(V_);  V_ = NULL;
		gsl_matrix_free(Qd_);  Qd_ = NULL;
		gsl_matrix_free(Rd_);  Rd_ = NULL;

		gsl_vector_free(Bu_);  Bu_ = NULL;
		gsl_vector_free(Du_);  Du_ = NULL;

		gsl_vector_free(y_tilde_);  y_tilde_ = NULL;
	}

private:
	SimpleLinearSystem(const SimpleLinearSystem &rhs);
	SimpleLinearSystem & operator=(const SimpleLinearSystem &rhs);

public:
	// the stochastic differential equation: x(k+1) = Phi(k) * x(k) + Bd(k) * u(k) + W(k) * w(k)
	/*virtual*/ gsl_matrix * getStateTransitionMatrix(const size_t step, const gsl_vector *state) const  {  return Phi_;  }  // Phi(k) = exp(A(k) * Ts)
	/*virtual*/ gsl_vector * getControlInput(const size_t step, const gsl_vector *state) const  // Bu(k) = Bd(k) * u(k)
	{  throw std::runtime_error("this function doesn't have to be called");  }
	///*virtual*/ gsl_matrix * getProcessNoiseCouplingMatrix(const size_t step) const  {  return W_;  }  // W(k)

	// the observation equation: y(k) = Cd(k) * x(k) + Dd(k) * u(k) + V(k) * v(k)
	/*virtual*/ gsl_matrix * getOutputMatrix(const size_t step, const gsl_vector *state) const  {  return C_;  }  // Cd(k) (C == Cd)
	/*virtual*/ gsl_vector * getMeasurementInput(const size_t step, const gsl_vector *state) const  // Du(k) = D(k) * u(k) (D == Dd)
	{  throw std::runtime_error("this function doesn't have to be called");  }
	///*virtual*/ gsl_matrix * getMeasurementNoiseCouplingMatrix(const size_t step) const  {  return V_;  }  // V(k)

	// noise covariance matrices
	/*virtual*/ gsl_matrix * getProcessNoiseCovarianceMatrix(const size_t step) const  {  return Qd_;  }  // Qd = W * Q * W^T, but not Q
	/*virtual*/ gsl_matrix * getMeasurementNoiseCovarianceMatrix(const size_t step) const  {  return Rd_;  }  // Rd = V * R * V^T, but not R

	//
	gsl_vector * calculateControlInput(const size_t step) const  {  return Bu_;  }  // Bu(k) = Bd(k) * u(k)
	gsl_vector * calculateMeasurementInput(const size_t step) const  {  return Du_;  }  // Du(k) = D(k) * u(k) (D == Dd)

	// actual measurement
	gsl_vector * simulateMeasurement(const size_t step, const gsl_vector *state) const
	{
#if 1  // 1-based time step
		if (1 == step) gsl_vector_set(y_tilde_, 0, 2.0);
		else if (2 == step) gsl_vector_set(y_tilde_, 0, 3.0);
#else  // 0-based time step
		if (0 == step) gsl_vector_set(y_tilde_, 0, 2.0);
		else if (1 == step) gsl_vector_set(y_tilde_, 0, 3.0);
#endif
		else throw std::runtime_error("undefined measurement");
		return y_tilde_;
	}

private:
	gsl_matrix *Phi_;
	gsl_matrix *C_;  // Cd = C
	//gsl_matrix *W_;
	//gsl_matrix *V_;
	gsl_matrix *Qd_;
	gsl_matrix *Rd_;

	// control input: Bu = Bd * u where Bd = integrate(exp(A * t), {t, 0, Ts}) * B or A^-1 * (Ad - I) * B if A is nonsingular
	gsl_vector *Bu_;
	// measurement input: Du = Dd * u where Dd = D
	gsl_vector *Du_;

	// actual measurement
	gsl_vector *y_tilde_;
};

/*static*/ const double SimpleLinearSystem::Ts = 1.0;
/*static*/ const double SimpleLinearSystem::Q = 1.0;
/*static*/ const double SimpleLinearSystem::R = 2.0;

// "The Global Positioning System and Inertial Navigation" Example (pp. 110)
class AidedINSSystem: public swl::DiscreteLinearStochasticSystem
{
public:
	typedef swl::DiscreteLinearStochasticSystem base_type;

private:
	static const double Ts;
	static const double Rv;
	static const double Qb;
	static const double Rp;

public:
	AidedINSSystem(const size_t stateDim, const size_t inputDim, const size_t outputDim)
	: base_type(stateDim, inputDim, outputDim, (size_t)-1, (size_t)-1),
	  Phi_(NULL), C_(NULL), /*W_(NULL), V_(NULL),*/ Qd_(NULL), Rd_(NULL), Bu_(NULL), Du_(NULL), y_tilde_(NULL)
	{
		// Phi = exp(A * Ts) = [ 1 Ts Ts^2/2 ; 0 1 Ts ; 0 0 1 ]
		Phi_ = gsl_matrix_alloc(stateDim_, stateDim_);
		gsl_matrix_set_identity(Phi_);
		gsl_matrix_set(Phi_, 0, 1, Ts);  gsl_matrix_set(Phi_, 0, 2, 0.5*Ts*Ts);  gsl_matrix_set(Phi_, 1, 2, Ts);

		// C = [ 1 0 0 ]
		C_ = gsl_matrix_alloc(outputDim_, stateDim_);
		gsl_matrix_set(C_, 0, 0, 1.0);  gsl_matrix_set(C_, 0, 1, 0.0);  gsl_matrix_set(C_, 0, 2, 0.0);

		// W = [ 0 0 ; 1 0 ; 0 1 ]
		//W_ = gsl_matrix_alloc(stateDim_, inputDim_);
		//gsl_matrix_set_zero(W_);
		//gsl_matrix_set(W_, 1, 0, 1.0);  gsl_matrix_set(W_, 2, 1, 1.0);

		// V = [ 1 ]
		//V_ = gsl_matrix_alloc(outputDim_, outputDim_);
		//gsl_matrix_set_identity(V_);

		// Qd = W * Q * W^T
		Qd_ = gsl_matrix_alloc(stateDim_, stateDim_);
#if 0
		// Q, but not Qd
		gsl_matrix_set_zero(Qd_);
		gsl_matrix_set(Qd_, 0, 0, Rv);  gsl_matrix_set(Qd_, 1, 1, Qb);
#else
		// continuous system  -->  discrete system
		// Qd = integrate(Phi(t) * W(t) * Q(t) * W(t)^T * Phi(t)^T, {t, 0, Ts})
		//	Q = [ Rv 0 ; 0 Qb ]
		//	W = [ 0 0 ; 1 0 ; 0 1 ]
		//	Qd = [ (Qb*Ts^5)/20 + (Rv*Ts^3)/3, (Qb*Ts^4)/8 + (Rv*Ts^2)/2, (Qb*Ts^3)/6 ]
		//	     [  (Qb*Ts^4)/8 + (Rv*Ts^2)/2,       (Qb*Ts^3)/3 + Rv*Ts, (Qb*Ts^2)/2 ]
		//	     [                (Qb*Ts^3)/6,               (Qb*Ts^2)/2,       Qb*Ts ]
		gsl_matrix_set(Qd_, 0, 0, Rv/3+Qb/20);  gsl_matrix_set(Qd_, 0, 1, Rv/2+Qb/8);  gsl_matrix_set(Qd_, 0, 2, Qb/6);
		gsl_matrix_set(Qd_, 1, 0, Rv/2+Qb/8);  gsl_matrix_set(Qd_, 1, 1, Rv+Qb/3);  gsl_matrix_set(Qd_, 1, 2, Qb/2);
		gsl_matrix_set(Qd_, 2, 0, Qb/6);  gsl_matrix_set(Qd_, 2, 1, Qb/2);  gsl_matrix_set(Qd_, 2, 2, Qb);
#endif

		// Rd = V * R * V^T
		Rd_ = gsl_matrix_alloc(outputDim_, outputDim_);
		gsl_matrix_set_all(Rd_, Rp);

		// no control input
		Bu_ = gsl_vector_alloc(stateDim_);
		gsl_vector_set_zero(Bu_);

		// no measurement input
		Du_ = gsl_vector_alloc(outputDim_);
		gsl_vector_set_zero(Du_);

		//
		y_tilde_ = gsl_vector_alloc(outputDim_);
		gsl_vector_set_zero(y_tilde_);
	}
	~AidedINSSystem()
	{
		gsl_matrix_free(Phi_);  Phi_ = NULL;
		gsl_matrix_free(C_);  C_ = NULL;
		//gsl_matrix_free(W_);  W_ = NULL;
		//gsl_matrix_free(V_);  V_ = NULL;
		gsl_matrix_free(Qd_);  Qd_ = NULL;
		gsl_matrix_free(Rd_);  Rd_ = NULL;

		gsl_vector_free(Bu_);  Bu_ = NULL;
		gsl_vector_free(Du_);  Du_ = NULL;

		gsl_vector_free(y_tilde_);  y_tilde_ = NULL;
	}

private:
	AidedINSSystem(const AidedINSSystem &rhs);
	AidedINSSystem & operator=(const AidedINSSystem &rhs);

public:
	// the stochastic differential equation: x(k+1) = Phi(k) * x(k) + Bd(k) * u(k) + W(k) * w(k)
	/*virtual*/ gsl_matrix * getStateTransitionMatrix(const size_t step, const gsl_vector *state) const  {  return Phi_;  }  // Phi(k) = exp(A(k) * Ts)
	/*virtual*/ gsl_vector * getControlInput(const size_t step, const gsl_vector *state) const  // Bu(k) = Bd(k) * u(k)
	{  throw std::runtime_error("this function doesn't have to be called");  }
	///*virtual*/ gsl_matrix * getProcessNoiseCouplingMatrix(const size_t step) const  {  return W_;  }  // W(k)

	// the observation equation: y(k) = Cd(k) * x(k) + Dd(k) * u(k) + V(k) * v(k)
	/*virtual*/ gsl_matrix * getOutputMatrix(const size_t step, const gsl_vector *state) const  {  return C_;  }  // Cd(k) (C == Cd)
	/*virtual*/ gsl_vector * getMeasurementInput(const size_t step, const gsl_vector *state) const  // Du(k) = D(k) * u(k) (D == Dd)
	{  throw std::runtime_error("this function doesn't have to be called");  }
	///*virtual*/ gsl_matrix * getMeasurementNoiseCouplingMatrix(const size_t step) const  {  return V_;  }  // V(k)

	// noise covariance matrices
	/*virtual*/ gsl_matrix * getProcessNoiseCovarianceMatrix(const size_t step) const  {  return Qd_;  }  // Qd = W * Q * W^T, but not Q
	/*virtual*/ gsl_matrix * getMeasurementNoiseCovarianceMatrix(const size_t step) const  {  return Rd_;  }  // Rd = V * R * V^T, but not R

	//
	gsl_vector * calculateControlInput(const size_t step) const  {  return Bu_;  }  // Bu(k) = Bd(k) * u(k)
	gsl_vector * calculateMeasurementInput(const size_t step) const  {  return Du_;  }  // Du(k) = D(k) * u(k) (D == Dd)

	// actual measurement
	gsl_vector * simulateMeasurement(const size_t step, const gsl_vector *state) const
	{
#if 0
		gsl_vector_set(y_tilde_, 0, gsl_vector_get(state, 0));  // measurement (no noise)
#elif 0
		typedef boost::minstd_rand base_generator_type;
		typedef boost::uniform_real<> distribution_type;
		typedef boost::variate_generator<base_generator_type &, distribution_type> generator_type;

		base_generator_type generator(static_cast<unsigned int>(std::time(0)));
		//generator.seed(static_cast<unsigned int>(std::time(0)));
		generator_type uni_gen(generator, distribution_type(-100, 101));

		gsl_vector_set(y_tilde_, 0, gsl_vector_get(state, 0) + uni_gen());  // measurement (white noise)
#else
		typedef boost::minstd_rand base_generator_type;
		typedef boost::normal_distribution<> distribution_type;
		typedef boost::variate_generator<base_generator_type &, distribution_type> generator_type;

		base_generator_type generator(static_cast<unsigned int>(std::time(0)));
		//generator.seed(static_cast<unsigned int>(std::time(0)));
		generator_type normal_gen(generator, distribution_type(0.0, std::sqrt(Rp)));

		gsl_vector_set(y_tilde_, 0, gsl_vector_get(state, 0) + normal_gen());  // measurement (normal noise)
#endif

		return y_tilde_;
	}

private:
	gsl_matrix *Phi_;
	gsl_matrix *C_;  // Cd = C
	//gsl_matrix *W_;
	//gsl_matrix *V_;
	gsl_matrix *Qd_;
	gsl_matrix *Rd_;

	// control input: Bu = Bd * u where Bd = integrate(exp(A * t), {t, 0, Ts}) * B or A^-1 * (Ad - I) * B if A is nonsingular
	gsl_vector *Bu_;
	// measurement input: Du = Dd * u where Dd = D
	gsl_vector *Du_;

	// actual measurement
	gsl_vector *y_tilde_;
};

/*static*/ const double AidedINSSystem::Ts = 1.0;
/*static*/ const double AidedINSSystem::Rv = 2.5e-3;
/*static*/ const double AidedINSSystem::Qb = 1.0e-6;
/*static*/ const double AidedINSSystem::Rp = 3.0;

// "Kalman Filtering: Theory and Practice Using MATLAB" Example 4.3 (pp. 156)
//	1) continuous ==> discrete
//	2) driving force exits
class LinearMassStringDamperSystem: public swl::DiscreteLinearStochasticSystem
{
public:
	typedef swl::DiscreteLinearStochasticSystem base_type;

private:
	static const double Ts;
	static const double zeta;
	static const double omega;
	static const double Q;
	static const double R;
	static const double Fd;  // driving force

public:
	LinearMassStringDamperSystem(const size_t stateDim, const size_t inputDim, const size_t outputDim)
	: base_type(stateDim, inputDim, outputDim, (size_t)-1, (size_t)-1),
	  Phi_(NULL), C_(NULL), /*W_(NULL), V_(NULL),*/ Qd_(NULL), Rd_(NULL), Bu_(NULL), Du_(NULL), y_tilde_(NULL)
	{
		const double lambda = std::exp(-Ts * omega * zeta);
		const double psi    = 1.0 - zeta * zeta;
		const double xi     = std::sqrt(psi);
		const double theta  = xi * omega * Ts;
		const double c = std::cos(theta);
		const double s = std::sin(theta);

		// Phi = exp(A * Ts)
		//	A = [ 0 1 ; -omega^2 -2*zeta*omega ]
		Phi_ = gsl_matrix_alloc(stateDim_, stateDim_);
#if 1
		gsl_matrix_set(Phi_, 0, 0, lambda*c + zeta*s/xi);  gsl_matrix_set(Phi_, 0, 1, lambda*s/(omega*xi));
		gsl_matrix_set(Phi_, 1, 0, -omega*lambda*s/xi);  gsl_matrix_set(Phi_, 1, 1, lambda*c - zeta*s/xi);
#else
		// my calculation result
		gsl_matrix_set(Phi_, 0, 0, lambda*(c+zeta*s/xi));  gsl_matrix_set(Phi_, 0, 1, lambda*s/(omega*xi));
		gsl_matrix_set(Phi_, 1, 0, -omega*lambda*s/xi);  gsl_matrix_set(Phi_, 1, 1, lambda*(c-zeta*s/xi));
#endif

		// C = [ 1 0 ]
		C_ = gsl_matrix_alloc(outputDim_, stateDim_);
		gsl_matrix_set(C_, 0, 0, 1.0);  gsl_matrix_set(C_, 0, 1, 0.0);

		// W = [ 0 ; 1 ]
		//W_ = gsl_matrix_alloc(stateDim_, inputDim_);
		//gsl_matrix_set(W_, 0, 0, 0.0);  gsl_matrix_set(W_, 1, 0, 1.0);

		// V = [ 1 ]
		//V_ = gsl_matrix_alloc(outputDim_, outputDim_);
		//gsl_matrix_set_identity(V_);

		// Qd = W * Q * W^T
		Qd_ = gsl_matrix_alloc(stateDim_, stateDim_);
		const double l1  = zeta * zeta;
		const double l4  = std::exp(-2.0 * omega * zeta * Ts);
		const double l6  = std::sqrt(1-l1);
		const double l8  = l6 * omega * Ts;
		const double l9  = std::cos(2.0 * l8);
		const double l11 = l4 * l9 * l1;
		const double l15 = l4 * std::sin(2.0 * l8) * l6 * zeta;
		const double l19 = 1.0 / (l1 - 1.0);
		const double l20 = omega * omega;
		const double l24 = 1.0 / zeta;
		const double l32 = l4 * (l9 - 1.0) * l19 / l20;

		gsl_matrix_set(Qd_, 0, 0, 0.25*Q * (l1-l11+l15-1+l4)*l19*l24);  gsl_matrix_set(Qd_, 0, 1, 0.25*Q * l32);
		gsl_matrix_set(Qd_, 1, 0, 0.25*Q * l32);  gsl_matrix_set(Qd_, 1, 1, 0.25*Q * (l1-l11-l15-1+l4)*l19*l24/omega);

		// Rd = V * R * V^T
		Rd_ = gsl_matrix_alloc(outputDim_, outputDim_);
		gsl_matrix_set_all(Rd_, R);

		// driving force: Bu = Bd * u where u(t) = 1
		//	Bd = integrate(exp(A * t), {t, 0, Ts}) * B or A^-1 * (Ad - I) * B if A is nonsingular
		//	Ad = Phi = exp(A * Ts)
		//	B = [ 0 ; Fd ]
		Bu_ = gsl_vector_alloc(stateDim_);
#if 0
		gsl_vector_set(Bu_, 0, Fd * (1.0 - lambda*(c-zeta*s*xi/psi)) / (omega*omega));  gsl_vector_set(Bu_, 1, Fd * lambda*s / (omega*xi));
#else
		gsl_vector_set(Bu_, 0, Fd * (1.0 - lambda*(c-zeta*s/xi)) / (omega*omega));  gsl_vector_set(Bu_, 1, Fd * lambda*s / (omega*xi));
#endif

		// no measurement input
		Du_ = gsl_vector_alloc(outputDim_);
		gsl_vector_set_zero(Du_);

		//
		y_tilde_ = gsl_vector_alloc(outputDim_);
		gsl_vector_set_zero(y_tilde_);
	}
	~LinearMassStringDamperSystem()
	{
		gsl_matrix_free(Phi_);  Phi_ = NULL;
		gsl_matrix_free(C_);  C_ = NULL;
		//gsl_matrix_free(W_);  W_ = NULL;
		//gsl_matrix_free(V_);  V_ = NULL;
		gsl_matrix_free(Qd_);  Qd_ = NULL;
		gsl_matrix_free(Rd_);  Rd_ = NULL;

		gsl_vector_free(Bu_);  Bu_ = NULL;
		gsl_vector_free(Du_);  Du_ = NULL;

		gsl_vector_free(y_tilde_);  y_tilde_ = NULL;
	}

private:
	LinearMassStringDamperSystem(const LinearMassStringDamperSystem &rhs);
	LinearMassStringDamperSystem & operator=(const LinearMassStringDamperSystem &rhs);

public:
	// the stochastic differential equation: x(k+1) = Phi(k) * x(k) + Bd(k) * u(k) + W(k) * w(k)
	/*virtual*/ gsl_matrix * getStateTransitionMatrix(const size_t step, const gsl_vector *state) const  {  return Phi_;  }  // Phi(k) = exp(A(k) * Ts)
	/*virtual*/ gsl_vector * getControlInput(const size_t step, const gsl_vector *state) const  // Bu(k) = Bd(k) * u(k)
	{  throw std::runtime_error("this function doesn't have to be called");  }
	///*virtual*/ gsl_matrix * getProcessNoiseCouplingMatrix(const size_t step) const  {  return W_;  }  // W(k)

	// the observation equation: y(k) = Cd(k) * x(k) + Dd(k) * u(k) + V(k) * v(k)
	/*virtual*/ gsl_matrix * getOutputMatrix(const size_t step, const gsl_vector *state) const  {  return C_;  }  // Cd(k) (C == Cd)
	/*virtual*/ gsl_vector * getMeasurementInput(const size_t step, const gsl_vector *state) const  // Du(k) = D(k) * u(k) (D == Dd)
	{  throw std::runtime_error("this function doesn't have to be called");  }
	///*virtual*/ gsl_matrix * getMeasurementNoiseCouplingMatrix(const size_t step) const  {  return V_;  }  // V(k)

	// noise covariance matrices
	/*virtual*/ gsl_matrix * getProcessNoiseCovarianceMatrix(const size_t step) const  {  return Qd_;  }  // Qd = W * Q * W^T, but not Q
	/*virtual*/ gsl_matrix * getMeasurementNoiseCovarianceMatrix(const size_t step) const  {  return Rd_;  }  // Rd = V * R * V^T, but not R

	//
	gsl_vector * calculateControlInput(const size_t step) const  {  return Bu_;  }  // Bu(k) = Bd(k) * u(k)
	gsl_vector * calculateMeasurementInput(const size_t step) const  {  return Du_;  }  // Du(k) = D(k) * u(k) (D == Dd)

	// actual measurement
	gsl_vector * simulateMeasurement(const size_t step, const gsl_vector *state) const
	{
		gsl_vector_set(y_tilde_, 0, gsl_vector_get(state, 0));  // measurement (no noise)
		return y_tilde_;
	}

private:
	gsl_matrix *Phi_;
	gsl_matrix *C_;  // Cd = C
	//gsl_matrix *W_;
	//gsl_matrix *V_;
	gsl_matrix *Qd_;
	gsl_matrix *Rd_;

	// control input: Bu = Bd * u where Bd = integrate(exp(A * t), {t, 0, Ts}) * B or A^-1 * (Ad - I) * B if A is nonsingular
	gsl_vector *Bu_;
	// measurement input: Du = Dd * u where Dd = D
	gsl_vector *Du_;

	// actual measurement
	gsl_vector *y_tilde_;
};

/*static*/ const double LinearMassStringDamperSystem::Ts = 0.01;
/*static*/ const double LinearMassStringDamperSystem::zeta = 0.2;
/*static*/ const double LinearMassStringDamperSystem::omega = 5.0;
/*static*/ const double LinearMassStringDamperSystem::Q = 4.47;
/*static*/ const double LinearMassStringDamperSystem::R = 0.01;
/*static*/ const double LinearMassStringDamperSystem::Fd = 12.0;

// "Kalman Filtering: Theory and Practice Using MATLAB" Example 4.4 (pp. 156)
class RadarTrackingSystem: public swl::DiscreteLinearStochasticSystem
{
public:
	typedef swl::DiscreteLinearStochasticSystem base_type;

public:
	static const double Ts;
	static const double rho;
	static const double var_r;
	static const double var_theta;
	static const double var_1;
	static const double var_2;

public:
	RadarTrackingSystem(const size_t stateDim, const size_t inputDim, const size_t outputDim)
	: base_type(stateDim, inputDim, outputDim, (size_t)-1, (size_t)-1),
	  Phi_(NULL), C_(NULL), /*W_(NULL), V_(NULL),*/ Qd_(NULL), Rd_(NULL), Bu_(NULL), Du_(NULL), y_tilde_(NULL)
	{
		// Phi = exp(A * Ts)
		Phi_ = gsl_matrix_alloc(stateDim_, stateDim_);
		gsl_matrix_set_identity(Phi_);
		gsl_matrix_set(Phi_, 0, 1, Ts);  gsl_matrix_set(Phi_, 1, 2, 1.0);  gsl_matrix_set(Phi_, 2, 2, rho);
		gsl_matrix_set(Phi_, 3, 4, Ts);  gsl_matrix_set(Phi_, 4, 5, 1.0);  gsl_matrix_set(Phi_, 5, 5, rho);

		// C = [ 1 0 0 0 0 0 ; 0 0 0 1 0 0 ]
		C_ = gsl_matrix_alloc(outputDim_, stateDim_);
		gsl_matrix_set_zero(C_);
		gsl_matrix_set(C_, 0, 0, 1.0);  gsl_matrix_set(C_, 1, 3, 1.0);

		// W = [ 0 0 ; 0 0 ; 1 0 ; 0 0 ; 0 0 ; 0 1 ]
		//W_ = gsl_matrix_alloc(stateDim_, inputDim_);
		//gsl_matrix_set_zero(W_);
		//gsl_matrix_set(W_, 2, 0, 1.0);  gsl_matrix_set(W_, 5, 1, 1.0);

		// V = [ 1 0 ; 0  1 ]
		//V_ = gsl_matrix_alloc(outputDim_, outputDim_);
		//gsl_matrix_set_identity(V_);

		// Qd = W * Q * W^T
		Qd_ = gsl_matrix_alloc(stateDim_, stateDim_);
		gsl_matrix_set_zero(Qd_);
		gsl_matrix_set(Qd_, 2, 2, var_1);  gsl_matrix_set(Qd_, 5, 5, var_2);

		// Rd = V * R * V^T
		Rd_ = gsl_matrix_alloc(outputDim_, outputDim_);
		gsl_matrix_set_zero(Rd_);
		gsl_matrix_set(Rd_, 0, 0, var_r);  gsl_matrix_set(Rd_, 1, 1, var_theta);

		// no control input
		Bu_ = gsl_vector_alloc(stateDim_);
		gsl_vector_set_zero(Bu_);

		// no measurement input
		Du_ = gsl_vector_alloc(outputDim_);
		gsl_vector_set_zero(Du_);

		//
		y_tilde_ = gsl_vector_alloc(outputDim_);
		gsl_vector_set_zero(y_tilde_);
	}
	~RadarTrackingSystem()
	{
		gsl_matrix_free(Phi_);  Phi_ = NULL;
		gsl_matrix_free(C_);  C_ = NULL;
		//gsl_matrix_free(W_);  W_ = NULL;
		//gsl_matrix_free(V_);  V_ = NULL;
		gsl_matrix_free(Qd_);  Qd_ = NULL;
		gsl_matrix_free(Rd_);  Rd_ = NULL;

		gsl_vector_free(Bu_);  Bu_ = NULL;
		gsl_vector_free(Du_);  Du_ = NULL;

		gsl_vector_free(y_tilde_);  y_tilde_ = NULL;
	}

private:
	RadarTrackingSystem(const RadarTrackingSystem &rhs);
	RadarTrackingSystem & operator=(const RadarTrackingSystem &rhs);

public:
	// the stochastic differential equation: x(k+1) = Phi(k) * x(k) + Bd(k) * u(k) + W(k) * w(k)
	/*virtual*/ gsl_matrix * getStateTransitionMatrix(const size_t step, const gsl_vector *state) const  {  return Phi_;  }  // Phi(k) = exp(A(k) * Ts)
	/*virtual*/ gsl_vector * getControlInput(const size_t step, const gsl_vector *state) const  // Bu(k) = Bd(k) * u(k)
	{  throw std::runtime_error("this function doesn't have to be called");  }
	///*virtual*/ gsl_matrix * getProcessNoiseCouplingMatrix(const size_t step) const  {  return W_;  }  // W(k)

	// the observation equation: y(k) = Cd(k) * x(k) + Dd(k) * u(k)
	/*virtual*/ gsl_matrix * getOutputMatrix(const size_t step, const gsl_vector *state) const  {  return C_;  }  // Cd(k) (C == Cd)
	/*virtual*/ gsl_vector * getMeasurementInput(const size_t step, const gsl_vector *state) const  // Du(k) = D(k) * u(k) (D == Dd)
	{  throw std::runtime_error("this function doesn't have to be called");  }
	///*virtual*/ gsl_matrix * getMeasurementNoiseCouplingMatrix(const size_t step) const  {  return V_;  }  // V(k)

	// noise covariance matrices
	/*virtual*/ gsl_matrix * getProcessNoiseCovarianceMatrix(const size_t step) const  {  return Qd_;  }  // Qd = W * Q * W^T, but not Q
	/*virtual*/ gsl_matrix * getMeasurementNoiseCovarianceMatrix(const size_t step) const  {  return Rd_;  }  // Rd = V * R * V^T, but not R

	//
	gsl_vector * calculateControlInput(const size_t step) const  {  return Bu_;  }  // Bu(k) = Bd(k) * u(k)
	gsl_vector * calculateMeasurementInput(const size_t step) const  {  return Du_;  }  // Du(k) = D(k) * u(k) (D == Dd)

	// actual measurement
	gsl_vector * simulateMeasurement(const size_t step, const gsl_vector *state) const
	{
		typedef boost::minstd_rand base_generator_type;
		typedef boost::normal_distribution<> distribution_type;
		typedef boost::variate_generator<base_generator_type &, distribution_type> generator_type;

		base_generator_type generator(static_cast<unsigned int>(std::time(0)));
		//generator.seed(static_cast<unsigned int>(std::time(0)));
		generator_type v1_noise(generator, distribution_type(0.0, std::sqrt(var_r)));
		generator_type v2_noise(generator, distribution_type(0.0, std::sqrt(var_theta)));

		gsl_vector_set(y_tilde_, 0, gsl_vector_get(state, 0) + v1_noise());  // measurement (white noise)
		gsl_vector_set(y_tilde_, 0, gsl_vector_get(state, 3) + v2_noise());  // measurement (white noise)

		return y_tilde_;
	}

private:
	gsl_matrix *Phi_;
	gsl_matrix *C_;  // Cd = C
	//gsl_matrix *W_;
	//gsl_matrix *V_;
	gsl_matrix *Qd_;
	gsl_matrix *Rd_;

	// control input: Bu = Bd * u where Bd = integrate(exp(A * t), {t, 0, Ts}) * B or A^-1 * (Ad - I) * B if A is nonsingular
	gsl_vector *Bu_;
	// measurement input: Du = Dd * u where Dd = D
	gsl_vector *Du_;

	// actual measurement
	gsl_vector *y_tilde_;
};

///*static*/ const double RadarTrackingSystem::Ts = 5.0;
///*static*/ const double RadarTrackingSystem::Ts = 10.0;
/*static*/ const double RadarTrackingSystem::Ts = 15.0;
/*static*/ const double RadarTrackingSystem::rho = 0.5;
/*static*/ const double RadarTrackingSystem::var_r = 1000.0 * 1000.0;
/*static*/ const double RadarTrackingSystem::var_theta = 0.017 * 0.017;
/*static*/ const double RadarTrackingSystem::var_1 = (103.0/3.0) * (103.0/3.0);
/*static*/ const double RadarTrackingSystem::var_2 = 1.3e-8;

void simple_system_kalman_filter()
{
	const size_t stateDim = 1;
	const size_t inputDim = 1;
	const size_t outputDim = 1;

	gsl_vector *x0 = gsl_vector_alloc(stateDim);
	gsl_matrix *P0 = gsl_matrix_alloc(stateDim, stateDim);
	gsl_vector_set(x0, 0, 1.0);
	gsl_matrix_set(P0, 0, 0, 10.0);

	const SimpleLinearSystem system(stateDim, inputDim, outputDim);
	swl::DiscreteKalmanFilter filter(system, x0, P0);

	gsl_vector_free(x0);  x0 = NULL;
	gsl_matrix_free(P0);  P0 = NULL;

	//
	const size_t Nstep = 2;
	std::vector<double> state, gain, errVar;
	state.reserve(Nstep * 2);
	gain.reserve(Nstep);
	errVar.reserve(Nstep * 2);

#if 1
	// method #1
	// 1-based time step. 0-th time step is initial
	size_t step = 0;
	while (step < Nstep)
	{
		// 0. initial estimates: x(0) & P(0)

		// 1. time update (prediction): x(k) & P(k)  ==>  x-(k+1) & P-(k+1)
		const gsl_vector *Bu = system.calculateControlInput(step);
		const bool retval1 = filter.updateTime(step, Bu);
		assert(retval1);

		// save x-(k+1) & P-(k+1)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			state.push_back(gsl_vector_get(x_hat, 0));
			errVar.push_back(gsl_matrix_get(P, 0, 0));
		}

		// advance time step
		++step;

		// 2. measurement update (correction): x-(k), P-(k) & y_tilde(k)  ==>  K(k), x(k) & P(k)
		const gsl_vector *actualMeasurement = system.simulateMeasurement(step, filter.getEstimatedState());
		const gsl_vector *Du = system.calculateMeasurementInput(step);
		const bool retval2 = filter.updateMeasurement(step, actualMeasurement, Du);
		assert(retval2);

		// save K(k), x(k) & P(k)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *K = filter.getKalmanGain();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			state.push_back(gsl_vector_get(x_hat, 0));
			gain.push_back(gsl_matrix_get(K, 0, 0));
			errVar.push_back(gsl_matrix_get(P, 0, 0));
		}
	}
#else
	// method #2
	// 0-based time step. 0-th time step is initial
	size_t step = 0;
	while (step < Nstep)
	{
		// 0. initial estimates: x-(0) & P-(0)

		// 1. measurement update (correction): x-(k), P-(k) & y_tilde(k)  ==>  K(k), x(k) & P(k)
		const gsl_vector *actualMeasurement = system.simulateMeasurement(step, filter.getEstimatedState());
		const gsl_vector *Du = system.calculateMeasurementInput(step);
		const bool retval1 = filter.updateMeasurement(step, actualMeasurement, Du);
		assert(retval1);

		// save K(k), x(k) & P(k)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *K = filter.getKalmanGain();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			state.push_back(gsl_vector_get(x_hat, 0));
			gain.push_back(gsl_matrix_get(K, 0, 0));
			errVar.push_back(gsl_matrix_get(P, 0, 0));
		}

		// 2. time update (prediction): x(k) & P(k)  ==>  x-(k+1) & P-(k+1)
		const gsl_vector *Bu = system.calculateControlInput(step);
		const bool retval2 = filter.updateTime(step, Bu);
		assert(retval2);

		// save x-(k+1) & P-(k+1)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			state.push_back(gsl_vector_get(x_hat, 0));
			errVar.push_back(gsl_matrix_get(P, 0, 0));
		}

		// advance time step
		++step;
	}
#endif

	//
	std::ofstream stream("./data/kalman_filter.dat", std::ios::out | std::ios::trunc);
	if (stream)
	{
		output_data_to_file(stream, "state", state);
		output_data_to_file(stream, "gain", gain);
		output_data_to_file(stream, "errVar", errVar);

		stream.close();
	}
}

void aided_INS_kalman_filter()
{
	const size_t stateDim = 3;
	const size_t inputDim = 2;
	const size_t outputDim = 1;

	gsl_vector *x0 = gsl_vector_alloc(stateDim);
	gsl_matrix *P0 = gsl_matrix_alloc(stateDim, stateDim);
	gsl_vector_set_zero(x0);
	gsl_matrix_set_zero(P0);

	const AidedINSSystem system(stateDim, inputDim, outputDim);
	swl::DiscreteKalmanFilter filter(system, x0, P0);

	gsl_vector_free(x0);  x0 = NULL;
	gsl_matrix_free(P0);  P0 = NULL;

	//
	const size_t Nstep = 100;
	std::vector<double> pos, vel, bias;
	std::vector<double> posGain, velGain, biasGain;
	std::vector<double> posErrVar, velErrVar, biasErrVar;
	pos.reserve(Nstep * 2);
	vel.reserve(Nstep * 2);
	bias.reserve(Nstep * 2);
	posGain.reserve(Nstep);
	velGain.reserve(Nstep);
	biasGain.reserve(Nstep);
	posErrVar.reserve(Nstep * 2);
	velErrVar.reserve(Nstep * 2);
	biasErrVar.reserve(Nstep * 2);

#if 0
	// method #1
	// 1-based time step. 0-th time step is initial
	size_t step = 0;
	while (step < Nstep)
	{
		// 0. initial estimates: x(0) & P(0)

		// 1. time update (prediction): x(k) & P(k)  ==>  x-(k+1) & P-(k+1)
		const gsl_vector *Bu = system.calculateControlInput(step);
		const bool retval1 = filter.updateTime(step, Bu);
		assert(retval1);

		// save x-(k+1) & P-(k+1)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			pos.push_back(gsl_vector_get(x_hat, 0));
			vel.push_back(gsl_vector_get(x_hat, 1));
			bias.push_back(gsl_vector_get(x_hat, 2));
			posErrVar.push_back(gsl_matrix_get(P, 0, 0));
			velErrVar.push_back(gsl_matrix_get(P, 1, 1));
			biasErrVar.push_back(gsl_matrix_get(P, 2, 2));
		}

		// advance time step
		++step;

		// 2. measurement update (correction): x-(k), P-(k) & y_tilde(k)  ==>  K(k), x(k) & P(k)
		const gsl_vector *actualMeasurement = system.simulateMeasurement(step, filter.getEstimatedState());
		const gsl_vector *Du = system.calculateMeasurementInput(step);
		const bool retval2 = filter.updateMeasurement(step, actualMeasurement, Du);
		assert(retval2);

		// save K(k), x(k) & P(k)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *K = filter.getKalmanGain();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			pos.push_back(gsl_vector_get(x_hat, 0));
			vel.push_back(gsl_vector_get(x_hat, 1));
			bias.push_back(gsl_vector_get(x_hat, 2));
			posGain.push_back(gsl_matrix_get(K, 0, 0));
			velGain.push_back(gsl_matrix_get(K, 1, 0));
			biasGain.push_back(gsl_matrix_get(K, 2, 0));
			posErrVar.push_back(gsl_matrix_get(P, 0, 0));
			velErrVar.push_back(gsl_matrix_get(P, 1, 1));
			biasErrVar.push_back(gsl_matrix_get(P, 2, 2));
		}
	}
#else
	// method #2
	// 0-based time step. 0-th time step is initial
	size_t step = 0;
	while (step < Nstep)
	{
		// 0. initial estimates: x-(0) & P-(0)

		// 1. measurement update (correction): x-(k), P-(k) & y_tilde(k)  ==>  K(k), x(k) & P(k)
		const gsl_vector *actualMeasurement = system.simulateMeasurement(step, filter.getEstimatedState());
		const gsl_vector *Du = system.calculateMeasurementInput(step);
		const bool retval1 = filter.updateMeasurement(step, actualMeasurement, Du);
		assert(retval1);

		// save K(k), x(k) & P(k)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *K = filter.getKalmanGain();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			pos.push_back(gsl_vector_get(x_hat, 0));
			vel.push_back(gsl_vector_get(x_hat, 1));
			bias.push_back(gsl_vector_get(x_hat, 2));
			posGain.push_back(gsl_matrix_get(K, 0, 0));
			velGain.push_back(gsl_matrix_get(K, 1, 0));
			biasGain.push_back(gsl_matrix_get(K, 2, 0));
			posErrVar.push_back(gsl_matrix_get(P, 0, 0));
			velErrVar.push_back(gsl_matrix_get(P, 1, 1));
			biasErrVar.push_back(gsl_matrix_get(P, 2, 2));
		}

		// 2. time update (prediction): x(k) & P(k)  ==>  x-(k+1) & P-(k+1)
		const gsl_vector *Bu = system.calculateControlInput(step);
		const bool retval2 = filter.updateTime(step, Bu);
		assert(retval2);

		// save x-(k+1) & P-(k+1)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			pos.push_back(gsl_vector_get(x_hat, 0));
			vel.push_back(gsl_vector_get(x_hat, 1));
			bias.push_back(gsl_vector_get(x_hat, 2));
			posErrVar.push_back(gsl_matrix_get(P, 0, 0));
			velErrVar.push_back(gsl_matrix_get(P, 1, 1));
			biasErrVar.push_back(gsl_matrix_get(P, 2, 2));
		}

		// advance time step
		++step;
	}
#endif

	//
	std::ofstream stream("./data/extended_kalman_filter.dat", std::ios::out | std::ios::trunc);
	if (stream)
	{
		output_data_to_file(stream, "pos", pos);
		output_data_to_file(stream, "vel", vel);
		output_data_to_file(stream, "bias", bias);
		output_data_to_file(stream, "posGain", posGain);
		output_data_to_file(stream, "velGain", velGain);
		output_data_to_file(stream, "biasGain", biasGain);
		output_data_to_file(stream, "posErrVar", posErrVar);
		output_data_to_file(stream, "velErrVar", velErrVar);
		output_data_to_file(stream, "biasErrVar", biasErrVar);

		stream.close();
	}
}

void linear_mass_spring_damper_system_kalman_filter()
{
	const size_t stateDim = 2;
	const size_t inputDim = 1;
	const size_t outputDim = 1;

	gsl_vector *x0 = gsl_vector_alloc(stateDim);
	gsl_vector_set_zero(x0);
	gsl_matrix *P0 = gsl_matrix_alloc(stateDim, stateDim);
	gsl_matrix_set_zero(P0);
	gsl_matrix_set(P0, 0, 0, 2.0);  gsl_matrix_set(P0, 1, 1, 2.0);

	LinearMassStringDamperSystem system(stateDim, inputDim, outputDim);
	swl::DiscreteKalmanFilter filter(system, x0, P0);

	gsl_vector_free(x0);  x0 = NULL;
	gsl_matrix_free(P0);  P0 = NULL;

	//
	const size_t Nstep = 100;
	std::vector<double> pos, vel;
	std::vector<double> posGain, velGain;
	std::vector<double> posErrVar, velErrVar;
	std::vector<double> corrCoeff;
	pos.reserve(Nstep * 2);
	vel.reserve(Nstep * 2);
	posGain.reserve(Nstep);
	velGain.reserve(Nstep);
	posErrVar.reserve(Nstep * 2);
	velErrVar.reserve(Nstep * 2);
	corrCoeff.reserve(Nstep * 2);

#if 1
	// method #1
	// 1-based time step. 0-th time step is initial
	size_t step = 0;
	while (step < Nstep)
	{
		// 0. initial estimates: x(0) & P(0)

		// 1. time update (prediction): x(k) & P(k)  ==>  x-(k+1) & P-(k+1)
		const gsl_vector *Bu = system.calculateControlInput(step);
		const bool retval1 = filter.updateTime(step, Bu);
		assert(retval1);

		// save x-(k+1) & P-(k+1)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			pos.push_back(gsl_vector_get(x_hat, 0));
			vel.push_back(gsl_vector_get(x_hat, 1));
			posErrVar.push_back(gsl_matrix_get(P, 0, 0));
			velErrVar.push_back(gsl_matrix_get(P, 1, 1));

			corrCoeff.push_back(gsl_matrix_get(P, 0, 1) / (std::sqrt(posErrVar[step]) * std::sqrt(velErrVar[step])));
		}

		// advance time step
		++step;

		// 2. measurement update (correction): x-(k), P-(k) & y_tilde(k)  ==>  K(k), x(k) & P(k)
		const gsl_vector *actualMeasurement = system.simulateMeasurement(step, filter.getEstimatedState());
		const gsl_vector *Du = system.calculateMeasurementInput(step);
		const bool retval2 = filter.updateMeasurement(step, actualMeasurement, Du);
		assert(retval2);

		// save K(k), x(k) & P(k)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *K = filter.getKalmanGain();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			pos.push_back(gsl_vector_get(x_hat, 0));
			vel.push_back(gsl_vector_get(x_hat, 1));
			posGain.push_back(gsl_matrix_get(K, 0, 0));
			velGain.push_back(gsl_matrix_get(K, 1, 0));
			posErrVar.push_back(gsl_matrix_get(P, 0, 0));
			velErrVar.push_back(gsl_matrix_get(P, 1, 1));

			corrCoeff.push_back(gsl_matrix_get(P, 0, 1) / (std::sqrt(posErrVar[step]) * std::sqrt(velErrVar[step])));
		}
	}
#else
	// method #2
	// 0-based time step. 0-th time step is initial
	size_t step = 0;
	while (step < Nstep)
	{
		// 0. initial estimates: x-(0) & P-(0)

		// 1. measurement update (correction): x-(k), P-(k) & y_tilde(k)  ==>  K(k), x(k) & P(k)
		const gsl_vector *actualMeasurement = system.simulateMeasurement(step, filter.getEstimatedState());
		const gsl_vector *Du = system.calculateMeasurementInput(step);
		const bool retval1 = filter.updateMeasurement(step, actualMeasurement, Du);
		assert(retval1);

		// save K(k), x(k) & P(k)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *K = filter.getKalmanGain();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			pos.push_back(gsl_vector_get(x_hat, 0));
			vel.push_back(gsl_vector_get(x_hat, 1));
			posGain.push_back(gsl_matrix_get(K, 0, 0));
			velGain.push_back(gsl_matrix_get(K, 1, 0));
			posErrVar.push_back(gsl_matrix_get(P, 0, 0));
			velErrVar.push_back(gsl_matrix_get(P, 1, 1));

			corrCoeff.push_back(gsl_matrix_get(P, 0, 1) / (std::sqrt(posErrVar[step]) * std::sqrt(velErrVar[step])));
		}

		// 2. time update (prediction): x(k) & P(k)  ==>  x-(k+1) & P-(k+1)
		const gsl_vector *Bu = system.calculateControlInput(step);
		const bool retval2 = filter.updateTime(step, Bu);
		assert(retval2);

		// save x-(k+1) & P-(k+1)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			pos.push_back(gsl_vector_get(x_hat, 0));
			vel.push_back(gsl_vector_get(x_hat, 1));
			posErrVar.push_back(gsl_matrix_get(P, 0, 0));
			velErrVar.push_back(gsl_matrix_get(P, 1, 1));

			corrCoeff.push_back(gsl_matrix_get(P, 0, 1) / (std::sqrt(posErrVar[step]) * std::sqrt(velErrVar[step])));
		}

		// advance time step
		++step;
	}
#endif

	//
	std::ofstream stream("./data/extended_kalman_filter.dat", std::ios::out | std::ios::trunc);
	if (stream)
	{
		output_data_to_file(stream, "pos", pos);
		output_data_to_file(stream, "vel", vel);
		output_data_to_file(stream, "posGain", posGain);
		output_data_to_file(stream, "velGain", velGain);
		output_data_to_file(stream, "posErrVar", posErrVar);
		output_data_to_file(stream, "velErrVar", velErrVar);

		stream.close();
	}
}

void radar_tracking_system_kalman_filter()
{
	const size_t stateDim = 6;
	const size_t inputDim = 2;
	const size_t outputDim = 2;

	const double &Ts = RadarTrackingSystem::Ts;
	const double &var_r = RadarTrackingSystem::var_r;
	const double &var_theta = RadarTrackingSystem::var_theta;
	const double &var_1 = RadarTrackingSystem::var_1;
	const double &var_2 = RadarTrackingSystem::var_2;

	gsl_vector *x0 = gsl_vector_alloc(stateDim);
	gsl_vector_set_zero(x0);
	gsl_matrix *P0 = gsl_matrix_alloc(stateDim, stateDim);
	gsl_matrix_set_zero(P0);
	gsl_matrix_set(P0, 0, 0, var_r);  gsl_matrix_set(P0, 0, 1, var_r / Ts);
	gsl_matrix_set(P0, 1, 0, var_r / Ts);  gsl_matrix_set(P0, 1, 1, 2.0 * var_r / (Ts*Ts) + var_1);
	gsl_matrix_set(P0, 2, 2, var_1);
	gsl_matrix_set(P0, 3, 3, var_theta);  gsl_matrix_set(P0, 3, 4, var_theta / Ts);
	gsl_matrix_set(P0, 4, 3, var_theta / Ts);  gsl_matrix_set(P0, 4, 4, 2.0 * var_theta / (Ts*Ts) + var_2);
	gsl_matrix_set(P0, 5, 5, var_2);

	RadarTrackingSystem system(stateDim, inputDim, outputDim);
	swl::DiscreteKalmanFilter filter(system, x0, P0);

	gsl_vector_free(x0);  x0 = NULL;
	gsl_matrix_free(P0);  P0 = NULL;

	//
	const size_t Nstep = 150;
	std::vector<double> range, rangeRate, rangeRateNoise, bearing, bearingRate, bearingRateNoise;
	std::vector<double> rangeGain, rangeRateGain, rangeRateNoiseGain, bearingGain, bearingRateGain, bearingRateNoiseGain;
	std::vector<double> rangeErrVar, rangeRateErrVar, rangeRateNoiseErrVar, bearingErrVar, bearingRateErrVar, bearingRateNoiseErrVar;
	range.reserve(Nstep * 2);
	rangeRate.reserve(Nstep * 2);
	rangeRateNoise.reserve(Nstep * 2);
	bearing.reserve(Nstep * 2);
	bearingRate.reserve(Nstep * 2);
	bearingRateNoise.reserve(Nstep * 2);
	rangeGain.reserve(Nstep);
	rangeRateGain.reserve(Nstep);
	rangeRateNoiseGain.reserve(Nstep);
	bearingGain.reserve(Nstep);
	bearingRateGain.reserve(Nstep);
	bearingRateNoiseGain.reserve(Nstep);
	rangeErrVar.reserve(Nstep * 2);
	rangeRateErrVar.reserve(Nstep * 2);
	rangeRateNoiseErrVar.reserve(Nstep * 2);
	bearingErrVar.reserve(Nstep * 2);
	bearingRateErrVar.reserve(Nstep * 2);
	bearingRateNoiseErrVar.reserve(Nstep * 2);
#if 1
	// method #1
	// 1-based time step. 0-th time step is initial
	size_t step = 0;
	while (step < Nstep)
	{
		// 0. initial estimates: x(0) & P(0)

		// 1. time update (prediction): x(k) & P(k)  ==>  x-(k+1) & P-(k+1)
		const gsl_vector *Bu = system.calculateControlInput(step);
		const bool retval1 = filter.updateTime(step, Bu);
		assert(retval1);

		// save x-(k+1) & P-(k+1)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			range.push_back(gsl_vector_get(x_hat, 0));
			rangeRate.push_back(gsl_vector_get(x_hat, 1));
			rangeRateNoise.push_back(gsl_vector_get(x_hat, 2));
			bearing.push_back(gsl_vector_get(x_hat, 3));
			bearingRate.push_back(gsl_vector_get(x_hat, 4));
			bearingRateNoise.push_back(gsl_vector_get(x_hat, 5));

			rangeErrVar.push_back(gsl_matrix_get(P, 0, 0));
			rangeRateErrVar.push_back(gsl_matrix_get(P, 1, 1));
			rangeRateNoiseErrVar.push_back(gsl_matrix_get(P, 2, 2));
			bearingErrVar.push_back(gsl_matrix_get(P, 3, 3));
			bearingRateErrVar.push_back(gsl_matrix_get(P, 4, 4));
			bearingRateNoiseErrVar.push_back(gsl_matrix_get(P, 5, 5));
		}

		// advance time step
		++step;

		// 2. measurement update (correction): x-(k), P-(k) & y_tilde(k)  ==>  K(k), x(k) & P(k)
		const gsl_vector *actualMeasurement = system.simulateMeasurement(step, filter.getEstimatedState());
		const gsl_vector *Du = system.calculateMeasurementInput(step);
		const bool retval2 = filter.updateMeasurement(step, actualMeasurement, Du);
		assert(retval2);

		// save K(k), x(k) & P(k)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *K = filter.getKalmanGain();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			range.push_back(gsl_vector_get(x_hat, 0));
			rangeRate.push_back(gsl_vector_get(x_hat, 1));
			rangeRateNoise.push_back(gsl_vector_get(x_hat, 2));
			bearing.push_back(gsl_vector_get(x_hat, 3));
			bearingRate.push_back(gsl_vector_get(x_hat, 4));
			bearingRateNoise.push_back(gsl_vector_get(x_hat, 5));

			rangeGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 0, 0),2)+std::pow(gsl_matrix_get(K, 0, 1),2)));
			rangeRateGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 1, 0),2)+std::pow(gsl_matrix_get(K, 1, 1),2)));
			rangeRateNoiseGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 2, 0),2)+std::pow(gsl_matrix_get(K, 2, 1),2)));
			bearingGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 3, 0),2)+std::pow(gsl_matrix_get(K, 3, 1),2)));
			bearingRateGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 4, 0),2)+std::pow(gsl_matrix_get(K, 4, 1),2)));
			bearingRateNoiseGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 5, 0),2)+std::pow(gsl_matrix_get(K, 5, 1),2)));

			rangeErrVar.push_back(gsl_matrix_get(P, 0, 0));
			rangeRateErrVar.push_back(gsl_matrix_get(P, 1, 1));
			rangeRateNoiseErrVar.push_back(gsl_matrix_get(P, 2, 2));
			bearingErrVar.push_back(gsl_matrix_get(P, 3, 3));
			bearingRateErrVar.push_back(gsl_matrix_get(P, 4, 4));
			bearingRateNoiseErrVar.push_back(gsl_matrix_get(P, 5, 5));
		}
	}
#else
	// method #2
	// 0-based time step. 0-th time step is initial
	size_t step = 0;
	while (step < Nstep)
	{
		// 0. initial estimates: x-(0) & P-(0)

		// 1. measurement update (correction): x-(k), P-(k) & y_tilde(k)  ==>  K(k), x(k) & P(k)
		const gsl_vector *actualMeasurement = system.simulateMeasurement(step, filter.getEstimatedState());
		const gsl_vector *Du = system.calculateMeasurementInput(step);
		const bool retval1 = filter.updateMeasurement(step, actualMeasurement, Du);
		assert(retval1);

		// save K(k), x(k) & P(k)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *K = filter.getKalmanGain();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			range.push_back(gsl_vector_get(x_hat, 0));
			rangeRate.push_back(gsl_vector_get(x_hat, 1));
			rangeRateNoise.push_back(gsl_vector_get(x_hat, 2));
			bearing.push_back(gsl_vector_get(x_hat, 3));
			bearingRate.push_back(gsl_vector_get(x_hat, 4));
			bearingRateNoise.push_back(gsl_vector_get(x_hat, 5));

			rangeGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 0, 0),2)+std::pow(gsl_matrix_get(K, 0, 1),2)));
			rangeRateGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 1, 0),2)+std::pow(gsl_matrix_get(K, 1, 1),2)));
			rangeRateNoiseGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 2, 0),2)+std::pow(gsl_matrix_get(K, 2, 1),2)));
			bearingGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 3, 0),2)+std::pow(gsl_matrix_get(K, 3, 1),2)));
			bearingRateGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 4, 0),2)+std::pow(gsl_matrix_get(K, 4, 1),2)));
			bearingRateNoiseGain.push_back(std::sqrt(std::pow(gsl_matrix_get(K, 5, 0),2)+std::pow(gsl_matrix_get(K, 5, 1),2)));

			rangeErrVar.push_back(gsl_matrix_get(P, 0, 0));
			rangeRateErrVar.push_back(gsl_matrix_get(P, 1, 1));
			rangeRateNoiseErrVar.push_back(gsl_matrix_get(P, 2, 2));
			bearingErrVar.push_back(gsl_matrix_get(P, 3, 3));
			bearingRateErrVar.push_back(gsl_matrix_get(P, 4, 4));
			bearingRateNoiseErrVar.push_back(gsl_matrix_get(P, 5, 5));
		}

		// 2. time update (prediction): x(k) & P(k)  ==>  x-(k+1) & P-(k+1)
		const gsl_vector *Bu = system.calculateControlInput(step);
		const bool retval2 = filter.updateTime(step, Bu);
		assert(retval2);

		// save x-(k+1) & P-(k+1)
		{
			const gsl_vector *x_hat = filter.getEstimatedState();
			const gsl_matrix *P = filter.getStateErrorCovarianceMatrix();

			range.push_back(gsl_vector_get(x_hat, 0));
			rangeRate.push_back(gsl_vector_get(x_hat, 1));
			rangeRateNoise.push_back(gsl_vector_get(x_hat, 2));
			bearing.push_back(gsl_vector_get(x_hat, 3));
			bearingRate.push_back(gsl_vector_get(x_hat, 4));
			bearingRateNoise.push_back(gsl_vector_get(x_hat, 5));

			rangeErrVar.push_back(gsl_matrix_get(P, 0, 0));
			rangeRateErrVar.push_back(gsl_matrix_get(P, 1, 1));
			rangeRateNoiseErrVar.push_back(gsl_matrix_get(P, 2, 2));
			bearingErrVar.push_back(gsl_matrix_get(P, 3, 3));
			bearingRateErrVar.push_back(gsl_matrix_get(P, 4, 4));
			bearingRateNoiseErrVar.push_back(gsl_matrix_get(P, 5, 5));
		}

		// advance time step
		++step;
	}
#endif

	//
	std::ofstream stream("./data/extended_kalman_filter.dat", std::ios::out | std::ios::trunc);
	if (stream)
	{
		output_data_to_file(stream, "rangeGain", rangeGain);
		output_data_to_file(stream, "rangeRateGain", rangeRateGain);
		output_data_to_file(stream, "rangeRateNoiseGain", rangeRateNoiseGain);
		output_data_to_file(stream, "bearingGain", bearingGain);
		output_data_to_file(stream, "bearingRateGain", bearingRateGain);
		output_data_to_file(stream, "bearingRateNoiseGain", bearingRateNoiseGain);

		output_data_to_file(stream, "rangeErrVar", rangeErrVar);
		output_data_to_file(stream, "rangeRateErrVar", rangeRateErrVar);
		output_data_to_file(stream, "rangeRateNoiseErrVar", rangeRateNoiseErrVar);
		output_data_to_file(stream, "bearingErrVar", bearingErrVar);
		output_data_to_file(stream, "bearingRateErrVar", bearingRateErrVar);
		output_data_to_file(stream, "bearingRateNoiseErrVar", bearingRateNoiseErrVar);

		stream.close();
	}
}

}  // namespace local
}  // unnamed namespace

void kalman_filter()
{
	local::simple_system_kalman_filter();
	local::aided_INS_kalman_filter();
	local::linear_mass_spring_damper_system_kalman_filter();
	local::radar_tracking_system_kalman_filter();
}
