#!/usr/bin/env python

import os
petsc_hash_pkgs=os.path.join(os.getenv('HOME'),'petsc-hash-pkgs')
if not os.path.isdir(petsc_hash_pkgs): os.mkdir(petsc_hash_pkgs)

configure_options = [
  '--package-prefix-hash='+petsc_hash_pkgs,
  'COPTFLAGS=-g -O',
  'FOPTFLAGS=-g -O',
  'CXXOPTFLAGS=-g -O',
  '--with-scalar-type=complex',
  '--with-precision=single',
   ]

if __name__ == '__main__':
  import sys,os
  sys.path.insert(0,os.path.abspath('config'))
  import configure
  configure.petsc_configure(configure_options)