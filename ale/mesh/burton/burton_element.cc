/*~--------------------------------------------------------------------------~*
 *  @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
 * /@@/////  /@@          @@////@@ @@////// /@@
 * /@@       /@@  @@@@@  @@    // /@@       /@@
 * /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
 * /@@////   /@@/@@@@@@@/@@       ////////@@/@@
 * /@@       /@@/@@//// //@@    @@       /@@/@@
 * /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
 * //       ///  //////   //////  ////////  //
 *
 * Copyright (c) 2016 Los Alamos National Laboratory, LLC
 * All rights reserved
 *~--------------------------------------------------------------------------~*/

//! user includes
#include "ale/math/math.h"
#include "ale/mesh/burton/burton_mesh_topology.h"
#include "ale/mesh/burton/burton_element.h"


namespace ale {
namespace mesh {

// some type aliases
using burton_2d_edge_t = burton_element_t<2,1>;
using burton_2d_cell_t = burton_element_t<2,2>;
using burton_3d_edge_t = burton_element_t<3,1>;
using burton_3d_face_t = burton_element_t<3,2>;
using burton_3d_cell_t = burton_element_t<3,3>;

// some type aliases


////////////////////////////////////////////////////////////////////////////////
// 2d - edge
////////////////////////////////////////////////////////////////////////////////

// the list of actual coordinates
burton_2d_edge_t::point_list_t burton_2d_edge_t::coordinates() const
{
  auto mesh = static_cast<const burton_2d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  point_list_t coords;
  coords.front() = vs.front()->coordinates();
  coords.back () = vs.back ()->coordinates();
  return coords;
}

  
// the edge midpoint
burton_2d_edge_t::point_t burton_2d_edge_t::midpoint() const
{
  auto mesh = static_cast<const burton_2d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);    
  return {0.5*(vs[0]->coordinates() + vs[1]->coordinates())};
}

// the edge centroid
burton_2d_edge_t::point_t burton_2d_edge_t::centroid() const
{
  return midpoint();
}

// the edge length
burton_2d_edge_t::real_t  burton_2d_edge_t::length() const
{
  using math::sqr;
  auto mesh = static_cast<const burton_2d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  auto & a = vs[0]->coordinates();
  auto & b = vs[1]->coordinates();
  return std::sqrt( sqr(a[0]-b[0]) + sqr(a[1]-b[1]) );
}

// in 2d, this doubles as a face, so the area is the same as the length
burton_2d_edge_t::real_t burton_2d_edge_t::area() const
{
  return length();
}

// the edge normal
burton_2d_edge_t::vector_t burton_2d_edge_t::normal() const
{
  auto mesh = static_cast<const burton_2d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  using math::normal;
  return normal( vs[1]->coordinates(), vs[0]->coordinates() );
}


//! is this a boundary
bool burton_2d_edge_t::is_boundary() const
{
  using flecsi::mesh_entity_base_t;
  auto mesh = static_cast<const burton_2d_mesh_topology_t *>(mesh_); 
  auto flag =
    data_t::instance().template dense_accessor<bitfield_t, flecsi_internal>(
      "edge_flags", mesh->runtime_id() );
  return flag[mesh_entity_base_t<num_domains>::template id<0>()].anybitset();
}

////////////////////////////////////////////////////////////////////////////////
// 3d - edge
////////////////////////////////////////////////////////////////////////////////

// the list of actual coordinates
burton_3d_edge_t::point_list_t burton_3d_edge_t::coordinates() const
{
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  point_list_t coords;
  coords.front() = vs.front()->coordinates();
  coords.back () = vs.back ()->coordinates();
  return coords;
}

  
// the edge midpoint
burton_3d_edge_t::point_t burton_3d_edge_t::midpoint() const
{
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);    
  return {0.5*(vs[0]->coordinates() + vs[1]->coordinates())};
}

// the edge length
burton_3d_edge_t::real_t  burton_3d_edge_t::length() const
{
  using math::sqr;
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  auto & a = vs[0]->coordinates();
  auto & b = vs[1]->coordinates();
  return std::sqrt( sqr(a[0]-b[0]) + sqr(a[1]-b[1]) );
}


//! is this a boundary
bool burton_3d_edge_t::is_boundary() const
{
  using flecsi::mesh_entity_base_t;
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto flag =
    data_t::instance().template dense_accessor<bitfield_t, flecsi_internal>(
      "edge_flags", mesh->runtime_id() );
  return flag[mesh_entity_base_t<num_domains>::template id<0>()].anybitset();
}

////////////////////////////////////////////////////////////////////////////////
// 2d - Planar Cell
////////////////////////////////////////////////////////////////////////////////

// the list of actual coordinates
burton_2d_cell_t::point_list_t burton_2d_cell_t::coordinates() const
{
  auto mesh = static_cast<const burton_2d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  point_list_t coords;
  coords.reserve( vs.size() );
  for ( auto v : vs ) coords.emplace_back( v->coordinates() );
  return coords;
}

// the minimum length in the element
burton_2d_cell_t::real_t burton_2d_cell_t::min_length() const
{
  using math::abs;
  // get the vertices
  auto mesh = static_cast<const burton_2d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  // get one of the edges as a reference
  auto es = mesh->template entities<1,0>(this);
  auto min_length = es.front()->length();
  // check each vertex combination
  for ( auto vi : vs ) {
    auto pi = vi->coordinates();
    for ( auto vj : vs ) {
      if ( vi == vj ) continue;
      auto pj = vj->coordinates();
      auto delta = pi - pj;
      min_length = std::min( abs(delta), min_length );
    }
  }
  // return the result
  return min_length;
}

// get the region id
burton_2d_cell_t::size_t & burton_2d_cell_t::region()
{
  using flecsi::mesh_entity_base_t;
  auto mesh = static_cast<const burton_2d_mesh_topology_t *>(mesh_); 
  auto cell_regions =
    data_t::instance().template dense_accessor<size_t, flecsi_internal>(
      "cell_region", mesh->runtime_id() );
  return cell_regions[mesh_entity_base_t<num_domains>::template id<0>()];
}

// get the region id
burton_2d_cell_t::size_t burton_2d_cell_t::region() const
{
  using flecsi::mesh_entity_base_t;
  auto mesh = static_cast<const burton_2d_mesh_topology_t *>(mesh_); 
  auto cell_regions =
    data_t::instance().template dense_accessor<size_t, flecsi_internal>(
      "cell_region", mesh->runtime_id() );
  return cell_regions[mesh_entity_base_t<num_domains>::template id<0>()];
}



////////////////////////////////////////////////////////////////////////////////
// 3d - Face
////////////////////////////////////////////////////////////////////////////////

// the list of actual coordinates
burton_3d_face_t::point_list_t burton_3d_face_t::coordinates() const
{
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  point_list_t coords;
  coords.reserve( vs.size() );
  for ( auto v : vs ) coords.emplace_back( v->coordinates() );
  return coords;
}

// the minimum length in the element
burton_3d_face_t::real_t burton_3d_face_t::min_length() const
{
  using math::abs;
  // get the vertices
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  // get one of the edges as a reference
  auto es = mesh->template entities<1,0>(this);
  auto min_length = es.front()->length();
  // check each vertex combination
  for ( auto vi : vs ) {
    auto pi = vi->coordinates();
    for ( auto vj : vs ) {
      if ( vi == vj ) continue;
      auto pj = vj->coordinates();
      auto delta = pi - pj;
      min_length = std::min( abs(delta), min_length );
    }
  }
  // return the result
  return min_length;
}

////////////////////////////////////////////////////////////////////////////////
// 3d - Cell
////////////////////////////////////////////////////////////////////////////////

// the list of actual coordinates
burton_3d_cell_t::point_list_t burton_3d_cell_t::coordinates() const
{
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  point_list_t coords;
  coords.reserve( vs.size() );
  for ( auto v : vs ) coords.emplace_back( v->coordinates() );
  return coords;
}

// the minimum length in the element
burton_3d_cell_t::real_t burton_3d_cell_t::min_length() const
{
  using math::abs;
  // get the vertices
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto vs = mesh->template entities<0,0>(this);
  // get one of the edges as a reference
  auto es = mesh->template entities<1,0>(this);
  auto min_length = es.front()->length();
  // check each vertex combination
  for ( auto vi : vs ) {
    auto pi = vi->coordinates();
    for ( auto vj : vs ) {
      if ( vi == vj ) continue;
      auto pj = vj->coordinates();
      auto delta = pi - pj;
      min_length = std::min( abs(delta), min_length );
    }
  }
  // return the result
  return min_length;
}


// get the region id
burton_3d_cell_t::size_t & burton_3d_cell_t::region()
{
  using flecsi::mesh_entity_base_t;
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto cell_regions =
    data_t::instance().template dense_accessor<size_t, flecsi_internal>(
      "cell_region", mesh->runtime_id() );
  return cell_regions[mesh_entity_base_t<num_domains>::template id<0>()];
}

// get the region id
burton_3d_cell_t::size_t burton_3d_cell_t::region() const
{
  using flecsi::mesh_entity_base_t;
  auto mesh = static_cast<const burton_3d_mesh_topology_t *>(mesh_); 
  auto cell_regions =
    data_t::instance().template dense_accessor<size_t, flecsi_internal>(
      "cell_region", mesh->runtime_id() );
  return cell_regions[mesh_entity_base_t<num_domains>::template id<0>()];
}

} // namespace
} // namespace
