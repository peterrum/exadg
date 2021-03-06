/*
 * convection_diffusion_operator_merged.h
 *
 *  Created on: Jun 6, 2019
 *      Author: fehn
 */

#ifndef INCLUDE_EXADG_CONVECTION_DIFFUSION_SPATIAL_DISCRETIZATION_OPERATORS_COMBINED_OPERATOR_H_
#define INCLUDE_EXADG_CONVECTION_DIFFUSION_SPATIAL_DISCRETIZATION_OPERATORS_COMBINED_OPERATOR_H_

#include <exadg/convection_diffusion/spatial_discretization/operators/convective_operator.h>
#include <exadg/convection_diffusion/spatial_discretization/operators/diffusive_operator.h>
#include <exadg/operators/mass_kernel.h>

namespace ExaDG
{
namespace ConvDiff
{
using namespace dealii;

template<int dim>
struct CombinedOperatorData : public OperatorBaseData
{
  CombinedOperatorData()
    : OperatorBaseData(),
      unsteady_problem(false),
      convective_problem(false),
      diffusive_problem(false)
  {
  }

  bool unsteady_problem;
  bool convective_problem;
  bool diffusive_problem;

  Operators::ConvectiveKernelData<dim> convective_kernel_data;
  Operators::DiffusiveKernelData       diffusive_kernel_data;

  std::shared_ptr<BoundaryDescriptor<dim>> bc;
};

template<int dim, typename Number>
class CombinedOperator : public OperatorBase<dim, Number, 1>
{
public:
  typedef Number value_type;

private:
  typedef OperatorBase<dim, Number, 1> Base;

  typedef typename Base::IntegratorCell IntegratorCell;
  typedef typename Base::IntegratorFace IntegratorFace;

  typedef typename Base::VectorType VectorType;

  typedef VectorizedArray<Number>                 scalar;
  typedef Tensor<1, dim, VectorizedArray<Number>> vector;

public:
  CombinedOperator();

  void
  initialize(MatrixFree<dim, Number> const &   matrix_free,
             AffineConstraints<Number> const & affine_constraints,
             CombinedOperatorData<dim> const & data);

  void
  initialize(MatrixFree<dim, Number> const &                           matrix_free,
             AffineConstraints<Number> const &                         affine_constraints,
             CombinedOperatorData<dim> const &                         data,
             std::shared_ptr<Operators::ConvectiveKernel<dim, Number>> convective_kernel,
             std::shared_ptr<Operators::DiffusiveKernel<dim, Number>>  diffusive_kernel);

  CombinedOperatorData<dim> const &
  get_data() const;

  void
  update_after_mesh_movement();

  LinearAlgebra::distributed::Vector<Number> const &
  get_velocity() const;

  void
  set_velocity_copy(VectorType const & velocity) const;

  void
  set_velocity_ptr(VectorType const & velocity) const;

  Number
  get_scaling_factor_mass_operator() const;

  void
  set_scaling_factor_mass_operator(Number const & scaling_factor);

private:
  void
  reinit_cell(unsigned int const cell) const;

  void
  reinit_face(unsigned int const face) const;

  void
  reinit_boundary_face(unsigned int const face) const;

  void
  reinit_face_cell_based(unsigned int const       cell,
                         unsigned int const       face,
                         types::boundary_id const boundary_id) const;

  void
  do_cell_integral(IntegratorCell & integrator) const;

  void
  do_face_integral(IntegratorFace & integrator_m, IntegratorFace & integrator_p) const;

  void
  do_face_int_integral(IntegratorFace & integrator_m, IntegratorFace & integrator_p) const;

  void
  do_face_ext_integral(IntegratorFace & integrator_m, IntegratorFace & integrator_p) const;

  void
  do_boundary_integral(IntegratorFace &           integrator_m,
                       OperatorType const &       operator_type,
                       types::boundary_id const & boundary_id) const;

  // TODO can be removed later once matrix-free evaluation allows accessing neighboring data for
  // cell-based face loops
  void
  do_face_int_integral_cell_based(IntegratorFace & integrator_m,
                                  IntegratorFace & integrator_p) const;

  CombinedOperatorData<dim> operator_data;

  std::shared_ptr<MassKernel<dim, Number>>                  mass_kernel;
  std::shared_ptr<Operators::ConvectiveKernel<dim, Number>> convective_kernel;
  std::shared_ptr<Operators::DiffusiveKernel<dim, Number>>  diffusive_kernel;

  double scaling_factor_mass;
};

} // namespace ConvDiff
} // namespace ExaDG

#endif /* INCLUDE_EXADG_CONVECTION_DIFFUSION_SPATIAL_DISCRETIZATION_OPERATORS_COMBINED_OPERATOR_H_ \
        */
