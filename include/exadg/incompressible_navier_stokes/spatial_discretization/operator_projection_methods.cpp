/*
 * dg_projection_methods.cpp
 *
 *  Created on: Dec 6, 2018
 *      Author: fehn
 */

#include <exadg/incompressible_navier_stokes/spatial_discretization/operator_projection_methods.h>
#include <exadg/poisson/preconditioner/multigrid_preconditioner.h>
#include <exadg/solvers_and_preconditioners/util/check_multigrid.h>

namespace ExaDG
{
namespace IncNS
{
using namespace dealii;

template<int dim, typename Number>
OperatorProjectionMethods<dim, Number>::OperatorProjectionMethods(
  parallel::TriangulationBase<dim> const & triangulation_in,
  Mapping<dim> const &                     mapping_in,
  unsigned int const                       degree_u_in,
  std::vector<GridTools::PeriodicFacePair<typename Triangulation<dim>::cell_iterator>> const
                                                  periodic_face_pairs_in,
  std::shared_ptr<BoundaryDescriptorU<dim>> const boundary_descriptor_velocity_in,
  std::shared_ptr<BoundaryDescriptorP<dim>> const boundary_descriptor_pressure_in,
  std::shared_ptr<FieldFunctions<dim>> const      field_functions_in,
  InputParameters const &                         parameters_in,
  std::string const &                             field_in,
  MPI_Comm const &                                mpi_comm_in)
  : Base(triangulation_in,
         mapping_in,
         degree_u_in,
         periodic_face_pairs_in,
         boundary_descriptor_velocity_in,
         boundary_descriptor_pressure_in,
         field_functions_in,
         parameters_in,
         field_in,
         mpi_comm_in)
{
  AssertThrow(this->param.get_degree_p(degree_u_in) > 0,
              ExcMessage("Polynomial degree of pressure shape functions has to be larger than "
                         "zero for projection methods."));
}

template<int dim, typename Number>
OperatorProjectionMethods<dim, Number>::~OperatorProjectionMethods()
{
}

template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::setup(
  std::shared_ptr<MatrixFree<dim, Number>>     matrix_free,
  std::shared_ptr<MatrixFreeData<dim, Number>> matrix_free_data,
  std::string const &                          dof_index_temperature)
{
  Base::setup(matrix_free, matrix_free_data, dof_index_temperature);

  initialize_laplace_operator();
}

template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::update_after_mesh_movement()
{
  Base::update_after_mesh_movement();

  // update SIPG penalty parameter of Laplace operator which depends on the deformation
  // of elements
  laplace_operator.update_after_mesh_movement();
}

template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::setup_pressure_poisson_solver()
{
  initialize_preconditioner_pressure_poisson();

  initialize_solver_pressure_poisson();
}

template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::initialize_laplace_operator()
{
  // setup Laplace operator
  Poisson::LaplaceOperatorData<0, dim> laplace_operator_data;
  laplace_operator_data.dof_index  = this->get_dof_index_pressure();
  laplace_operator_data.quad_index = this->get_quad_index_pressure();

  /*
   * In case no Dirichlet boundary conditions as prescribed for the pressure, the pressure Poisson
   * equation is singular (i.e. vectors describing a constant pressure state form the nullspace
   * of the discrete pressure Poisson operator). To solve the pressure Poisson equation in that
   * case, a Krylov subspace projection is applied during the solution of the linear system of
   * equations.
   *
   * Strictly speaking, this projection is only needed if the linear system of equations
   *
   *  A * p = rhs (with nullspace spanned by vector p0)
   *
   * is not consistent, meaning that
   *
   *  p0^T * rhs != 0 ,
   *
   *  so that
   *
   *  p0^T * A * p = (A^T * p0)^T * p = 0 != p0 * rhs (since A is symmetric).
   */
  if(this->param.temporal_discretization == TemporalDiscretization::BDFDualSplittingScheme)
  {
    // the Krylov subspace projection is needed for the dual splitting scheme since the linear
    // system of equations is not consistent for this splitting method (due to the boundary
    // conditions).
    laplace_operator_data.operator_is_singular = this->is_pressure_level_undefined();
  }
  else if(this->param.temporal_discretization == TemporalDiscretization::BDFPressureCorrection)
  {
    // One can show that the linear system of equations of the pressure Poisson equation is
    // consistent in case of the pressure-correction scheme if the velocity Dirichlet BC is
    // consistent. So there should be no need to solve a transformed linear system of equations.
    //    laplace_operator_data.operator_is_singular = false;

    // In principle, it works (since the linear system of equations is consistent)
    // but we detected no convergence for some test cases and specific parameters.
    // Hence, for reasons of robustness we also solve a transformed linear system of equations
    // in case of the pressure-correction scheme.
    laplace_operator_data.operator_is_singular = this->is_pressure_level_undefined();
  }

  laplace_operator_data.bc                   = this->boundary_descriptor_laplace;
  laplace_operator_data.use_cell_based_loops = this->param.use_cell_based_face_loops;
  laplace_operator_data.implement_block_diagonal_preconditioner_matrix_free =
    this->param.implement_block_diagonal_preconditioner_matrix_free;

  laplace_operator_data.kernel_data.IP_factor = this->param.IP_factor_pressure;

  laplace_operator.initialize(this->get_matrix_free(),
                              this->get_constraint_p(),
                              laplace_operator_data);
}

template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::initialize_preconditioner_pressure_poisson()
{
  // setup preconditioner
  if(this->param.preconditioner_pressure_poisson == PreconditionerPressurePoisson::None)
  {
    // do nothing, preconditioner will not be used
  }
  else if(this->param.preconditioner_pressure_poisson == PreconditionerPressurePoisson::PointJacobi)
  {
    preconditioner_pressure_poisson.reset(
      new JacobiPreconditioner<Poisson::LaplaceOperator<dim, Number, 1>>(laplace_operator));
  }
  else if(this->param.preconditioner_pressure_poisson == PreconditionerPressurePoisson::Multigrid)
  {
    MultigridData mg_data;
    mg_data = this->param.multigrid_data_pressure_poisson;

    typedef Poisson::MultigridPreconditioner<dim, Number, 1> Multigrid;

    preconditioner_pressure_poisson.reset(new Multigrid(this->mpi_comm));

    std::shared_ptr<Multigrid> mg_preconditioner =
      std::dynamic_pointer_cast<Multigrid>(preconditioner_pressure_poisson);

    parallel::TriangulationBase<dim> const * tria =
      dynamic_cast<const parallel::TriangulationBase<dim> *>(
        &this->get_dof_handler_p().get_triangulation());
    const FiniteElement<dim> & fe = this->get_dof_handler_p().get_fe();

    mg_preconditioner->initialize(mg_data,
                                  tria,
                                  fe,
                                  this->get_mapping(),
                                  laplace_operator.get_data(),
                                  this->param.ale_formulation,
                                  &laplace_operator.get_data().bc->dirichlet_bc,
                                  &this->periodic_face_pairs);
  }
  else
  {
    AssertThrow(
      false, ExcMessage("Specified preconditioner for pressure Poisson equation not implemented"));
  }
}

template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::initialize_solver_pressure_poisson()
{
  if(this->param.solver_pressure_poisson == SolverPressurePoisson::CG)
  {
    // setup solver data
    CGSolverData solver_data;
    solver_data.max_iter             = this->param.solver_data_pressure_poisson.max_iter;
    solver_data.solver_tolerance_abs = this->param.solver_data_pressure_poisson.abs_tol;
    solver_data.solver_tolerance_rel = this->param.solver_data_pressure_poisson.rel_tol;
    // use default value of update_preconditioner (=false)

    if(this->param.preconditioner_pressure_poisson != PreconditionerPressurePoisson::None)
    {
      solver_data.use_preconditioner = true;
    }

    // setup solver
    pressure_poisson_solver.reset(
      new CGSolver<Poisson::LaplaceOperator<dim, Number, 1>,
                   PreconditionerBase<Number>,
                   VectorType>(laplace_operator, *preconditioner_pressure_poisson, solver_data));
  }
  else if(this->param.solver_pressure_poisson == SolverPressurePoisson::FGMRES)
  {
    FGMRESSolverData solver_data;
    solver_data.max_iter             = this->param.solver_data_pressure_poisson.max_iter;
    solver_data.solver_tolerance_abs = this->param.solver_data_pressure_poisson.abs_tol;
    solver_data.solver_tolerance_rel = this->param.solver_data_pressure_poisson.rel_tol;
    solver_data.max_n_tmp_vectors    = this->param.solver_data_pressure_poisson.max_krylov_size;
    // use default value of update_preconditioner (=false)

    if(this->param.preconditioner_pressure_poisson != PreconditionerPressurePoisson::None)
    {
      solver_data.use_preconditioner = true;
    }

    pressure_poisson_solver.reset(new FGMRESSolver<Poisson::LaplaceOperator<dim, Number, 1>,
                                                   PreconditionerBase<Number>,
                                                   VectorType>(laplace_operator,
                                                               *preconditioner_pressure_poisson,
                                                               solver_data));
  }
  else
  {
    AssertThrow(false,
                ExcMessage("Specified solver for pressure Poisson equation is not implemented."));
  }
}

template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::do_rhs_add_viscous_term(VectorType & dst,
                                                                double const time) const
{
  this->viscous_operator.set_time(time);
  this->viscous_operator.rhs_add(dst);
}

template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::do_rhs_ppe_laplace_add(VectorType &   dst,
                                                               double const & time) const
{
  this->laplace_operator.set_time(time);
  this->laplace_operator.rhs_add(dst);
}

template<int dim, typename Number>
unsigned int
OperatorProjectionMethods<dim, Number>::do_solve_pressure(VectorType &       dst,
                                                          VectorType const & src,
                                                          bool const update_preconditioner) const
{
  // Check multigrid algorithm
  //  std::shared_ptr<MultigridPoisson> mg_preconditioner
  //    = std::dynamic_pointer_cast<MultigridPoisson>(preconditioner_pressure_poisson);
  //
  //  CheckMultigrid<dim, Number, Poisson::LaplaceOperator<dim, Number>, MultigridPoisson,
  //  MultigridNumber>
  //    check_multigrid(this->laplace_operator, mg_preconditioner, this->mpi_comm);
  //  check_multigrid.check();

  // Use multigrid as a solver (use double precision here)
  //  std::shared_ptr<MultigridPoisson> mg_preconditioner
  //    = std::dynamic_pointer_cast<MultigridPoisson>(preconditioner_pressure_poisson);
  //  unsigned int n_iter = mg_preconditioner->solve(dst,src);

  // call pressure Poisson solver
  unsigned int n_iter = this->pressure_poisson_solver->solve(dst, src, update_preconditioner);

  return n_iter;
}


template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::apply_laplace_operator(VectorType &       dst,
                                                               VectorType const & src) const
{
  this->laplace_operator.vmult(dst, src);
}

template<int dim, typename Number>
void
OperatorProjectionMethods<dim, Number>::apply_projection_operator(VectorType &       dst,
                                                                  VectorType const & src) const
{
  AssertThrow(this->projection_operator.get() != 0,
              ExcMessage("Projection operator is not initialized correctly."));

  this->projection_operator->vmult(dst, src);
}

template class OperatorProjectionMethods<2, float>;
template class OperatorProjectionMethods<2, double>;

template class OperatorProjectionMethods<3, float>;
template class OperatorProjectionMethods<3, double>;

} // namespace IncNS
} // namespace ExaDG
