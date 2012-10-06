#include <assert.h>
#include <stdio.h>
#include "fixedpoint.h"

int main(int argc, char** argv) {
  int x = 17;
  int y = 34;
  printf("%d\n", Float(1));
  
  printf("%d\n", Round(Float(1)));
  printf("%d\n", Round0(Float(1)));

  printf("%f\n", ((float)DivI(Float(4), 5))/F);
  printf("%f\n", ((float)DivI(Float(5), 5))/F);
  printf("%f\n", ((float)DivI(Float(6), 5))/F);

  printf("%f\n", ((float)DivI(Float(4), 5))/F);
  printf("%f\n", ((float)DivI(Float(5), 5))/F);
  printf("%f\n", ((float)DivI(Float(6), 5))/F);

  printf("%f\n", ((float)AddI(Float(4), 5))/F);
  printf("%f\n", ((float)SubI(Float(4), 5))/F);
  printf("%f\n", ((float)MulI(Float(4), 5))/F);

  printf("%f\n", ((float)AddF(Float(4), Float(5)))/F);
  printf("%f\n", ((float)SubF(Float(4), Float(5)))/F);
  printf("%f\n", ((float)MulF(Float(4), Float(5)))/F);

  int i;
  float f;
  for (i = 0; i < 10; i++) {
    printf("-----------------------------------\n");
    f = ((float)MulI(Float(i), 2)) / F;
    printf("%f\n", f);

    f = ((float)AddI(MulI(Float(i), 2), 1)) / F;
    printf("%f\n", f);

    f = ((float)DivF(MulI(Float(i), 2), AddI(MulI(Float(i), 2), 1))) / F;
    printf("%f\n", f);
    
    f = ((float) MulF(DivF(MulI(Float(i), 2), AddI(MulI(Float(i), 2), 1)), Float(1))) / F;
    printf("%f\n", f);
  }
}