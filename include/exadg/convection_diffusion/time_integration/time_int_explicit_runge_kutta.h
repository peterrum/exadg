/*
 * time_int_explicit_runge_kutta.h
 *
 *  Created on: Aug 2, 2016
 *      Author: fehn
 */

#ifndef INCLUDE_CONVECTION_DIFFUSION_TIME_INT_EXPLICIT_RUNGE_KUTTA_H_
#define INCLUDE_CONVECTION_DIFFUSION_TIME_INT_EXPLICIT_RUNGE_KUTTA_H_

// deal.II
#include <deal.II/base/timer.h>
#include <deal.II/lac/la_parallel_vector.h>

// ExaDG
#include <exadg/time_integration/explicit_runge_kutta.h>
#include <exadg/time_integration/time_int_explicit_runge_kutta_base.h>

namespace ExaDG
{
namespace ConvDiff
{
using namespace dealii;

// forward declarations
class InputParameters;

template<typename Number>
class PostProcessorInterface;

namespace Interface
{
template<typename Number>
class Operator;
}

template<typename Number>
class OperatorExplRK;

template<typename Number>
class TimeIntExplRK : public TimeIntExplRKBase<Number>
{
public:
  typedef LinearAlgebra::distributed::Vector<Number> VectorType;

  TimeIntExplRK(std::shared_ptr<Interface::Operator<Number>>    operator_in,
                InputParameters const &                         param_in,
                unsigned int const                              refine_steps_time_in,
                MPI_Comm const &                                mpi_comm_in,
                bool const                                      print_wall_times_in,
                std::shared_ptr<PostProcessorInterface<Number>> postprocessor_in);

  void
  set_velocities_and_times(std::vector<VectorType const *> const & velocities_in,
                           std::vector<double> const &             times_in);

  void
  extrapolate_solution(VectorType & vector);

private:
  void
  initialize_vectors();

  void
  initialize_solution();

  void
  postprocessing() const;

  bool
  print_solver_info() const;

  void
  solve_timestep();

  void
  calculate_time_step_size();

  double
  recalculate_time_step_size() const;

  void
  initialize_time_integrator();

  std::shared_ptr<Interface::Operator<Number>> pde_operator;

  std::shared_ptr<OperatorExplRK<Number>> expl_rk_operator;

  std::shared_ptr<ExplicitTimeIntegrator<OperatorExplRK<Number>, VectorType>> rk_time_integrator;

  InputParameters const & param;

  unsigned int const refine_steps_time;

  std::vector<VectorType const *> velocities;
  std::vector<double>             times;

  // store time step size according to diffusion condition so that it does not have to be
  // recomputed in case of adaptive time stepping
  double time_step_diff;

  double const cfl;
  double const diffusion_number;

  std::shared_ptr<PostProcessorInterface<Number>> postprocessor;
};

} // namespace ConvDiff
} // namespace ExaDG

#endif /* INCLUDE_CONVECTION_DIFFUSION_TIME_INT_EXPLICIT_RUNGE_KUTTA_H_ */
