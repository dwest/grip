/*
  cppmain.cc

  grip links against libid3, which is a c++ library; therefore,
  "main" has to be a c++ function in order to ensure that correct
  static initialization and shutdown happens for C++ library code.
*/


extern "C" int Cmain(int ac, char *av[]);


int 
main(int ac, char *av[])
{
  return Cmain(ac, av);
}
