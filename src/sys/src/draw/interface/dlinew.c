#ifndef lint
static char vcid[] = "$Id: dlinew.c,v 1.4 1996/03/19 21:28:06 bsmith Exp bsmith $";
#endif
/*
       Provides the calling sequences for all the basic Draw routines.
*/
#include "drawimpl.h"  /*I "draw.h" I*/

/*@
   DrawLineSetWidth - Sets the line width for future draws.  The width is
   relative to the user coordinates of the window; 0.0 denotes the natural
   width; 1.0 denotes the entire viewport. 

   Input Parameters:
.  draw - the drawing context
.  width - the width in user coordinates

.keywords:  draw, line, set, width

.seealso:  DrawLineGetWidth()
@*/
int DrawLineSetWidth(Draw draw,double width)
{
  PetscValidHeaderSpecific(draw,DRAW_COOKIE);
  if (draw->type == NULLWINDOW) return 0;
  return (*draw->ops.linesetwidth)(draw,width);
}
