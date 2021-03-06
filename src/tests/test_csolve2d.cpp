#include "../ESolver.hpp"
#include "../TArray.hpp"
#include "../Parformer.hpp"
#include <blitz/array.h>
#include <cstdio>
#include <iostream>
#include <math.h>
#include <mpi.h>
#include "../Par_util.hpp"

using namespace TArrayn;
using namespace ESolver;
using namespace std;
using namespace Transformer;
using blitz::Array;
using blitz::firstIndex;
using blitz::secondIndex;
using blitz::thirdIndex;

/* Test the Elliptic solver as specialized for one Chebyshev dimension
   (vertical/z), using both Fourier x/y components and Sine/Cosine x/y
   components */
int main(int argc, char * argv[]) {
   MPI_Init(0,0);
   int myrank;
   MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
   int sz = 0; /* 3D size */
   if (argc > 1) {
      sz = atoi(argv[1]);
   }
   if (sz == 0) {
      sz = 32;
   }
   double M = 100; // Coefficient for Helmholtz problem
   double length = 2.5;
//   double length = 2;
   bool returnOK = true;
   firstIndex ii; secondIndex jj; thirdIndex kk;

   DTArray & exact_soln = *alloc_array(sz,1,sz), 
           & numer_soln = *alloc_array(sz,1,sz), 
           & rhs        = *alloc_array(sz,1,sz),
           & kernel     = *alloc_array(sz,1,sz);
   TransWrapper wrapperF(sz,1,sz,FOURIER,FOURIER,NONE);
   TransWrapper wrapperR(sz,1,sz,SINE,SINE,NONE); // dual-purposed real-valued wrapper

   Array<double,1> xx(split_range(sz)), yy(1), zz(sz);
   xx = -length/2 + length*(ii+0.5)/sz; // Grid goes from -L/2 -> L/2
   yy = 0;
   zz = -length/2*cos(M_PI*ii/(sz-1)); // Grid goes from -L/2 -> L/2

   kernel = pow(xx(ii)-0.1,2) +  pow(zz(kk)-0.1,2); 
   exact_soln = exp(-20*kernel);
   rhs = 40*(-2 + 40*kernel)*exact_soln;
   double meansoln = pvsum(exact_soln)/sz/sz;
   exact_soln = exact_soln - meansoln;

   // z-derivative for Neumann BCs
   // Bottom
   rhs(blitz::Range::all(),blitz::Range::all(),0) =
       40*(-length/2-.1)*
       exp(-20*kernel(blitz::Range::all(),blitz::Range::all(),0));
   // Top
   rhs(blitz::Range::all(),blitz::Range::all(),sz-1) =
      -40*(length/2-0.1)*
      exp(-20*kernel(blitz::Range::all(),blitz::Range::all(),sz-1));
   
   /*
   rhs(blitz::Range::all(),blitz::Range::all(),0)=
      exact_soln(blitz::Range::all(),blitz::Range::all(),0);

   rhs(blitz::Range::all(),blitz::Range::all(),sz-1)=
      exact_soln(blitz::Range::all(),blitz::Range::all(),sz-1);*/

   ElipSolver imag_solve(0,&wrapperF,length,length,length);
   ElipSolver real_solve(0,&wrapperR,length,length,length);

   // Solve poisson problems
   double maxdiff;
   imag_solve.solve(rhs,numer_soln,FOURIER,FOURIER,CHEBY,0,1);
//   cout << rhs;
//   cout <<numer_soln;
//   cout << exact_soln;
   maxdiff = psmax(max(abs(numer_soln - exact_soln)));
   if (maxdiff > 1e-8) returnOK = false;
   if (!myrank) cout << "Poisson FFT(" << sz << "): " << maxdiff << "\n";
   real_solve.solve(rhs,numer_soln,COSINE,COSINE,CHEBY,0,1);
   maxdiff = psmax(max(abs(numer_soln - exact_soln)));
   if (maxdiff > 1e-8) returnOK = false;
   if (!myrank) cout << "Poisson DCT(" << sz << "): " << maxdiff << "\n";
   real_solve.solve(rhs,numer_soln,SINE,SINE,CHEBY,0,1);
   maxdiff = psmax(max(abs(numer_soln - exact_soln - meansoln)));
   if (maxdiff > 1e-8) returnOK = false;
   if (!myrank) cout << "Poisson DST(" << sz << "): " << maxdiff << "\n";

   exact_soln = exact_soln + meansoln;
   // Solve Helmholtz problems
   imag_solve.change_m(M);
   real_solve.change_m(M);

   rhs = rhs  - M*exact_soln;
   // Bottom
   rhs(blitz::Range::all(),blitz::Range::all(),0) =
       40*(-length/2-.1)*
       exp(-20*kernel(blitz::Range::all(),blitz::Range::all(),0));
   // Top
   rhs(blitz::Range::all(),blitz::Range::all(),sz-1) =
      -40*(length/2-0.1)*
      exp(-20*kernel(blitz::Range::all(),blitz::Range::all(),sz-1));


   imag_solve.solve(rhs,numer_soln,FOURIER,FOURIER,CHEBY,0,1);
   maxdiff = psmax(max(abs(numer_soln - exact_soln)));
   if (maxdiff > 1e-8) returnOK = false;
   if (!myrank) cout << "Helmholtz FFT(" << sz << "): " << maxdiff << "\n";
   real_solve.solve(rhs,numer_soln,COSINE,COSINE,CHEBY,0,1);
   maxdiff = psmax(max(abs(numer_soln - exact_soln)));
   if (maxdiff > 1e-8) returnOK = false;
   if (!myrank) cout << "Helmholtz DCT(" << sz << "): " << maxdiff << "\n";
   real_solve.solve(rhs,numer_soln,SINE,SINE,CHEBY,0,1);
   maxdiff = psmax(max(abs(numer_soln - exact_soln)));
   if (maxdiff > 1e-8) returnOK = false;
   if (!myrank) cout << "Helmholtz DST(" << sz << "): " << maxdiff << "\n";
   MPI_Finalize();
   return (returnOK? 0:1);
}
