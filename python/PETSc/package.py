import config.base
import os
import re
import sys

class Package(config.base.Configure):
  def __init__(self, framework):
    config.base.Configure.__init__(self, framework)
    self.headerPrefix = ''
    self.substPrefix  = ''
    self.compilers    = self.framework.require('config.compilers',self)
    self.libraries    = self.framework.require('config.libraries',self)
    self.blasLapack   = self.framework.require('PETSc.packages.BlasLapack',self)
    self.clanguage    = self.framework.require('PETSc.utilities.clanguage',self)    
    self.functions    = self.framework.require('config.functions',         self)
    self.found        = 0
    self.lib          = []
    self.include      = []
    self.name         = os.path.splitext(os.path.basename(sys.modules.get(self.__module__).__file__))[0]
    self.PACKAGE      = self.name.upper()
    self.package      = self.name.lower()
    self.complex      = 0
    
  def __str__(self):
    output=''
    if self.found:
      output  = self.name+':\n'
      if self.include: output += '  Includes: '+str(self.include)+'\n'
      if self.lib:     output += '  Library: '+str(self.lib)+'\n'
    return output
  
  def setupHelp(self,help):
    import nargs
    help.addArgument(self.PACKAGE,'-with-'+self.package+'=<bool>',nargs.ArgBool(None,0,'Indicate if you wish to test for '+self.name))
    help.addArgument(self.PACKAGE,'-with-'+self.package+'-dir=<dir>',nargs.ArgDir(None,None,'Indicate the root directory of the '+self.name+' installation'))
    if hasattr(self,'downLoad'):
      help.addArgument(self.PACKAGE, '-download-'+self.package+'=<no,yes,ifneeded>',  nargs.ArgFuzzyBool(None, 0, 'Download and install '+self.name))
    return

  def checkInclude(self,incl,hfile,otherIncludes = None):
    if hasattr(self,'mpi'):    incl.extend(self.mpi.include)
    if otherIncludes:          incl.extend(otherIncludes)
    oldFlags = self.framework.argDB['CPPFLAGS']
    self.framework.argDB['CPPFLAGS'] += ' '.join([self.libraries.getIncludeArgument(inc) for inc in incl])
    found = self.checkPreprocess('#include <' +hfile+ '>\n')
    self.framework.argDB['CPPFLAGS'] = oldFlags
    if found:
      self.framework.log.write('Found header file ' +hfile+ ' in '+str(incl)+'\n')
    return found

  def configure(self):
    if self.framework.argDB['download-'+self.package]:
      self.framework.argDB['with-'+self.package] = 1
    if 'with-'+self.package+'-dir' in self.framework.argDB or 'with-'+self.package+'-include' in self.framework.argDB or 'with-'+self.package+'-lib' in self.framework.argDB:
      self.framework.argDB['with-'+self.package] = 1
      
    if self.framework.argDB['with-'+self.package]:
      if hasattr(self,'mpi') and self.mpi.usingMPIUni:
        raise RuntimeError('Cannot use '+self.name+' with MPIUNI, you need a real MPI')
      if self.framework.argDB['with-64-bit-ints']:
        raise RuntimeError('Cannot use '+self.name+' with 64 bit integers, it is not coded for this capability')    
      if not self.clanguage.precision.lower() == 'double':
        raise RuntimeError('Cannot use '+self.name+' withOUT double precision numbers, it is not coded for this capability')    
      if not self.complex && self.clanguage.scalartype.lower() == 'complex':
        raise RuntimeError('Cannot use '+self.name+' with complex numbers it is not coded for this capability')    
      self.executeTest(self.configureLibrary)
    return

