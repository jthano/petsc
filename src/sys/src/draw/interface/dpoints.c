#ifndef lint
static char vcid[] = "$Id: dpoints.c,v 1.4 1996/03/19 21:28:06 bsmith Exp bsmith $";
#endif
/*
       Provides the calling sequences for all the basic Draw routines.
*/
#include "drawimpl.h"  /*I "draw.h" I*/

/*@
   DrawPointSetSize - Sets the point size for future draws.  The size is
   relative to the user coordinates of the window; 0.0 denotes the natural
   width, 1.0 denotes the entire viewport. 

   Input Parameters:
.  draw - the drawing context
.  width - the width in user coordinates

   Note: 
   Even a size of zero insures that a single pixel is colored.

.keywords: draw, point, set, size
@*/
int DrawPointSetSize(Draw draw,double width)
{
  PetscValidHeaderSpecific(draw,DRAW_COOKIE);
  if (draw->type == NULLWINDOW) return 0;
  if (width < 0.0 || width > 1.0) SETERRQ(1,"DrawPointSetSize: Bad size");
  return (*draw->ops.pointsetsize)(draw,width);
}

