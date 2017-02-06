/*~-------------------------------------------------------------------------~~*
 * Copyright (c) 2016 Los Alamos National Laboratory, LLC
 * All rights reserved
 *~-------------------------------------------------------------------------~~*/
////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief Simple tasks related to solving full hydro solutions.
////////////////////////////////////////////////////////////////////////////////

#pragma once

// hydro includes
#include "types.h"

#include <ale/linalg/qr.h>
#include <ale/utils/algorithm.h>
#include <ale/utils/array_view.h>
#include <ale/utils/filter_iterator.h>

// system includes
 #include <iomanip>
 
namespace apps {
namespace hydro {

////////////////////////////////////////////////////////////////////////////////
//! \brief The main task for setting initial conditions
//!
//! \param [in,out] mesh the mesh object
//! \param [in]     ics  the initial conditions to set
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T, typename F >
int32_t initial_conditions( T & mesh, F && ics ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using real_t = typename T::real_t;
  using vector_t = typename T::vector_t;

  // get the current time
  auto soln_time = mesh.time();

  // get the collection accesor
  auto M = get_accessor( mesh, hydro, cell_mass,       real_t, dense, 0 );
  auto p = get_accessor( mesh, hydro, cell_pressure,   real_t, dense, 0 );
  auto v = get_accessor( mesh, hydro, cell_velocity, vector_t, dense, 0 );

  auto  xc = get_accessor( mesh, mesh, cell_centroid, vector_t, dense, 0 );
  auto vol = get_accessor( mesh, mesh, cell_volume,     real_t, dense, 0 );

  auto cs = mesh.cells();
  auto num_cells = cs.size();

  // This doesn't work with lua input
  // #pragma omp parallel for
  for ( counter_t i=0; i<num_cells; ++i ) {
    auto c = cs[i];
    // now copy the state to flexi
    real_t d;
    std::tie( d, v[c], p[c] ) = std::forward<F>(ics)( xc[c], soln_time );
    // set mass and volume now
    M[c] = d*vol[c];
  }

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
//! \brief The main task for setting initial conditions
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t update_state_from_pressure( T & mesh ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using eqns_t = eqns_t<T::num_dimensions>;

  // get the collection accesor
  auto eos = get_accessor( mesh, hydro, eos, eos_t, global, 0 );
  auto cell_state = cell_state_accessor<T>( mesh );

  auto cs = mesh.cells();
  auto num_cells = cs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_cells; ++i ) {
    auto c = cs[i];
    auto u = cell_state(c);
    eqns_t::update_state_from_pressure( u, *eos );
  }

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
//! \brief The main task for setting initial conditions
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t update_state_from_energy( T & mesh ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using eqns_t = eqns_t<T::num_dimensions>;
  using flux_data_t = flux_data_t<T::num_dimensions>;

  // get the collection accesor
  auto eos = get_accessor( mesh, hydro, eos, eos_t, global, 0 );
  auto cell_state = cell_state_accessor<T>( mesh );

  auto dudt = get_accessor( mesh, hydro, cell_residual, flux_data_t, dense, 0 );

  auto cs = mesh.cells();
  auto num_cells = cs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_cells; ++i ) {
    auto c = cs[i];
    auto u = cell_state(c);
    eqns_t::update_state_from_energy( u, *eos );
  }

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to compute the time step size
//!
//! \param [in,out] mesh  the mesh object
//! \param [in,out] limit_string  a string describing the limiting time step
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t evaluate_time_step( T & mesh, std::string & limit_string ) 
{

  // type aliases
  using counter_t = typename T::counter_t;
  using real_t = typename T::real_t;
  using vector_t = typename T::vector_t;
  using eqns_t = eqns_t<T::num_dimensions>;
  using flux_data_t = flux_data_t<T::num_dimensions>;

  // access what we need
  auto sound_speed = get_accessor( mesh, hydro, cell_sound_speed, real_t, dense, 0 );

  auto dudt = get_accessor( mesh, hydro, cell_residual, flux_data_t, dense, 0 );
  auto cell_volume = get_accessor( mesh, mesh, cell_volume, real_t, dense, 0 );
  auto cell_min_length = get_accessor( mesh, mesh, cell_min_length, real_t, dense, 0 );

  auto time_step = get_accessor( mesh, hydro, time_step, real_t, global, 0 );
  auto cfl = get_accessor( mesh, hydro, cfl, time_constants_t, global, 0 );
 
 
  //----------------------------------------------------------------------------
  // Loop over each cell, computing the minimum time step,
  // which is also the maximum 1/dt
  real_t dt_acc_inv(0);
  real_t dt_vol_inv(0);

  auto cs = mesh.cells();
  auto num_cells = cs.size();

  #pragma omp parallel for reduction( max : dt_acc_inv, dt_vol_inv )
  for ( counter_t i=0; i<num_cells; ++i ) {
    auto c = cs[i];

    // compute the inverse of the time scale
    auto dti =  sound_speed[c] / cell_min_length[c] / cfl->accoustic;
    // check for the maximum value
    dt_acc_inv = std::max( dti, dt_acc_inv );

    // now check the volume change
    auto dVdt = eqns_t::volumetric_rate_of_change( dudt[c] );
    dti = std::abs(dVdt) / cell_volume[c] / cfl->volume;
    // check for the maximum value
    dt_vol_inv = std::max( dti, dt_vol_inv );

  } // cell
  //----------------------------------------------------------------------------


  assert( dt_acc_inv > 0 && dt_vol_inv > 0 && "infinite delta t" );
  
  // get the individual cfls
  auto dt_acc = cfl->accoustic / dt_acc_inv;
  auto dt_vol = cfl->volume / dt_vol_inv;
  auto dt_growth = cfl->growth*(*time_step);

  // find the minimum one
  auto dts = std::array<real_t,3> { dt_acc, dt_vol, dt_growth };
  auto it = std::min_element( dts.begin(), dts.end() );

  switch ( std::distance( dts.begin(), it ) ) {
  case 0:
    limit_string = "accoustic";
    break;
  case 1:
    limit_string = "volume";
    break;
  case 2:
    limit_string = "growth";
    break;
  default:
    limit_string = "unknown";
    raise_runtime_error( "could not determine time step limit" );
    break;    
  };

  // invert dt and check against growth
  *time_step = *it;
      
  return 0;

}

////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to compute nodal quantities
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t estimate_nodal_state( T & mesh ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using vector_t = typename T::vector_t;

  // access what we need
  auto cell_vel = get_accessor( mesh, hydro, cell_velocity, vector_t, dense, 0 );
  auto vertex_vel = get_accessor( mesh, hydro, node_velocity, vector_t, dense, 0 );

  //----------------------------------------------------------------------------
  // Loop over each vertex
  auto vs = mesh.vertices();
  auto num_verts = vs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_verts; ++i ) {
    auto v = vs[i];
    vertex_vel[v] = 0.;
    auto cells = mesh.cells(v);
    for ( auto c : cells ) vertex_vel[v] += cell_vel[c];
    vertex_vel[v] /= cells.size();
  } // vertex
  //----------------------------------------------------------------------------

  return 0;

}

////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to compute nodal quantities
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t evaluate_corner_coef( T & mesh ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using size_t = typename T::size_t;
  using real_t = typename T::real_t;
  using vector_t = typename T::vector_t;
  using matrix_t = matrix_t< T::num_dimensions >; 
  using eqns_t = eqns_t<T::num_dimensions>;

  // get the number of dimensions and create a matrix
  constexpr size_t num_dims = T::num_dimensions;
  
  // access what we need
  auto cell_state = cell_state_accessor<T>( mesh );
  auto vertex_velocity = get_accessor( mesh, hydro, node_velocity, vector_t, dense, 0 );
  auto Mpc = get_accessor( mesh, hydro, corner_matrix, matrix_t, dense, 0 );
  auto npc = get_accessor( mesh, hydro, corner_normal, vector_t, dense, 0 );
  auto wedge_facet_normal = get_accessor( mesh, mesh, wedge_facet_normal, vector_t, dense, 0 );
  auto wedge_facet_area = get_accessor( mesh, mesh, wedge_facet_area, real_t, dense, 0 );

  auto eos = get_accessor( mesh, hydro, eos, eos_t, global, 0 );

  //----------------------------------------------------------------------------
  // build corner matrices
  auto cnrs = mesh.corners();
  auto num_corners = cnrs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_corners; ++i ) {
  
    auto cn = cnrs[i];
      
    // initialize
    Mpc[cn] = 0;
    npc[cn] = 0;

    // a corner connects to a cell and a vertex
    auto cl = mesh.cells(cn).front();
    auto vt = mesh.vertices(cn).front();

    // get the cell state (there is only one)
    auto state = cell_state(cl);
    // the corner quantities ( equal to the cell quantities for 1st order )
    const auto & dc = eqns_t::density( state );
    const auto & uc = eqns_t::velocity( state );
    const auto & ac = eqns_t::sound_speed( state );
    const auto & Gc = eqns_t::impedance_multiplier( state, *eos );
    // the vertex velocity
    const auto & uv = vertex_velocity[vt];

    // for estimating the impedances
    // auto delta_u = uv - uc;
    
    // Estimate the impedance.  This is Burton's form but it also seems to
    // kill the timestep in 3d.
    // auto delta_velocity = math::delta_magnitude( uv, uc );
    // auto zc = dc * ( ac + Gc * delta_velocity );

    // The true impedance
    auto zc = dc * ac;
    
    // iterate over the wedges in pairs
    auto ws = mesh.wedges(cn);
    for ( auto w : ws ) 
    {
      // get the first wedge normal
      const auto & n = wedge_facet_normal[w];
      const auto & l = wedge_facet_area[w];
      // Estimate the impedance.  This is Maires original form, but it seems
      // to kill the timestep in 3d.
      // auto zc = dc * ( ac + Gc * std::abs(dot_product(delta_u, n)) );
      // the final matrix
      // Mpc = zc * ( lpc^- npc^-.npc^-  + lpc^+ npc^+.npc^+ );
      math::outer_product( n, n, Mpc[cn], zc*l );
      // compute the pressure coefficient
      for ( int d=0; d<T::num_dimensions; ++d ) 
        npc[cn][d] += l * n[d];
    } // wedges

  } // corners

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to compute nodal quantities
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T, typename BC >
int32_t evaluate_nodal_state( T & mesh, const BC & boundary_map ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using size_t = typename T::size_t;
  using real_t = typename T::real_t;
  using vector_t = typename T::vector_t;
  using matrix_t = matrix_t< T::num_dimensions >; 
  using eqns_t = eqns_t<T::num_dimensions>;


  // get the number of dimensions and create a matrix
  constexpr size_t dims = T::num_dimensions;
  
  // access what we need
  auto cell_state = cell_state_accessor<T>( mesh );
  auto vertex_velocity = get_accessor( mesh, hydro, node_velocity, vector_t, dense, 0 );
  auto Mpc = get_accessor( mesh, hydro, corner_matrix, matrix_t, dense, 0 );
  auto npc = get_accessor( mesh, hydro, corner_normal, vector_t, dense, 0 );

  auto wedge_facet_normal = get_accessor( mesh, mesh, wedge_facet_normal, vector_t, dense, 0 );
  auto wedge_facet_area = get_accessor( mesh, mesh, wedge_facet_area, real_t, dense, 0 );
  auto wedge_facet_centroid = get_accessor( mesh, mesh, wedge_facet_centroid, vector_t, dense, 0 );

  // get the current time
  auto soln_time = mesh.time();

  //----------------------------------------------------------------------------
  // Loop over each vertex
  //----------------------------------------------------------------------------

  auto vs = mesh.vertices();
  auto num_verts = vs.size();

  #pragma omp parallel for schedule(dynamic)
  for ( counter_t i=0; i<num_verts; ++i ) {

    auto vt = vs[i];

    // create the final matrix the point
    matrix_t Mp(0);
    vector_t rhs(0);

    //--------------------------------------------------------------------------
    // build point matrix
    for ( auto cn : mesh.corners(vt) ) {
      // corner attaches to one cell and one point
      auto cl = mesh.cells(cn).front();
      // get the cell state (there is only one)
      auto state = cell_state(cl);
      // the cell quantities
      const auto & pc = eqns_t::pressure( state );
      const auto & uc = eqns_t::velocity( state );
      // add to the global matrix
      Mp += Mpc[cn];
      // add the pressure and velocity contributions to the system
      for ( int d=0; d<T::num_dimensions; ++d ) rhs[d] += pc * npc[cn][d];
      ax_plus_y( Mpc[cn], uc, rhs );      
    } // corner

    //--------------------------------------------------------------------------
    // now solve the system for the point velocity

    //---------- boundary point
    if ( vt->is_boundary() ) {

      // this is used to keep track of the symmetry normals
      std::map< tag_t, vector_t > symmetry_normals;

      // get the boundary tags
      const auto & point_tags =  vt->tags();

      // first check if this has a prescribed velocity.  If it does, then nothing to do
      auto vel_bc = std::find_if( 
        point_tags.begin(), point_tags.end(), 
        [&boundary_map](const auto & id) { 
          return boundary_map.at(id)->has_prescribed_velocity();
        } );
      if ( vel_bc != point_tags.end() ) {
        vertex_velocity[vt] = boundary_map.at(*vel_bc)->velocity( vt->coordinates(), soln_time );
        continue;
      }

      // otherwise, apply the pressure conditions
      auto bnd_wedges = filter_boundary( mesh.wedges(vt) );
      for ( auto w : bnd_wedges ) 
      {
        auto f = mesh.faces(w).front();
        auto c = mesh.cells(w).front();
        for ( auto tag : f->tags() ) {
          auto b = boundary_map.at( tag );
          // PRESSURE CONDITION
          if ( b->has_prescribed_pressure() ) {
            const auto & n = wedge_facet_normal[w];
            const auto & l = wedge_facet_area[w];
            const auto & x = wedge_facet_centroid[w];
            auto fact = l * b->pressure( x, soln_time );
            for ( int d=0; d<T::num_dimensions; ++d )
              rhs[d] -= fact * n[d];
          }
          // SYMMETRY CONDITION
          else if ( b->has_symmetry() ) {
            const auto & n = wedge_facet_normal[w];
            const auto & l = wedge_facet_area[w];          
            if ( symmetry_normals.count(tag) ) {
              auto & tmp = symmetry_normals[tag];
              for ( int d=0; d<T::num_dimensions; ++d )
                tmp[d] += l * n[d];
            }
            else {
              auto & tmp = symmetry_normals[tag];
              for ( int d=0; d<T::num_dimensions; ++d )              
                tmp[d] = l * n[d];
            }
          } // END CONDITIONS
        } // for each tag
      } // for each wedge


      // now construct the system to solve
      //
      // no additional symmetry constraints
      if ( symmetry_normals.empty() ) {
        vertex_velocity[vt]  = math::solve( Mp, rhs );
      }
      // add symmetry constraints and grow the system
      else {
        // how many dimensions
        constexpr auto num_dims = T::num_dimensions;
        // how many extra constraints ( there are two wedges per face )
        auto num_symmetry = symmetry_normals.size();
        // the matrix size
        auto num_rows = num_dims+num_symmetry;
        // create storage for the new system in a 1d array
        std::vector< real_t > A( num_rows * num_rows, 0 ); // zerod
        std::vector< real_t > b( num_rows ); // zerod
        // create the views
        auto A_view = utils::make_array_view( A, num_rows, num_rows );
        auto M_view = utils::make_array_view( Mp.data(), num_dims, num_dims );
        auto b_view = utils::make_array_view( b );
        // insert the old system into the new one
        for ( int d=0; d<num_dims; ++d )
          b_view[d] = rhs[d];
        for ( int i=0; i<num_dims; i++ ) 
          for ( int j=0; j<num_dims; j++ ) 
            A_view(i,j) = Mp(i,j);
        // insert each constraint
        for ( int i=0; i<num_dims; i++ ) {
          int j = num_dims;
          for ( const auto & n : symmetry_normals )
            A_view( i, j++ ) = n.second[i];          
        }
        int i = num_dims;
        for ( const auto & n : symmetry_normals ) {
          for ( int j=0; j<num_dims; j++ ) 
            A_view( i, j ) = n.second[j];          
          i++;
        }               
        // solve the system
        ale::linalg::qr( A_view, b_view );
        // copy the results back
        for ( int d=0; d<num_dims; ++d )
          vertex_velocity[vt][d] = b_view[d];

      } // end has symmetry

    } // boundary point

    //---------- internal point
    // make sure sum(lpc) = 0
    // assert( abs(np) < eps && "error in norms" );
    // now solve for point velocity
    else {
      
      vertex_velocity[vt] = math::solve( Mp, rhs );

    } // internal point


  } // vertex
  //----------------------------------------------------------------------------

  return 0;
   
}

////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to evaluate residuals
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t evaluate_forces( T & mesh ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using real_t = typename T::real_t;
  using vector_t = typename T::vector_t;
  using matrix_t = matrix_t< T::num_dimensions >; 
  using flux_data_t = flux_data_t<T::num_dimensions>;
  using eqns_t = eqns_t<T::num_dimensions>;

  // access what we need
  auto cell_state = cell_state_accessor<T>( mesh );

  auto dudt = get_accessor( mesh, hydro, cell_residual, flux_data_t, dense, 0 );
  auto uv = get_accessor( mesh, hydro, node_velocity, vector_t, dense, 0 );

  auto Mpc = get_accessor( mesh, hydro, corner_matrix, matrix_t, dense, 0 );
  auto npc = get_accessor( mesh, hydro, corner_normal, vector_t, dense, 0 );

  //----------------------------------------------------------------------------
  // TASK: loop over each cell and compute the residual
  
  // get the cells
  auto cs = mesh.cells();
  auto num_cells = cs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_cells; i++ ) {
    
    auto cl = cs[i];

    // get the cell state (there is only one)
    auto state = cell_state(cl);
    // the cell quantities
    const auto & pc = eqns_t::pressure( state );
    const auto & uc = eqns_t::velocity( state );

    // local cell residual
    dudt[cl] = 0.0;

    //--------------------------------------------------------------------------
    // compute subcell forces
    for ( auto cn : mesh.corners(cl) ) {

      // corner attaches to one point and zone
      auto pt = mesh.vertices(cn).front();

      // compute the subcell force
      // lpc*Pc*npc - Mpc*(uv-uc)
      vector_t force, delta_u;
      for ( int d=0; d<T::num_dimensions; ++ d ) {
        force[d] =  pc * npc[cn][d];
        delta_u[d] = uc[d] - uv[pt][d];
      }
      ax_plus_y( Mpc[cn], delta_u, force );

      // add contribution
      eqns_t::compute_update( uv[pt], force, npc[cn], dudt[cl] );
      
    }// corners    
    //--------------------------------------------------------------------------
    
  } // cell
  //----------------------------------------------------------------------------

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to update the solution
//!
//! \param [in,out] mesh the mesh object
//!   \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t apply_update( T & mesh, real_t coef, bool first_time ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using real_t = typename T::real_t;
  using vector_t = typename T::vector_t;
  using flux_data_t = flux_data_t<T::num_dimensions>;
  using eqns_t = eqns_t<T::num_dimensions>;


  // access what we need
  auto dudt = get_accessor( mesh, hydro, cell_residual, flux_data_t, dense, 0 );

  auto cell_state = cell_state_accessor<T>( mesh );

  // read only access
  const auto delta_t = get_accessor( mesh, hydro, time_step, real_t, global, 0 );

  // the time step factor
  auto fact = coef * (*delta_t);

  real_t mass(0);
  vector_t mom(0);
  real_t ener(0);
 
  //----------------------------------------------------------------------------
  // Loop over each cell, scattering the fluxes to the cell

  auto cs = mesh.cells();
  auto num_cells = cs.size();
  
  #pragma omp declare reduction( + : vector_t : omp_out += omp_in ) \
    initializer (omp_priv(0))

  #ifndef NDEBUG
  #  pragma omp parallel for reduction( + : mass, mom, ener )
  #else
  #  pragma omp parallel for
  #endif
  
  for ( counter_t i=0; i<num_cells; i++ ) {

    // get the cell state
    auto cell = cs[i];
    auto u = cell_state( cell );
    
    // apply the update
    eqns_t::update_state_from_flux( u, dudt[cell], fact );
    eqns_t::update_volume( u, cell->volume() );

#ifndef NDEBUG
    // post update sums
    auto vel = eqns_t::velocity(u);
    auto et = eqns_t::total_energy(u);
    auto m  = eqns_t::mass(u);
    mass += m;
    for ( int d=0; d<T::num_dimensions; ++d ) mom[d] += m * vel[d];
    ener += m * et;
#endif
    
  } // for
  //----------------------------------------------------------------------------

#ifndef NDEBUG
  std::stringstream mom_ss;
  mom_ss.setf( std::ios::scientific );

  for ( const auto & mx : mom )
    mom_ss << std::setprecision(2) << std::setw(9) << mx << " ";
  auto mom_str = mom_ss.str();
  mom_str.pop_back();

  auto ss = cout.precision();
  cout.setf( std::ios::scientific );

  if ( first_time ) {
    cout << std::string(60, '-') << endl;
    cout << "| " << std::setw(10) << "Mass:"
         << " | " << std::setw(29) << "Momentum:"
         << " | " << std::setw(11) << "Energy:"
         << " |" << std::endl;
  }
  cout << "| " << std::setprecision(3) << std::setw(10) << mass
       << " | " << std::setw(29) << mom_str
       << " | " << std::setprecision(4)  << std::setw(11) << ener
       << " |" << std::endl;

  cout.unsetf( std::ios::scientific );
  cout.precision(ss);
#endif

  return 0;

}

////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to move the mesh
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t move_mesh( T & mesh, real_t coef ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using real_t = typename T::real_t;
  using vector_t = typename T::vector_t;

  // access what we need
  auto vel = get_accessor( mesh, hydro, node_velocity, vector_t, dense, 0 );

  // read only access
  const auto delta_t = get_accessor( mesh, hydro, time_step, real_t, global, 0 );

  // the time step factor
  auto fact = coef * (*delta_t);
 
  // Loop over each cell, scattering the fluxes to the cell
  auto vs = mesh.vertices();
  auto num_verts = vs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_verts; i++ ) {
    auto vt = vs[i];
    for ( int d=0; d<T::num_dimensions; ++d )
      vt->coordinates()[d] += fact * vel[vt][d];
  }

  // now update the geometry
  mesh.update_geometry();

  return 0;

}


////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to save the coordinates
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t save_coordinates( T & mesh ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using vector_t = typename T::vector_t;

  // access what we need
  auto coord0 = get_accessor( mesh, hydro, node_coordinates, vector_t, dense, 0 );

  // Loop over vertices
  auto vs = mesh.vertices();
  auto num_verts = vs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_verts; i++ ) {
    auto vt = vs[i];
    coord0[vt] = vt->coordinates();
  }

  return 0;

}


////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to restore the coordinates
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t restore_coordinates( T & mesh ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using vector_t = typename T::vector_t;

  // access what we need
  auto coord0 = get_accessor( mesh, hydro, node_coordinates, vector_t, dense, 0 );

  // Loop over vertices
  auto vs = mesh.vertices();
  auto num_verts = vs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_verts; i++ ) {
    auto vt = vs[i];
    vt->coordinates() = coord0[vt];
  }

  return 0;

}


////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to save the coordinates
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t save_solution( T & mesh ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using real_t = typename T::real_t;
  using vector_t = typename T::vector_t;

  // access what we need
  auto vel  = get_accessor( mesh, hydro, cell_velocity, vector_t, dense, 0 );
  auto vel0 = get_accessor( mesh, hydro, cell_velocity, vector_t, dense, 1 );

  auto ener  = get_accessor( mesh, hydro, cell_internal_energy, real_t, dense, 0 );
  auto ener0 = get_accessor( mesh, hydro, cell_internal_energy, real_t, dense, 1 );

  // Loop over cells
  auto cs = mesh.cells();
  auto num_cells = cs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_cells; i++ ) {
    auto c = cs[i];
    vel0[c] = vel[c];
    ener0[c] = ener[c];
  }

  return 0;

}


////////////////////////////////////////////////////////////////////////////////
//! \brief The main task to restore the coordinates
//!
//! \param [in,out] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t restore_solution( T & mesh ) {

  // type aliases
  using counter_t = typename T::counter_t;
  using real_t = typename T::real_t;
  using vector_t = typename T::vector_t;

  // access what we need
  auto vel  = get_accessor( mesh, hydro, cell_velocity, vector_t, dense, 0 );
  auto vel0 = get_accessor( mesh, hydro, cell_velocity, vector_t, dense, 1 );

  auto ener  = get_accessor( mesh, hydro, cell_internal_energy, real_t, dense, 0 );
  auto ener0 = get_accessor( mesh, hydro, cell_internal_energy, real_t, dense, 1 );

  // Loop over cells
  auto cs = mesh.cells();
  auto num_cells = cs.size();

  #pragma omp parallel for
  for ( counter_t i=0; i<num_cells; i++ ) {
    auto c = cs[i];
    vel[c] = vel0[c];
    ener[c] = ener0[c];
  }

  return 0;

}

////////////////////////////////////////////////////////////////////////////////
//! \brief Output the solution
//!
//! \param [in] mesh the mesh object
//! \return 0 for success
////////////////////////////////////////////////////////////////////////////////
template< typename T >
int32_t output( T & mesh, 
                const std::string & prefix, 
                const std::string & postfix, 
                size_t output_freq ) 
{

  if ( output_freq < 1 ) return 0;

  auto cnt = mesh.time_step_counter();
  if ( cnt % output_freq != 0 ) return 0;

  std::stringstream ss;
  ss << prefix;
  ss << std::setw( 7 ) << std::setfill( '0' ) << cnt++;
  ss << "."+postfix;
  
  cout << endl;
  mesh::write_mesh( ss.str(), mesh );
  cout << endl;
  
  return 0;
}


} // namespace hydro
} // namespace apps
