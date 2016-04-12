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
 * \file burton_io.cc
 * 
 * \brief Tests io of the burton mesh.
 *
 ******************************************************************************/

//! user includes
#include "burton_io_test.h"


// Below tests need exodus to read the file
#ifdef HAVE_EXODUS 

////////////////////////////////////////////////////////////////////////////////
//! \brief test reading  an exodus file
////////////////////////////////////////////////////////////////////////////////
TEST_F(BurtonIO, read_write_exo) {
  mesh_t m;
  // read mesh written by above test
  string name("mixed.exo");
  ASSERT_FALSE(read_mesh(name, m));
  // create state data on b
  create_data(m);
  // write m to a different file
  name = output_prefix()+".exo";
  ASSERT_FALSE(write_mesh(name, m));
} // TEST_F

////////////////////////////////////////////////////////////////////////////////
//! \brief test reading an exodus file
////////////////////////////////////////////////////////////////////////////////
TEST_F(BurtonIO, read_write_g) {
  mesh_t m;
  // read mesh written by above test
  string name("mixed.g");
  ASSERT_FALSE(read_mesh(name, m));
  // create state data on b
  create_data(m);
  // write m to a different file
  name = output_prefix()+".g";
  ASSERT_FALSE(write_mesh(name, m));
} // TEST_F

////////////////////////////////////////////////////////////////////////////////
//! \brief test reading an exodus file
////////////////////////////////////////////////////////////////////////////////
TEST_F(BurtonIO, read_write_voro) {
  mesh_t m;
  // read mesh written by above test
  string name("voro.g");
  ASSERT_FALSE(read_mesh(name, m));
  // write mesh written by above test
  name = output_prefix()+".g";
  ASSERT_FALSE(write_mesh(name, m));
} // TEST_F


////////////////////////////////////////////////////////////////////////////////
//! \brief test writing an exodus file
////////////////////////////////////////////////////////////////////////////////
TEST_F(BurtonIO, write_dat) {
  mesh_t m;
  // read mesh written by above test
  string name("mixed.g");
  ASSERT_FALSE(read_mesh(name, m));
  // create state data on b
  create_data(m);
  // write the mesh
  name = output_prefix()+".dat";
  ASSERT_FALSE(write_mesh(name, m));
} // TEST_F



#ifdef HAVE_TECIO

////////////////////////////////////////////////////////////////////////////////
//! \brief test writing an exodus file
////////////////////////////////////////////////////////////////////////////////
TEST_F(BurtonIO, write_plt) {
  mesh_t m;
  // read mesh written by above test
  string name("mixed.g");
  ASSERT_FALSE(read_mesh(name, m));
  // create state data on b
  create_data(m);
  // write the mesh
  name = output_prefix()+".plt";
  ASSERT_FALSE(write_mesh(name, m));
} // TEST_F

#endif // HAVE_TECIO

////////////////////////////////////////////////////////////////////////////////
//! \brief test writing an exodus file
////////////////////////////////////////////////////////////////////////////////
TEST_F(BurtonIO, write_vtk) {
  mesh_t m;
  // read mesh written by above test
  string name("mixed.g");
  ASSERT_FALSE(read_mesh(name, m));
  // create state data on b
  create_data(m);
  // write the mesh in default format
  name = output_prefix()+"-default.vtk";
  ASSERT_FALSE(write_mesh(name, m));
  // write the mesh in ascii
  name = output_prefix()+"-ascii.vtk";
  ASSERT_FALSE(write_mesh(name, m, false));
  // write it again in binary
  name = output_prefix()+"binary.vtk";
  ASSERT_FALSE(write_mesh(name, m, true));
} // TEST_F


#endif // HAVE_EXODUS


// Below tests have their own readers

#ifdef HAVE_VTK

////////////////////////////////////////////////////////////////////////////////
//! \brief test writing an vtk file
////////////////////////////////////////////////////////////////////////////////
TEST_F(BurtonIO, read_write_vtk) {
  mesh_t m;
  // read mesh written by above test
  string name("mixed.vtk");
  ASSERT_FALSE(read_mesh(name, m));
  // create state data on b
  create_data(m);
  // write the mesh in default format
  name = output_prefix()+".vtk";
  ASSERT_FALSE(write_mesh(name, m));
} // TEST_F

////////////////////////////////////////////////////////////////////////////////
//! \brief test writing an exodus file
////////////////////////////////////////////////////////////////////////////////
TEST_F(BurtonIO, read_write_vtu) {
  mesh_t m;
  // read mesh written by above test
  string name("mixed.vtu");
  ASSERT_FALSE(read_mesh(name, m));
  // create state data on b
  create_data(m);
  // write the mesh in default format
  name = output_prefix()+".vtu";
  ASSERT_FALSE(write_mesh(name, m));
} // TEST_F


////////////////////////////////////////////////////////////////////////////////
//! \brief test writing an exodus file
////////////////////////////////////////////////////////////////////////////////
TEST_F(BurtonIO, read_write_vtm) {
  mesh_t m;
  // read mesh written by above test
  string name("mixed.vtm");
  ASSERT_FALSE(read_mesh(name, m));
  // create state data on b
  create_data(m);
  // write the mesh in default format
  name = output_prefix()+".vtm";
  ASSERT_FALSE(write_mesh(name, m));
} // TEST_F

#endif // HAVE_VTK

/*~------------------------------------------------------------------------~--*
 * Formatting options
 * vim: set tabstop=2 shiftwidth=2 expandtab :
 *~------------------------------------------------------------------------~--*/