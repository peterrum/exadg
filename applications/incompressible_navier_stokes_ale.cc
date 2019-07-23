/*
 * incompressible_navier_stokes.cc
 *
 *  Created on: Oct 10, 2016
 *      Author: fehn
 */

// deal.II
#include <deal.II/base/revision.h>
#include <deal.II/distributed/tria.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/manifold_lib.h>

// postprocessor
#include "../include/incompressible_navier_stokes/postprocessor/postprocessor_base.h"

// spatial discretization
#include "../include/incompressible_navier_stokes/spatial_discretization/interface.h"
#include "../include/incompressible_navier_stokes/spatial_discretization/dg_coupled_solver.h"
#include "../include/incompressible_navier_stokes/spatial_discretization/dg_dual_splitting.h"
#include "../include/incompressible_navier_stokes/spatial_discretization/dg_pressure_correction.h"

// temporal discretization
#include "../include/incompressible_navier_stokes/time_integration/driver_steady_problems.h"
#include "../include/incompressible_navier_stokes/time_integration/time_int_bdf_coupled_solver.h"
#include "../include/incompressible_navier_stokes/time_integration/time_int_bdf_dual_splitting.h"
#include "../include/incompressible_navier_stokes/time_integration/time_int_bdf_navier_stokes.h"
#include "../include/incompressible_navier_stokes/time_integration/time_int_bdf_pressure_correction.h"

// Parameters, BCs, etc.
#include "../include/incompressible_navier_stokes/user_interface/boundary_descriptor.h"
#include "../include/incompressible_navier_stokes/user_interface/field_functions.h"
#include "../include/incompressible_navier_stokes/user_interface/input_parameters.h"

#include "../include/functionalities/print_general_infos.h"

#include "../include/time_integration/push_back_vectors.h"

using namespace dealii;
using namespace IncNS;

// specify the flow problem that has to be solved


// 2D Navier-Stokes flow
//#include "incompressible_navier_stokes_ale_test_cases/poiseuille_ale.h"
//#include "incompressible_navier_stokes_ale_test_cases/vortex_ale.h"
#include "incompressible_navier_stokes_ale_test_cases/taylor_vortex_ale.h"


template<typename Number>
class ProblemBase
{
public:
  virtual ~ProblemBase()
  {
  }

  virtual void
  setup(InputParameters const & param) = 0;

  virtual void
  solve() = 0;

  virtual void
  analyze_computing_times() const = 0;
};

template<int dim, typename Number>
class Problem : public ProblemBase<Number>
{
public:
  Problem();

  void
  setup(InputParameters const & param);

  void
  solve();

  void
  analyze_computing_times() const;

private:
  void
  print_header() const;

  void
  move_mesh(double t) const;

  void
  compute_grid_velocity();

  void
  initialize_d_grid_and_u_grid_np();

  void
  fill_d_grid();

  ConditionalOStream pcout;

  std::shared_ptr<parallel::Triangulation<dim>> triangulation;
  std::vector<GridTools::PeriodicFacePair<typename Triangulation<dim>::cell_iterator>>
    periodic_faces;

  std::shared_ptr<FieldFunctions<dim>>      field_functions;
  std::shared_ptr<BoundaryDescriptorU<dim>> boundary_descriptor_velocity;
  std::shared_ptr<BoundaryDescriptorP<dim>> boundary_descriptor_pressure;

  InputParameters param;

  typedef DGNavierStokesBase<dim, Number>               DGBase;
  typedef DGNavierStokesCoupled<dim, Number>            DGCoupled;
  typedef DGNavierStokesDualSplitting<dim, Number>      DGDualSplitting;
  typedef DGNavierStokesPressureCorrection<dim, Number> DGPressureCorrection;

  std::shared_ptr<DGBase> navier_stokes_operation;

  typedef PostProcessorBase<dim, Number> Postprocessor;

  std::shared_ptr<Postprocessor> postprocessor;

  // unsteady solvers
  typedef TimeIntBDF<Number>                   TimeInt;
  typedef TimeIntBDFCoupled<Number>            TimeIntCoupled;
  typedef TimeIntBDFDualSplitting<Number>      TimeIntDualSplitting;
  typedef TimeIntBDFPressureCorrection<Number> TimeIntPressureCorrection;

  std::shared_ptr<TimeInt> time_integrator;

  // steady solver
  typedef DriverSteadyProblems<Number> DriverSteady;

  std::shared_ptr<DriverSteady> driver_steady;

  /*
   * Computation time (wall clock time).
   */
  Timer          timer;
  mutable double overall_time;
  double         setup_time;
  mutable double         update_time;
  mutable double         move_mesh_time;
  mutable double         timer_help;

  LinearAlgebra::distributed::Vector<Number> u_grid_np;
  std::vector<LinearAlgebra::distributed::Vector<Number>> d_grid;
  std::shared_ptr<std::vector< Point< dim > >> initial_coordinates;

};

template<int dim, typename Number>
Problem<dim, Number>::Problem()
  : pcout(std::cout, Utilities::MPI::this_mpi_process(MPI_COMM_WORLD) == 0),
    overall_time(0.0),
    setup_time(0.0),
    update_time(0.0),
    move_mesh_time(0.0),
    timer_help(0.0),
    d_grid(ORDER_TIME_INTEGRATOR + 1)
{
}

template<int dim, typename Number>
void
Problem<dim, Number>::print_header() const
{
  // clang-format off
  pcout << std::endl << std::endl << std::endl
  << "_________________________________________________________________________________" << std::endl
  << "                                                                                 " << std::endl
  << "                High-order discontinuous Galerkin solver for the                 " << std::endl
  << "                     incompressible Navier-Stokes equations                      " << std::endl
  << "_________________________________________________________________________________" << std::endl
  << std::endl;
  // clang-format on
}

template<int dim, typename Number>
void
Problem<dim, Number>::setup(InputParameters const & param_in)
{
  timer.restart();

  print_header();
  print_dealii_info<Number>(pcout);
  print_MPI_info(pcout);

  // input parameters
  param = param_in;
  param.check_input_parameters();
  param.print(pcout, "List of input parameters:");

  // triangulation
  if(param.triangulation_type == TriangulationType::Distributed)
  {
    triangulation.reset(new parallel::distributed::Triangulation<dim>(
      MPI_COMM_WORLD,
      dealii::Triangulation<dim>::none,
      parallel::distributed::Triangulation<dim>::construct_multigrid_hierarchy));
  }
  else if(param.triangulation_type == TriangulationType::FullyDistributed)
  {
    triangulation.reset(new parallel::fullydistributed::Triangulation<dim>(MPI_COMM_WORLD));
  }
  else
  {
    AssertThrow(false, ExcMessage("Invalid parameter triangulation_type."));
  }

  create_grid_and_set_boundary_ids(triangulation, param.h_refinements, periodic_faces);
  print_grid_data(pcout, param.h_refinements, *triangulation);

  boundary_descriptor_velocity.reset(new BoundaryDescriptorU<dim>());
  boundary_descriptor_pressure.reset(new BoundaryDescriptorP<dim>());

  IncNS::set_boundary_conditions(boundary_descriptor_velocity, boundary_descriptor_pressure);

  // field functions and boundary conditions
  field_functions.reset(new FieldFunctions<dim>());
  set_field_functions(field_functions);

  // initialize postprocessor
  postprocessor = construct_postprocessor<dim, Number>(param);

  if(param.solver_type == SolverType::Unsteady)
  {
    // initialize navier_stokes_operation
    if(this->param.temporal_discretization == TemporalDiscretization::BDFCoupledSolution)
    {
      std::shared_ptr<DGCoupled> navier_stokes_operation_coupled;

      navier_stokes_operation_coupled.reset(new DGCoupled(*triangulation, param, postprocessor));

      navier_stokes_operation = navier_stokes_operation_coupled;

      time_integrator.reset(new TimeIntCoupled(navier_stokes_operation_coupled,
                                               navier_stokes_operation_coupled,
                                               param));
    }
    else if(this->param.temporal_discretization == TemporalDiscretization::BDFDualSplittingScheme)
    {
      std::shared_ptr<DGDualSplitting> navier_stokes_operation_dual_splitting;

      navier_stokes_operation_dual_splitting.reset(
        new DGDualSplitting(*triangulation, param, postprocessor));

      navier_stokes_operation = navier_stokes_operation_dual_splitting;

      time_integrator.reset(new TimeIntDualSplitting(navier_stokes_operation_dual_splitting,
                                                     navier_stokes_operation_dual_splitting,
                                                     param));
    }
    else if(this->param.temporal_discretization == TemporalDiscretization::BDFPressureCorrection)
    {
      std::shared_ptr<DGPressureCorrection> navier_stokes_operation_pressure_correction;

      navier_stokes_operation_pressure_correction.reset(
        new DGPressureCorrection(*triangulation, param, postprocessor));

      navier_stokes_operation = navier_stokes_operation_pressure_correction;

      time_integrator.reset(
        new TimeIntPressureCorrection(navier_stokes_operation_pressure_correction,
                                      navier_stokes_operation_pressure_correction,
                                      param));
    }
    else
    {
      AssertThrow(false, ExcMessage("Not implemented."));
    }
  }
  else if(param.solver_type == SolverType::Steady)
  {
    // initialize navier_stokes_operation
    std::shared_ptr<DGCoupled> navier_stokes_operation_coupled;

    navier_stokes_operation_coupled.reset(new DGCoupled(*triangulation, param, postprocessor));

    navier_stokes_operation = navier_stokes_operation_coupled;

    // initialize driver for steady state problem that depends on navier_stokes_operation
    driver_steady.reset(
      new DriverSteady(navier_stokes_operation_coupled, navier_stokes_operation_coupled, param));
  }
  else
  {
    AssertThrow(false, ExcMessage("Not implemented."));
  }

  AssertThrow(navier_stokes_operation.get() != 0, ExcMessage("Not initialized."));
  navier_stokes_operation->setup(periodic_faces,
                                 boundary_descriptor_velocity,
                                 boundary_descriptor_pressure,
                                 field_functions);

  if(param.solver_type == SolverType::Unsteady)
  {
    // setup time integrator before calling setup_solvers
    // (this is necessary since the setup of the solvers
    // depends on quantities such as the time_step_size or gamma0!!!)
    time_integrator->setup(param.restarted_simulation);

    navier_stokes_operation->setup_solvers(
      time_integrator->get_scaling_factor_time_derivative_term(), time_integrator->get_velocity());
  }
  else if(param.solver_type == SolverType::Steady)
  {
    driver_steady->setup();

    navier_stokes_operation->setup_solvers(1.0 /* dummy */, driver_steady->get_velocity());
  }
  else
  {
    AssertThrow(false, ExcMessage("Not implemented."));
  }

  initialize_d_grid_and_u_grid_np();


  setup_time = timer.wall_time();
}


template<int dim, typename Number>
void
Problem<dim, Number>::move_mesh(double t) const
{
  triangulation->clear();

  time_dependent_mesh_generation(t, triangulation, param.h_refinements, periodic_faces);

  }

template<int dim, typename Number>
void
Problem<dim, Number>::compute_grid_velocity()
{
  push_back(d_grid);
  fill_d_grid();

  time_integrator->compute_BDF_time_derivative(u_grid_np, d_grid);

}


template<int dim, typename Number>
void
Problem<dim, Number>::initialize_d_grid_and_u_grid_np()
{

  for(unsigned int i = 0; i < d_grid.size(); ++i)
    {//TODO: Fix MPI error for 6 processes when d_grid[i] is filled
    navier_stokes_operation->initialize_vector_velocity(d_grid[i]);
    d_grid[i].update_ghost_values();
    }
  navier_stokes_operation->initialize_vector_velocity(u_grid_np);

  fill_d_grid();

  if (param.start_with_low_order==false)
  {//only possible when analytical. otherwise lower_order has to be true
    for(unsigned int i = 1; i < d_grid.size(); ++i)
    {
      //TODO: only possible if analytical solution of grid displacement can be provided
     /* move_mesh(time_integrator->get_time());
      update();*/
      push_back(d_grid);
      fill_d_grid();
    }
  }
}


template<int dim, typename Number>
void
Problem<dim, Number>::fill_d_grid()
{

  FESystem<dim> fe_grid = navier_stokes_operation->get_fe_u_grid(); //DGQ is needed to determine the right velocitys in any point
  auto tria = dynamic_cast<parallel::distributed::Triangulation<dim> *>(&*triangulation);
  DoFHandler<dim> dof_handler_grid(*tria);

  dof_handler_grid.distribute_dofs(fe_grid);
  dof_handler_grid.distribute_mg_dofs();

  IndexSet relevant_dofs_grid;
  DoFTools::extract_locally_relevant_dofs(dof_handler_grid,
      relevant_dofs_grid);

  d_grid[0].reinit(dof_handler_grid.locally_owned_dofs(), relevant_dofs_grid, MPI_COMM_WORLD);
  d_grid[0].update_ghost_values();

  FEValues<dim> fe_values(navier_stokes_operation->get_mapping(), fe_grid,
                          Quadrature<dim>(fe_grid.get_unit_support_points()),
                          update_quadrature_points);
  std::vector<types::global_dof_index> dof_indices(fe_grid.dofs_per_cell);
  for (const auto & cell : dof_handler_grid.active_cell_iterators())
  {

    if (!cell->is_artificial())
      {
        fe_values.reinit(cell);
        cell->get_dof_indices(dof_indices);
        for (unsigned int i=0; i<fe_grid.dofs_per_cell; ++i)
          {
            const unsigned int coordinate_direction =
                fe_grid.system_to_component_index(i).first;
            const Point<dim> point = fe_values.quadrature_point(i);
            d_grid[0](dof_indices[i]) = point[coordinate_direction];
          }
      }
  }
}

template<int dim, typename Number>
void
Problem<dim, Number>::solve()
{
  if(param.solver_type == SolverType::Unsteady)
  {
    // stability analysis (uncomment if desired)
    // time_integrator->postprocessing_stability_analysis();

    // run time loop
    if(this->param.problem_type == ProblemType::Steady)
      time_integrator->timeloop_steady_problem();
    else if(this->param.problem_type == ProblemType::Unsteady && param.ale_formulation == false)
      time_integrator->timeloop();
    else if(this->param.problem_type == ProblemType::Unsteady && param.ale_formulation == true )
        {
          bool timeloop_finished=false;

          if(param.mesh_movement_mappingfefield==true)
          {
            //Start at mesh t^n+1 //TODO: Check why leaving out leads to smaller errors
            navier_stokes_operation->move_mesh(time_integrator->get_next_time());
          }
          else
          {
            //Start at mesh t^n+1
            move_mesh(time_integrator->get_next_time());
          }
          navier_stokes_operation->update();

          while(!timeloop_finished)
          {
            if(param.grid_velocity_analytical==true)
            {
              navier_stokes_operation->set_analytical_grid_velocity_in_convective_operator_kernel(time_integrator->get_next_time());
            }
            else
            {
             compute_grid_velocity();
             navier_stokes_operation->set_grid_velocity_in_convective_operator_kernel(u_grid_np);
            }

            timeloop_finished = time_integrator->advance_one_timestep(!timeloop_finished);

            timer_help = timer.wall_time();
            if(param.mesh_movement_mappingfefield==true)
            {
              navier_stokes_operation->move_mesh(time_integrator->get_next_time());
            }
            else
            {
              move_mesh(time_integrator->get_next_time());
            }
            move_mesh_time += timer.wall_time() - timer_help;

            timer_help = timer.wall_time();
            navier_stokes_operation->update();
            update_time += timer.wall_time() - timer_help;
          }
        }
    else
      AssertThrow(false, ExcMessage("Not implemented."));
  }
  else if(param.solver_type == SolverType::Steady)
  {
    // solve steady problem
    driver_steady->solve_steady_problem();
  }
  else
  {
    AssertThrow(false, ExcMessage("Not implemented."));
  }

  overall_time += this->timer.wall_time();
}

template<int dim, typename Number>
void
Problem<dim, Number>::analyze_computing_times() const
{
  this->pcout << std::endl
              << "_________________________________________________________________________________"
              << std::endl
              << std::endl;

  this->pcout << "Performance results for incompressible Navier-Stokes solver:" << std::endl;

  // Iterations
  if(param.solver_type == SolverType::Unsteady)
  {
    this->pcout << std::endl << "Average number of iterations:" << std::endl;

    std::vector<std::string> names;
    std::vector<double>      iterations;

    this->time_integrator->get_iterations(names, iterations);

    unsigned int length = 1;
    for(unsigned int i = 0; i < names.size(); ++i)
    {
      length = length > names[i].length() ? length : names[i].length();
    }

    for(unsigned int i = 0; i < iterations.size(); ++i)
    {
      this->pcout << "  " << std::setw(length + 2) << std::left << names[i] << std::fixed
                  << std::setprecision(2) << std::right << std::setw(6) << iterations[i]
                  << std::endl;
    }
  }

  // overall wall time including postprocessing
  Utilities::MPI::MinMaxAvg overall_time_data =
    Utilities::MPI::min_max_avg(overall_time, MPI_COMM_WORLD);
  double const overall_time_avg = overall_time_data.avg;

  // wall times
  this->pcout << std::endl << "Wall times:" << std::endl;

  std::vector<std::string> names;
  std::vector<double>      computing_times;

  if(param.solver_type == SolverType::Unsteady)
  {
    this->time_integrator->get_wall_times(names, computing_times);
  }
  else
  {
    this->driver_steady->get_wall_times(names, computing_times);
  }

  unsigned int length = 1;
  for(unsigned int i = 0; i < names.size(); ++i)
  {
    length = length > names[i].length() ? length : names[i].length();
  }

  double sum_of_substeps = 0.0;
  for(unsigned int i = 0; i < computing_times.size(); ++i)
  {
    Utilities::MPI::MinMaxAvg data =
      Utilities::MPI::min_max_avg(computing_times[i], MPI_COMM_WORLD);
    this->pcout << "  " << std::setw(length + 2) << std::left << names[i] << std::setprecision(2)
                << std::scientific << std::setw(10) << std::right << data.avg << " s  "
                << std::setprecision(2) << std::fixed << std::setw(6) << std::right
                << data.avg / overall_time_avg * 100 << " %" << std::endl;

    sum_of_substeps += data.avg;
  }

  Utilities::MPI::MinMaxAvg setup_time_data =
    Utilities::MPI::min_max_avg(setup_time, MPI_COMM_WORLD);
  double const setup_time_avg = setup_time_data.avg;
  this->pcout << "  " << std::setw(length + 2) << std::left << "Setup" << std::setprecision(2)
              << std::scientific << std::setw(10) << std::right << setup_time_avg << " s  "
              << std::setprecision(2) << std::fixed << std::setw(6) << std::right
              << setup_time_avg / overall_time_avg * 100 << " %" << std::endl;


  Utilities::MPI::MinMaxAvg move_mesh_time_data =
      Utilities::MPI::min_max_avg(move_mesh_time, MPI_COMM_WORLD);
  double const move_mesh = move_mesh_time_data.avg;
  this->pcout << "  " << std::setw(length + 2) << std::left << "Move Mesh" << std::setprecision(2)
              << std::scientific << std::setw(10) << std::right << move_mesh << " s  "
              << std::setprecision(2) << std::fixed << std::setw(6) << std::right
              << move_mesh / overall_time_avg * 100 << " %" << std::endl;

  Utilities::MPI::MinMaxAvg update_time_data =
      Utilities::MPI::min_max_avg(update_time, MPI_COMM_WORLD);
  double const update = update_time_data.avg;
  this->pcout << "  " << std::setw(length + 2) << std::left << "Update" << std::setprecision(2)
              << std::scientific << std::setw(10) << std::right << update << " s  "
              << std::setprecision(2) << std::fixed << std::setw(6) << std::right
              << update / overall_time_avg * 100 << " %" << std::endl;

  double const other = overall_time_avg - sum_of_substeps - setup_time_avg - update - move_mesh;
  this->pcout << "  " << std::setw(length + 2) << std::left << "Other" << std::setprecision(2)
              << std::scientific << std::setw(10) << std::right << other << " s  "
              << std::setprecision(2) << std::fixed << std::setw(6) << std::right
              << other / overall_time_avg * 100 << " %" << std::endl;


  this->pcout << "  " << std::setw(length + 2) << std::left << "Overall" << std::setprecision(2)
              << std::scientific << std::setw(10) << std::right << overall_time_avg << " s  "
              << std::setprecision(2) << std::fixed << std::setw(6) << std::right
              << overall_time_avg / overall_time_avg * 100 << " %" << std::endl;

  // computational costs in CPUh
  unsigned int N_mpi_processes = Utilities::MPI::n_mpi_processes(MPI_COMM_WORLD);

  this->pcout << std::endl
              << "Computational costs (including setup + postprocessing):" << std::endl
              << "  Number of MPI processes = " << N_mpi_processes << std::endl
              << "  Wall time               = " << std::scientific << std::setprecision(2)
              << overall_time_avg << " s" << std::endl
              << "  Computational costs     = " << std::scientific << std::setprecision(2)
              << overall_time_avg * (double)N_mpi_processes / 3600.0 << " CPUh" << std::endl;

  // Throughput in DoFs/s per time step per core
  types::global_dof_index const DoFs = this->navier_stokes_operation->get_number_of_dofs();

  if(param.solver_type == SolverType::Unsteady)
  {
    unsigned int N_time_steps      = this->time_integrator->get_number_of_time_steps();
    double const time_per_timestep = overall_time_avg / (double)N_time_steps;
    this->pcout << std::endl
                << "Throughput per time step (including setup + postprocessing):" << std::endl
                << "  Degrees of freedom      = " << DoFs << std::endl
                << "  Wall time               = " << std::scientific << std::setprecision(2)
                << overall_time_avg << " s" << std::endl
                << "  Time steps              = " << std::left << N_time_steps << std::endl
                << "  Wall time per time step = " << std::scientific << std::setprecision(2)
                << time_per_timestep << " s" << std::endl
                << "  Throughput              = " << std::scientific << std::setprecision(2)
                << DoFs / (time_per_timestep * N_mpi_processes) << " DoFs/s/core" << std::endl;
  }
  else
  {
    this->pcout << std::endl
                << "Throughput (including setup + postprocessing):" << std::endl
                << "  Degrees of freedom      = " << DoFs << std::endl
                << "  Wall time               = " << std::scientific << std::setprecision(2)
                << overall_time_avg << " s" << std::endl
                << "  Throughput              = " << std::scientific << std::setprecision(2)
                << DoFs / (overall_time_avg * N_mpi_processes) << " DoFs/s/core" << std::endl;
  }


  this->pcout << "_________________________________________________________________________________"
              << std::endl
              << std::endl;


}

int
main(int argc, char ** argv)
{
  try
  {
    Utilities::MPI::MPI_InitFinalize mpi(argc, argv, 1);

    // set parameters
    InputParameters param;
    set_input_parameters(param);

    // check parameters in case of restart
    if(param.restarted_simulation)
    {
      AssertThrow(DEGREE_MIN == DEGREE_MAX && REFINE_SPACE_MIN == REFINE_SPACE_MAX,
                  ExcMessage("Spatial refinement not possible in combination with restart!"));

      AssertThrow(REFINE_TIME_MIN == REFINE_TIME_MAX,
                  ExcMessage("Temporal refinement not possible in combination with restart!"));
    }

    // k-refinement
    for(unsigned int degree = DEGREE_MIN; degree <= DEGREE_MAX; ++degree)
    {
      // h-refinement
      for(unsigned int h_refinements = REFINE_SPACE_MIN; h_refinements <= REFINE_SPACE_MAX;
          ++h_refinements)
      {
        // dt-refinement
        for(unsigned int dt_refinements = REFINE_TIME_MIN; dt_refinements <= REFINE_TIME_MAX;
            ++dt_refinements)
        {
          // reset degree
          param.degree_u = degree;

          // reset mesh refinements
          param.h_refinements = h_refinements;

          // reset dt refinements
          param.dt_refinements = dt_refinements;

          // setup problem and run simulation
          typedef double                       Number;
          std::shared_ptr<ProblemBase<Number>> problem;

          if(param.dim == 2)
            problem.reset(new Problem<2, Number>());
          else if(param.dim == 3)
            problem.reset(new Problem<3, Number>());
          else
            AssertThrow(false, ExcMessage("Only dim=2 and dim=3 implemented."));

          problem->setup(param);

          problem->solve();

          problem->analyze_computing_times();
        }
      }
    }
  }
  catch(std::exception & exc)
  {
    std::cerr << std::endl
              << std::endl
              << "----------------------------------------------------" << std::endl;
    std::cerr << "Exception on processing: " << std::endl
              << exc.what() << std::endl
              << "Aborting!" << std::endl
              << "----------------------------------------------------" << std::endl;
    return 1;
  }
  catch(...)
  {
    std::cerr << std::endl
              << std::endl
              << "----------------------------------------------------" << std::endl;
    std::cerr << "Unknown exception!" << std::endl
              << "Aborting!" << std::endl
              << "----------------------------------------------------" << std::endl;
    return 1;
  }
  return 0;
}
