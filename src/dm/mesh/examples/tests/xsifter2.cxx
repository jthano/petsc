static char help[] = "Sifter Cone and Support Tests.\n\n";

#include <petsc.h>
#include "xsifterTest.hh"


typedef ALE::Test::XSifterTester::xsifter_type     xsifter_type;



#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc, char *argv[])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscInitialize(&argc, &argv, (char *) 0, help); CHKERRQ(ierr);
  {
    ALE::Component::ArgDB argDB(ALE::Test::XSifterTester().argDB, argc, argv);
    ALE::Obj<xsifter_type> xsifterFork = ALE::Test::XSifterTester::createForkXSifter(PETSC_COMM_SELF, argDB);
    ierr = ALE::Test::XSifterTester::ConeTest(xsifterFork, argDB, "Fork XSifter"); CHKERRQ(ierr);
    ALE::Obj<xsifter_type> xsifterHat = ALE::Test::XSifterTester::createHatXSifter(PETSC_COMM_SELF, argDB);
    ierr = ALE::Test::XSifterTester::ConeTest(xsifterHat, argDB, "Hat XSifter"); CHKERRQ(ierr);
  }
  ierr = PetscFinalize();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
