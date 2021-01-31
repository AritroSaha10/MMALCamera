#include <iostream>
#include "MMALCamera.h"

int main()
{
   // Create object
   MMALCamera *myCamera = new MMALCamera();

   // Start preview
   std::cout << "Starting preview...\n";
   myCamera->startPreview();

   sleep(10);

   // End preview
   std::cout << "Ending preview...\n";
   myCamera->endPreview();

   delete myCamera;
   myCamera = NULL;

   std::cout << "Done!\n";
   return 0;
}
