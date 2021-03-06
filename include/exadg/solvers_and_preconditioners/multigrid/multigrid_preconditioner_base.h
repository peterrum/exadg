/*
 * multigrid_preconditioner_base.h
 *
 *  Created on: Nov 23, 2016
 *      Author: fehn
 */

#ifndef INCLUDE_SOLVERS_AND_PRECONDITIONERS_MULTIGRID_PRECONDITIONER_ADAPTER_BASE_H_
#define INCLUDE_SOLVERS_AND_PRECONDITIONERS_MULTIGRID_PRECONDITIONER_ADAPTER_BASE_H_

#include <exadg/matrix_free/matrix_free_wrapper.h>
#include <exadg/operators/multigrid_operator_base.h>
#include <exadg/solvers_and_preconditioners/multigrid/coarse_grid_solvers.h>
#include <exadg/solvers_and_preconditioners/multigrid/levels_hybrid_multigrid.h>
#include <exadg/solvers_and_preconditioners/multigrid/multigrid_algorithm.h>
#include <exadg/solvers_and_preconditioners/multigrid/multigrid_input_parameters.h>
#include <exadg/solvers_and_preconditioners/multigrid/smoothers/smoother_base.h>
#include <exadg/solvers_and_preconditioners/multigrid/transfer/mg_transfer_global_coarsening.h>
#include <exadg/solvers_and_preconditioners/multigrid/transfer/mg_transfer_mg_level_object.h>

namespace ExaDG
{
using namespace dealii;

template<int dim, typename Number>
class MultigridPreconditionerBase : public PreconditionerBase<Number>
{
public:
  typedef float MultigridNumber;

protected:
  typedef std::map<types::boundary_id, std::shared_ptr<Function<dim>>> Map;
  typedef std::vector<GridTools::PeriodicFacePair<typename Triangulation<dim>::cell_iterator>>
    PeriodicFacePairs;

  typedef LinearAlgebra::distributed::Vector<Number>          VectorType;
  typedef LinearAlgebra::distributed::Vector<MultigridNumber> VectorTypeMG;

private:
  typedef MultigridOperatorBase<dim, MultigridNumber> Operator;

  typedef std::vector<std::pair<unsigned int, unsigned int>> Levels;

public:
  /*
   * Constructor.
   */
  MultigridPreconditionerBase(MPI_Comm const & comm);

  /*
   * Destructor.
   */
  virtual ~MultigridPreconditionerBase()
  {
  }

  /*
   * Initialization function.
   */
  void
  initialize(MultigridData const &                    data,
             parallel::TriangulationBase<dim> const * tria,
             FiniteElement<dim> const &               fe,
             Mapping<dim> const &                     mapping,
             bool const                               operator_is_singular = false,
             Map const *                              dirichlet_bc         = nullptr,
             PeriodicFacePairs *                      periodic_face_pairs  = nullptr);

  /*
   * This function applies the multigrid preconditioner dst = P^{-1} src.
   */
  void
  vmult(VectorType & dst, VectorType const & src) const;

  /*
   * Use multigrid as a solver.
   */
  unsigned int
  solve(VectorType & dst, VectorType const & src) const;

  /*
   * This function applies the smoother on the fine level as a means to test the
   * multigrid ingredients.
   */
  virtual void
  apply_smoother_on_fine_level(VectorTypeMG & dst, VectorTypeMG const & src) const;

  /*
   * Update of multigrid preconditioner including operators, smoothers, etc. (e.g. for problems
   * with time-dependent coefficients).
   */
  virtual void
  update();

protected:
  /*
   * This function initializes the matrix-free objects for all multigrid levels.
   */
  virtual void
  initialize_matrix_free();

  /*
   * This function updates the matrix-free objects for all multigrid levels, which
   * is necessary if the domain changes over time.
   */
  void
  update_matrix_free();

  /*
   * This function updates the smoother for all multigrid levels.
   * The prerequisite to call this function is that the multigrid operators have been updated.
   */
  void
  update_smoothers();

  /*
   * Update functions that have to be called/implemented by derived classes.
   */
  virtual void
  update_smoother(unsigned int level);

  virtual void
  update_coarse_solver(bool const operator_is_singular);

  /*
   * Dof-handlers and constraints.
   */
  virtual void
  initialize_dof_handler_and_constraints(bool                       is_singular,
                                         PeriodicFacePairs *        periodic_face_pairs,
                                         FiniteElement<dim> const & fe,
                                         parallel::TriangulationBase<dim> const * tria,
                                         Map const *                              dirichlet_bc);

  void
  do_initialize_dof_handler_and_constraints(
    bool                                                                 is_singular,
    PeriodicFacePairs &                                                  periodic_face_pairs,
    FiniteElement<dim> const &                                           fe,
    parallel::TriangulationBase<dim> const *                             tria,
    Map const &                                                          dirichlet_bc,
    std::vector<MGLevelInfo> &                                           level_info,
    std::vector<MGDoFHandlerIdentifier> &                                p_levels,
    MGLevelObject<std::shared_ptr<const DoFHandler<dim>>> &              dofhandlers,
    MGLevelObject<std::shared_ptr<MGConstrainedDoFs>> &                  constrained_dofs,
    MGLevelObject<std::shared_ptr<AffineConstraints<MultigridNumber>>> & constraints);

  /*
   * Transfer operators.
   */
  virtual void
  initialize_transfer_operators();

  MGLevelObject<std::shared_ptr<const DoFHandler<dim>>>                dof_handlers;
  MGLevelObject<std::shared_ptr<MGConstrainedDoFs>>                    constrained_dofs;
  MGLevelObject<std::shared_ptr<AffineConstraints<MultigridNumber>>>   constraints;
  MGLevelObject<std::shared_ptr<MatrixFreeData<dim, MultigridNumber>>> matrix_free_data_objects;
  MGLevelObject<std::shared_ptr<MatrixFree<dim, MultigridNumber>>>     matrix_free_objects;
  MGLevelObject<std::shared_ptr<Operator>>                             operators;
  std::shared_ptr<MGTransfer<VectorTypeMG>>                            transfers;
  std::vector<std::shared_ptr<Triangulation<dim>>>                     coarse_grid_triangulations;

  Mapping<dim> const * mapping;

  std::vector<MGDoFHandlerIdentifier> p_levels;
  std::vector<MGLevelInfo>            level_info;
  unsigned int                        n_levels;
  unsigned int                        coarse_level;
  unsigned int                        fine_level;

private:
  bool
  mg_transfer_to_continuous_elements() const;

  /*
   * Multigrid levels (i.e. coarsening strategy, h-/p-/hp-/ph-MG).
   */
  void
  initialize_levels(parallel::TriangulationBase<dim> const * tria,
                    unsigned int const                       degree,
                    bool const                               is_dg);

  void
  check_levels(std::vector<MGLevelInfo> const & level_info);


  /*
   * Constrained dofs.
   */
  virtual void
  initialize_constrained_dofs(DoFHandler<dim> const &,
                              MGConstrainedDoFs &,
                              Map const & dirichlet_bc);

  void
  initialize_affine_constraints(DoFHandler<dim> const &              dof_handler,
                                AffineConstraints<MultigridNumber> & affine_contraints,
                                Map const &                          dirichlet_bc);

  /*
   * Data structures needed for matrix-free operator evaluation.
   */
  virtual void
  fill_matrix_free_data(MatrixFreeData<dim, MultigridNumber> & matrix_free_data,
                        unsigned int const                     level,
                        unsigned int const                     h_level) = 0;

  /*
   * Initializes the multigrid operators for all multigrid levels.
   */
  void
  initialize_operators();

  /*
   * This function initializes an operator for a specified level. It needs to be implemented by
   * derived classes.
   */
  virtual std::shared_ptr<Operator>
  initialize_operator(unsigned int const level);

  /*
   * Smoother.
   */
  void
  initialize_smoothers();

  void
  initialize_smoother(Operator & matrix, unsigned int level);

  void
  initialize_chebyshev_smoother(Operator & matrix, unsigned int level);

  /*
   * Coarse grid solver.
   */
  void
  initialize_coarse_solver(bool const operator_is_singular);

  void
  initialize_chebyshev_smoother_coarse_grid(Operator &         matrix,
                                            SolverData const & solver_data,
                                            bool const         operator_is_singular);

  /*
   * Initialization of actual multigrid algorithm.
   */
  virtual void
  initialize_multigrid_preconditioner();

  MPI_Comm const & mpi_comm;

  MultigridData data;

  typedef SmootherBase<VectorTypeMG>       SMOOTHER;
  MGLevelObject<std::shared_ptr<SMOOTHER>> smoothers;

  std::shared_ptr<MGCoarseGridBase<VectorTypeMG>> coarse_grid_solver;

  std::shared_ptr<MultigridPreconditioner<VectorTypeMG, Operator, SMOOTHER>>
    multigrid_preconditioner;
};
} // namespace ExaDG

#endif /* INCLUDE_SOLVERS_AND_PRECONDITIONERS_MULTIGRID_PRECONDITIONER_ADAPTER_BASE_H_ \
        */
