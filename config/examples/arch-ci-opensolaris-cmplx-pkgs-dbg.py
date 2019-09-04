#!/usr/bin/env python

import os
petsc_hash_pkgs=os.path.join(os.getenv('HOME'),'petsc-hash-pkgs')
if not os.path.isdir(petsc_hash_pkgs): os.mkdir(petsc_hash_pkgs)

configure_options = [
  '--package-prefix-hash='+petsc_hash_pkgs,
  'COPTFLAGS=-g -O',
  'FOPTFLAGS=-g -O',
  'CXXOPTFLAGS=-g -O',
  'CC=cc',
  'CXX=CC',
  'FC=f90',
  '--with-scalar-type=complex',
  'FFLAGS=-ftrap=%none',
  '--with-c2html=0',
  '--download-mpich',
  '--download-metis',
  '--download-parmetis',
  '--download-triangle',
  '--download-superlu',
  '--download-fblaslapack',
  '--download-scalapack',
  '--download-mumps',
  '--download-hdf5',
  '-download-hdf5-fc=0', # as the compiler is not F2003 compilant
  '--download-suitesparse',
  '--download-chaco',
  ]

if __name__ == '__main__':
  import sys,os
  sys.path.insert(0,os.path.abspath('config'))
  import configure
  configure.petsc_configure(configure_options)
