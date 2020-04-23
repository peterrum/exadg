/*
 * dg_operator.h
 *
 *  Created on: 2018
 *      Author: fehn
 */

#ifndef INCLUDE_COMPRESSIBLE_NAVIER_STOKES_SPATIAL_DISCRETIZATION_DG_OPERATOR_H_
#define INCLUDE_COMPRESSIBLE_NAVIER_STOKES_SPATIAL_DISCRETIZATION_DG_OPERATOR_H_

// deal.II
#include <deal.II/base/timer.h>
#include <deal.II/fe/fe_dgq.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/fe/mapping_q.h>
#include <deal.II/lac/la_parallel_vector.h>
#include <deal.II/numerics/vector_tools.h>

// user interface
#include "../../compressible_navier_stokes/user_interface/boundary_descriptor.h"
#include "../../compressible_navier_stokes/user_interface/field_functions.h"
#include "../../compressible_navier_stokes/user_interface/input_parameters.h"

// operators
#include "comp_navier_stokes_calculators.h"
#include "comp_navier_stokes_operators.h"
#include "operators/inverse_mass_matrix.h"
#include "operators/mapping_flags.h"

// interface
#include "interface.h"

// time step calculation
#include "time_integration/time_step_calculation.h"

// matrix-free
#include "../../matrix_free/matrix_free_wrapper.h"

namespace CompNS
{
template<int dim, typename Number>
class DGOperator : public dealii::Subscriptor, public Interface::Operator<Number>
{
private:
  typedef LinearAlgebra::distributed::Vector<Number> VectorType;

public:
  DGOperator(parallel::TriangulationBase<dim> const &       triangulation_in,
             Mapping<dim> const &                           mapping_in,
             unsigned int const                             degree_in,
             std::shared_ptr<BoundaryDescriptor<dim>>       boundary_descriptor_density_in,
             std::shared_ptr<BoundaryDescriptor<dim>>       boundary_descriptor_velocity_in,
             std::shared_ptr<BoundaryDescriptor<dim>>       boundary_descriptor_pressure_in,
             std::shared_ptr<BoundaryDescriptorEnergy<dim>> boundary_descriptor_energy_in,
             std::shared_ptr<FieldFunctions<dim>>           field_functions_in,
             InputParameters const &                        param_in,
             MPI_Comm const &                               mpi_comm_in);

  void
  append_data_structures(MatrixFreeWrapper<dim, Number> & matrix_free_wrapper,
                         std::string const &              field = "") const;

  void
  setup(std::shared_ptr<MatrixFreeWrapper<dim, Number>> matrix_free_wrapper);

  types::global_dof_index
  get_number_of_dofs() const;

  // initialization of DoF vectors
  void
  initialize_dof_vector(VectorType & src) const;

  void
  initialize_dof_vector_scalar(VectorType & src) const;

  void
  initialize_dof_vector_dim_components(VectorType & src) const;

  // set initial conditions
  void
  prescribe_initial_conditions(VectorType & src, double const time) const;

  /*
   *  This function is used in case of explicit time integration:
   *  This function evaluates the right-hand side operator, the
   *  convective and viscous terms (subsequently multiplied by -1.0 in order
   *  to shift these terms to the right-hand side of the equations)
   *  and finally applies the inverse mass matrix operator.
   */
  void
  evaluate(VectorType & dst, VectorType const & src, Number const time) const;

  void
  evaluate_convective(VectorType & dst, VectorType const & src, Number const time) const;

  void
  evaluate_viscous(VectorType & dst, VectorType const & src, Number const time) const;

  void
  evaluate_convective_and_viscous(VectorType &       dst,
                                  VectorType const & src,
                                  Number const       time) const;

  void
  apply_inverse_mass(VectorType & dst, VectorType const & src) const;

  // getters
  MatrixFree<dim, Number> const &
  get_matrix_free() const;

  Mapping<dim> const &
  get_mapping() const;

  FESystem<dim> const &
  get_fe() const;

  DoFHandler<dim> const &
  get_dof_handler() const;

  DoFHandler<dim> const &
  get_dof_handler_scalar() const;

  DoFHandler<dim> const &
  get_dof_handler_vector() const;

  unsigned int
  get_dof_index_vector() const;

  unsigned int
  get_dof_index_scalar() const;

  unsigned int
  get_quad_index_standard() const;

  // pressure
  void
  compute_pressure(VectorType & dst, VectorType const & src) const;

  // velocity
  void
  compute_velocity(VectorType & dst, VectorType const & src) const;

  // temperature
  void
  compute_temperature(VectorType & dst, VectorType const & src) const;

  // vorticity
  void
  compute_vorticity(VectorType & dst, VectorType const & src) const;

  // divergence
  void
  compute_divergence(VectorType & dst, VectorType const & src) const;

  double
  get_wall_time_operator_evaluation() const;

  double
  calculate_minimum_element_length() const;

  unsigned int
  get_polynomial_degree() const;

private:
  void
  distribute_dofs();

  void
  setup_operators();

  unsigned int
  get_dof_index_all() const;

  unsigned int
  get_quad_index_overintegration_conv() const;

  unsigned int
  get_quad_index_overintegration_vis() const;

  unsigned int
  get_quad_index_l2_projections() const;

  /*
   * Mapping
   */
  Mapping<dim> const & mapping;

  /*
   * polynomial degree
   */
  unsigned int const degree;

  /*
   * User interface: Boundary conditions and field functions.
   */
  std::shared_ptr<BoundaryDescriptor<dim>>       boundary_descriptor_density;
  std::shared_ptr<BoundaryDescriptor<dim>>       boundary_descriptor_velocity;
  std::shared_ptr<BoundaryDescriptor<dim>>       boundary_descriptor_pressure;
  std::shared_ptr<BoundaryDescriptorEnergy<dim>> boundary_descriptor_energy;
  std::shared_ptr<FieldFunctions<dim>>           field_functions;

  /*
   * List of input parameters.
   */
  InputParameters const & param;

  /*
   * Basic finite element ingredients.
   */

  std::shared_ptr<FESystem<dim>> fe;        // all (dim+2) components: (rho, rho u, rho E)
  std::shared_ptr<FESystem<dim>> fe_vector; // e.g. velocity
  FE_DGQ<dim>                    fe_scalar; // scalar quantity, e.g, pressure

  // Quadrature points
  unsigned int n_q_points_conv;
  unsigned int n_q_points_visc;

  // DoFHandler
  DoFHandler<dim> dof_handler;        // all (dim+2) components: (rho, rho u, rho E)
  DoFHandler<dim> dof_handler_vector; // e.g. velocity
  DoFHandler<dim> dof_handler_scalar; // scalar quantity, e.g, pressure

  /*
   * Constraints.
   */
  AffineConstraints<double> constraint;


  std::string const dof_index_all    = "all_fields";
  std::string const dof_index_vector = "vector";
  std::string const dof_index_scalar = "scalar";

  std::string const quad_index_standard             = "standard";
  std::string const quad_index_overintegration_conv = "overintegration_conv";
  std::string const quad_index_overintegration_vis  = "overintegration_vis";

  std::string const quad_index_l2_projections = quad_index_standard;
  // alternative: use more accurate over-integration strategy
  //  std::string const quad_index_l2_projections = quad_index_overintegration_conv;

  mutable std::string field;

  /*
   * Matrix-free operator evaluation.
   */
  std::shared_ptr<MatrixFreeWrapper<dim, Number>> matrix_free_wrapper;
  std::shared_ptr<MatrixFree<dim, Number>>        matrix_free;

  /*
   * Basic operators.
   */
  MassMatrixOperator<dim, Number> mass_matrix_operator;
  BodyForceOperator<dim, Number>  body_force_operator;
  ConvectiveOperator<dim, Number> convective_operator;
  ViscousOperator<dim, Number>    viscous_operator;

  /*
   * Merged operators.
   */
  CombinedOperator<dim, Number> combined_operator;

  InverseMassMatrixOperator<dim, dim + 2, Number> inverse_mass_all;
  InverseMassMatrixOperator<dim, dim, Number>     inverse_mass_vector;
  InverseMassMatrixOperator<dim, 1, Number>       inverse_mass_scalar;

  // L2 projections to calculate derived quantities
  p_u_T_Calculator<dim, Number>     p_u_T_calculator;
  VorticityCalculator<dim, Number>  vorticity_calculator;
  DivergenceCalculator<dim, Number> divergence_calculator;

  /*
   * MPI
   */
  MPI_Comm const & mpi_comm;

  /*
   * Output to screen.
   */
  ConditionalOStream pcout;

  // wall time for operator evaluation
  mutable double wall_time_operator_evaluation;
};

} // namespace CompNS

#endif /* INCLUDE_CONVECTION_DIFFUSION_DG_CONVECTION_DIFFUSION_OPERATION_H_ */
