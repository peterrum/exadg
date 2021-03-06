/*
 * input_parameters.cpp
 *
 *  Created on: May 14, 2019
 *      Author: fehn
 */

// deal.II
#include <deal.II/base/exceptions.h>

// ExaDG
#include <exadg/poisson/user_interface/input_parameters.h>

namespace ExaDG
{
namespace Poisson
{
using namespace dealii;

InputParameters::InputParameters()
  : // MATHEMATICAL MODEL
    right_hand_side(false),

    // SPATIAL DISCRETIZATION
    triangulation_type(TriangulationType::Undefined),
    mapping(MappingType::Affine),
    spatial_discretization(SpatialDiscretization::Undefined),
    IP_factor(1.0),

    // SOLVER
    solver(Solver::Undefined),
    solver_data(SolverData(1e4, 1.e-20, 1.e-12)),
    compute_performance_metrics(false),
    preconditioner(Preconditioner::Undefined),
    multigrid_data(MultigridData()),
    enable_cell_based_face_loops(false)
{
}

void
InputParameters::check_input_parameters()
{
  // MATHEMATICAL MODEL

  // SPATIAL DISCRETIZATION
  AssertThrow(triangulation_type != TriangulationType::Undefined,
              ExcMessage("parameter must be defined."));

  AssertThrow(spatial_discretization != SpatialDiscretization::Undefined,
              ExcMessage("parameter must be defined."));

  // SOLVER
  AssertThrow(solver != Solver::Undefined, ExcMessage("parameter must be defined."));
  AssertThrow(preconditioner != Preconditioner::Undefined,
              ExcMessage("parameter must be defined."));
}

void
InputParameters::print(ConditionalOStream & pcout, std::string const & name)
{
  pcout << std::endl << name << std::endl;

  // MATHEMATICAL MODEL
  print_parameters_mathematical_model(pcout);

  // SPATIAL DISCRETIZATION
  print_parameters_spatial_discretization(pcout);

  // SOLVER
  print_parameters_solver(pcout);

  // NUMERICAL PARAMETERS
  print_parameters_numerical_parameters(pcout);
}

void
InputParameters::print_parameters_mathematical_model(ConditionalOStream & pcout)
{
  pcout << std::endl << "Mathematical model:" << std::endl;

  print_parameter(pcout, "Right-hand side", right_hand_side);
}

void
InputParameters::print_parameters_spatial_discretization(ConditionalOStream & pcout)
{
  pcout << std::endl << "Spatial Discretization:" << std::endl;

  print_parameter(pcout, "Triangulation type", enum_to_string(triangulation_type));

  print_parameter(pcout, "Mapping", enum_to_string(mapping));

  print_parameter(pcout, "Element type", enum_to_string(spatial_discretization));

  if(spatial_discretization == SpatialDiscretization::DG)
    print_parameter(pcout, "IP factor", IP_factor);
}

void
InputParameters::print_parameters_solver(ConditionalOStream & pcout)
{
  pcout << std::endl << "Solver:" << std::endl;

  print_parameter(pcout, "Solver", enum_to_string(solver));

  solver_data.print(pcout);

  print_parameter(pcout, "Preconditioner", enum_to_string(preconditioner));

  if(preconditioner == Preconditioner::Multigrid)
    multigrid_data.print(pcout);
}


void
InputParameters::print_parameters_numerical_parameters(ConditionalOStream & pcout)
{
  pcout << std::endl << "Numerical parameters:" << std::endl;

  print_parameter(pcout, "Enable cell-based face loops", enable_cell_based_face_loops);
}


} // namespace Poisson
} // namespace ExaDG
