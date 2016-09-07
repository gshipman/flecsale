/*~-------------------------------------------------------------------------~~*
 *     _   ______________     ___    __    ______
 *    / | / / ____/ ____/    /   |  / /   / ____/
 *   /  |/ / / __/ /  ______/ /| | / /   / __/   
 *  / /|  / /_/ / /__/_____/ ___ |/ /___/ /___   
 * /_/ |_/\____/\____/    /_/  |_/_____/_____/   
 * 
 * Copyright (c) 2016 Los Alamos National Laboratory, LLC
 * All rights reserved
 *~-------------------------------------------------------------------------~~*/
/*!
 *
 * \file types.h
 * 
 * \brief Define the main types for the hydro solver.
 *
 ******************************************************************************/

#pragma once

// user includes
#include <ale/common/types.h>
#include <ale/eqns/lagrange_eqns.h>
#include <ale/eqns/flux.h>
#include <ale/eos/ideal_gas.h>
#include <ale/math/general.h>
#include <ale/math/matrix.h>

#include <ale/mesh/burton/burton.h>


namespace apps {
namespace hydro {

// some namespace aliases
namespace common= ale::common;
namespace mesh  = ale::mesh;
namespace math  = ale::math;
namespace utils = ale::utils;
namespace geom  = ale::geom;
namespace eos   = ale::eos;
namespace eqns  = ale::eqns;

// mesh and some underlying data types
using mesh_2d_t = mesh::burton_mesh_2d_t;
using mesh_3d_t = mesh::burton_mesh_3d_t;

using size_t = common::size_t;
using real_t = common::real_t;

template< std::size_t N >
using matrix_t = math::matrix< real_t, N, N >; 

using eos_t = eos::ideal_gas_t<real_t>;

template<std::size_t N>
using eqns_t = eqns::lagrange_eqns_t<real_t, N>;

template<std::size_t N>
using flux_data_t = typename eqns_t<N>::flux_data_t;

// explicitly use some other stuff
using std::cout;
using std::cerr;
using std::endl;



template< std::size_t N >
class boundary_condition_t
{
public:

  using real_type   = real_t; 
  using vector_type = math::vector< real_type, N >; 

  virtual bool has_prescribed_velocity() const 
  { return false; };

  virtual bool has_prescribed_pressure() const 
  { return false; };

  virtual bool has_symmetry() const 
  { return true; };

  virtual vector_type velocity( const vector_type & x, const real_type & t ) const 
  { return 0; };

  virtual real_type pressure( const vector_type & x, const real_type & t ) const 
  { return 0; };

};


// a type for storing boundary conditions
using tag_t = mesh_2d_t::tag_t;

template< std::size_t N >
using boundary_map_t = std::map< tag_t, boundary_condition_t<N> * >;


////////////////////////////////////////////////////////////////////////////////
//! \brief A functor for accessing state in the mesh
//! \tparam M  the mesh type
////////////////////////////////////////////////////////////////////////////////
template < typename M >
class cell_state_accessor 
{
public:
  
  //! some type aliases
  using real_t = typename M::real_t;
  using vector_t = typename M::vector_t;

  //! \brief determine the type of accessor
  //! \tparam T the data type we are accessing
  template< typename T >
  using accessor_t = 
    decltype( access_state( std::declval<M>(), "var_name", T ) );

  //! \brief main constructor
  //! \param [in]  mesh  the mesh object
  constexpr cell_state_accessor( M & mesh ) :
    m( access_state( mesh, "cell_mass",              real_t ) ),
    V( access_state( mesh, "cell_volume",            real_t ) ),
    p( access_state( mesh, "cell_pressure",          real_t ) ),
    v( access_state( mesh, "cell_velocity",        vector_t ) ),
    d( access_state( mesh, "cell_density",           real_t ) ),
    e( access_state( mesh, "cell_internal_energy",   real_t ) ),
    t( access_state( mesh, "cell_temperature",       real_t ) ),
    a( access_state( mesh, "cell_sound_speed",       real_t ) )
  {}

  //! \brief main accessor
  //! \remark const version
  //!
  //! \tparam T the element type
  //! \param [in] i  the element we are accessing
  //! \return a tuple of references to all the state data
  template< typename T >
  constexpr auto operator()( T && i ) const 
  {
    using std::forward;
    return std::forward_as_tuple( V[ forward<T>(i) ], 
                                  m[ forward<T>(i) ], 
                                  v[ forward<T>(i) ], 
                                  p[ forward<T>(i) ], 
                                  d[ forward<T>(i) ], 
                                  e[ forward<T>(i) ], 
                                  t[ forward<T>(i) ], 
                                  a[ forward<T>(i) ] );
  }

  //! \brief main accessor
  //! \remark non-const version
  //!
  //! \tparam T the element type
  //! \param [in] i  the element we are accessing
  //! \return a tuple of references to all the state data
  template< typename T >
  auto operator()( T && i ) 
  {
    using std::forward;
    return std::forward_as_tuple( V[ forward<T>(i) ], 
                                  m[ forward<T>(i) ], 
                                  v[ forward<T>(i) ], 
                                  p[ forward<T>(i) ], 
                                  d[ forward<T>(i) ], 
                                  e[ forward<T>(i) ], 
                                  t[ forward<T>(i) ], 
                                  a[ forward<T>(i) ] );
  }

private:

  // private data
  accessor_t<real_t>   m;
  accessor_t<real_t>   V;
  accessor_t<real_t>   p;
  accessor_t<vector_t> v;
  accessor_t<real_t>   d;
  accessor_t<real_t>   e;
  accessor_t<real_t>   t;
  accessor_t<real_t>   a;
       
};


////////////////////////////////////////////////////////////////////////////////
//! \brief A class to make setting the cfl easy
////////////////////////////////////////////////////////////////////////////////
struct time_constants_t {

  real_t accoustic = 1.0;
  real_t volume = 1.0;
  real_t growth = 0.0; 

};

} // namespace hydro
} // namespace apps
