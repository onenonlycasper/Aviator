/***************************************************************************/
/*                                                                         */
/*  ftxf86.c                                                               */
/*                                                                         */
/*    FreeType utility file for X11 support (body).                        */
/*                                                                         */
/*  Copyright 2002, 2003, 2004 by                                          */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#include "../../include/ft2build.h"
#include "../../include/freetype/ftxf86.h"
#include "../../include/freetype/internal/ftobjs.h"
#include "../../include/freetype/internal/services/svxf86nm.h"


  /* documentation is in ftxf86.h */

  FT_EXPORT_DEF( const char* )
  FT_Get_X11_Font_Format( FT_Face  face )
  {
    const char*  result = NULL;


    if ( face )
      FT_FACE_FIND_SERVICE( face, result, XF86_NAME );

    return result;
  }


/* END */
