/*
 * analytical_solution.h
 *
 *  Created on: Oct 11, 2016
 *      Author: fehn
 */

#ifndef INCLUDE_EXADG_INCOMPRESSIBLE_NAVIER_STOKES_USER_INTERFACE_ANALYTICAL_SOLUTION_H_
#define INCLUDE_EXADG_INCOMPRESSIBLE_NAVIER_STOKES_USER_INTERFACE_ANALYTICAL_SOLUTION_H_

#include <deal.II/base/function.h>

namespace ExaDG
{
namespace IncNS
{
using namespace dealii;

template<int dim>
struct AnalyticalSolution
{
  /*
   *  velocity
   */
  std::shared_ptr<Function<dim>> velocity;

  /*
   *  pressure
   */
  std::shared_ptr<Function<dim>> pressure;
};

} // namespace IncNS
} // namespace ExaDG

#endif /* INCLUDE_EXADG_INCOMPRESSIBLE_NAVIER_STOKES_USER_INTERFACE_ANALYTICAL_SOLUTION_H_ */
