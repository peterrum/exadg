/*
 * throughput_study.h
 *
 *  Created on: 24.03.2020
 *      Author: fehn
 */

#ifndef INCLUDE_EXADG_UTILITIES_THROUGHPUT_PARAMETERS_H_
#define INCLUDE_EXADG_UTILITIES_THROUGHPUT_PARAMETERS_H_

// deal.II
#include <deal.II/base/parameter_handler.h>

// ExaDG
#include "print_solver_results.h"

namespace ExaDG
{
using namespace dealii;

inline double
measure_operator_evaluation_time(std::function<void(void)> const & evaluate_operator,
                                 unsigned int const                degree,
                                 unsigned int const                n_repetitions_inner,
                                 unsigned int const                n_repetitions_outer,
                                 MPI_Comm const &                  mpi_comm)
{
  (void)degree;

  Timer global_timer;
  global_timer.restart();
  Utilities::MPI::MinMaxAvg global_time;

  double wall_time = std::numeric_limits<double>::max();

  do
  {
    for(unsigned int i_outer = 0; i_outer < n_repetitions_outer; ++i_outer)
    {
      Timer timer;
      timer.restart();

#ifdef LIKWID_PERFMON
      LIKWID_MARKER_START(("degree_" + std::to_string(degree)).c_str());
#endif

      // apply matrix-vector product several times
      for(unsigned int i = 0; i < n_repetitions_inner; ++i)
      {
        evaluate_operator();
      }

#ifdef LIKWID_PERFMON
      LIKWID_MARKER_STOP(("degree_" + std::to_string(degree)).c_str());
#endif

      MPI_Barrier(mpi_comm);
      Utilities::MPI::MinMaxAvg wall_time_inner =
        Utilities::MPI::min_max_avg(timer.wall_time(), mpi_comm);

      wall_time = std::min(wall_time, wall_time_inner.avg / (double)n_repetitions_inner);
    }

    global_time = Utilities::MPI::min_max_avg(global_timer.wall_time(), mpi_comm);
  } while(global_time.avg < 1.0 /*wall time in seconds*/);

  return wall_time;
}

struct ThroughputParameters
{
  ThroughputParameters()
  {
  }

  ThroughputParameters(const std::string & input_file)
  {
    dealii::ParameterHandler prm;
    add_parameters(prm);
    prm.parse_input(input_file, "", true, true);
  }

  void
  add_parameters(dealii::ParameterHandler & prm)
  {
    // clang-format off
    prm.enter_subsection("Throughput");
      prm.add_parameter("OperatorType",
                        operator_type,
                        "Type of operator.",
                        Patterns::Anything(),
                        true);
      prm.add_parameter("RepetitionsInner",
                        n_repetitions_inner,
                        "Number of operator evaluations.",
                        Patterns::Integer(1),
                        true);
      prm.add_parameter("RepetitionsOuter",
                        n_repetitions_outer,
                        "Number of runs (taking minimum wall time).",
                        Patterns::Integer(1,10),
                        true);
    prm.leave_subsection();
    // clang-format on
  }

  void
  print_results(MPI_Comm const & mpi_comm)
  {
    print_throughput(wall_times, operator_type, mpi_comm);
  }

  std::string operator_type = "Undefined";

  // number of repetitions used to determine the average/minimum wall time required
  // to compute the matrix-vector product
  unsigned int n_repetitions_inner = 100; // take the average of inner repetitions
  unsigned int n_repetitions_outer = 1;   // take the minimum of outer repetitions

  // global variable used to store the wall times for different polynomial degrees and problem sizes
  mutable std::vector<std::tuple<unsigned int, types::global_dof_index, double>> wall_times;
};
} // namespace ExaDG


#endif /* INCLUDE_EXADG_UTILITIES_THROUGHPUT_PARAMETERS_H_ */
