/*
 * output_data.h
 *
 *  Created on: Oct 12, 2016
 *      Author: fehn
 */

#ifndef INCLUDE_EXADG_POSTPROCESSOR_OUTPUT_DATA_BASE_H_
#define INCLUDE_EXADG_POSTPROCESSOR_OUTPUT_DATA_BASE_H_

#include <exadg/utilities/print_functions.h>

namespace ExaDG
{
using namespace dealii;

struct OutputDataBase
{
  OutputDataBase()
    : write_output(false),
      output_counter_start(0),
      output_folder("output"),
      output_name("name"),
      output_start_time(std::numeric_limits<double>::max()),
      output_interval_time(std::numeric_limits<double>::max()),
      write_surface_mesh(false),
      write_boundary_IDs(false),
      write_grid(false),
      write_processor_id(false),
      write_higher_order(true),
      degree(1)
  {
  }

  void
  print(ConditionalOStream & pcout, bool unsteady)
  {
    // output for visualization of results
    print_parameter(pcout, "Write output", write_output);

    if(write_output == true)
    {
      print_parameter(pcout, "Output counter start", output_counter_start);
      print_parameter(pcout, "Output folder", output_folder);
      print_parameter(pcout, "Name of output files", output_name);

      if(unsteady == true)
      {
        print_parameter(pcout, "Output start time", output_start_time);
        print_parameter(pcout, "Output interval time", output_interval_time);
      }

      print_parameter(pcout, "Write surface mesh", write_surface_mesh);
      print_parameter(pcout, "Write boundary IDs", write_boundary_IDs);

      print_parameter(pcout, "Write processor ID", write_processor_id);

      print_parameter(pcout, "Write higher order", write_higher_order);
      print_parameter(pcout, "Polynomial degree", degree);
    }
  }

  // set write_output = true in order to write files for visualization
  bool write_output;

  unsigned int output_counter_start;

  // output_folder
  std::string output_folder;

  // name of generated output files
  std::string output_name;

  // before then no output will be written
  double output_start_time;

  // specifies the time interval in which output is written
  double output_interval_time;

  // this variable decides whether the surface mesh is written separately
  bool write_surface_mesh;

  // this variable decides whether a vtk-file is written that allows a visualization of boundary
  // IDs, e.g., to verify that boundary IDs have been set correctly. Note that in the current
  // version of deal.II, boundaries with ID = 0 (default) are not visible, but only those with
  // ID != 0.
  bool write_boundary_IDs;

  // write grid output for debug meshing
  bool write_grid;

  // write processor ID to scalar field in order to visualize the
  // distribution of cells to processors
  bool write_processor_id;

  // write higher order output (NOTE: requires at least ParaView version 5.5, switch off if ParaView
  // version is lower)
  bool write_higher_order;

  // defines polynomial degree used for output (for visualization in ParaView: Properties >
  // Miscellaneous > Nonlinear Subdivision Level (use a value > 1)) if write_higher_order = true. In
  // case of write_higher_order = false, this variable defines the number of subdivisions of a cell,
  // with ParaView using linear interpolation for visualization on these subdivided cells.
  unsigned int degree;
};

} // namespace ExaDG

#endif /* INCLUDE_EXADG_POSTPROCESSOR_OUTPUT_DATA_BASE_H_ */
