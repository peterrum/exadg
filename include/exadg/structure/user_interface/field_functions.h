/*
 * field_functions.h
 *
 *  Created on: 21.03.2020
 *      Author: fehn
 */


#ifndef INCLUDE_EXADG_STRUCTURE_USER_INTERFACE_FIELD_FUNCTIONS_H_
#define INCLUDE_EXADG_STRUCTURE_USER_INTERFACE_FIELD_FUNCTIONS_H_

namespace ExaDG
{
namespace Structure
{
using namespace dealii;

template<int dim>
struct FieldFunctions
{
  std::shared_ptr<Function<dim>> right_hand_side;
  std::shared_ptr<Function<dim>> initial_displacement;
  std::shared_ptr<Function<dim>> initial_velocity;
};

} // namespace Structure
} // namespace ExaDG

#endif
