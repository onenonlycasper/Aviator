/***************************************************************************/
/*                                                                         */
/*  ttpic.h                                                                */
/*                                                                         */
/*    The FreeType position independent code services for truetype module. */
/*                                                                         */
/*  Copyright 2009, 2012, 2013 by                                          */
/*  Oran Agra and Mickey Gabel.                                            */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#ifndef __TTPIC_H__
#define __TTPIC_H__


FT_BEGIN_HEADER

#ifndef FT_CONFIG_OPTION_PIC

#define TT_SERVICES_GET                  tt_services
#define TT_SERVICE_GX_MULTI_MASTERS_GET  tt_service_gx_multi_masters
#define TT_SERVICE_TRUETYPE_GLYF_GET     tt_service_truetype_glyf
#define TT_SERVICE_PROPERTIES_GET        tt_service_properties

#else /* FT_CONFIG_OPTION_PIC */

#include "../../include/freetype/ftmm.h"
#include "../../include/freetype/internal/services/svmm.h"
#include "../../include/freetype/internal/services/svttglyf.h"
#include "../../include/freetype/internal/services/svprop.h"


  typedef struct  TTModulePIC_
  {
    FT_ServiceDescRec*          tt_services;
#ifdef TT_CONFIG_OPTION_GX_VAR_SUPPORT
    FT_Service_MultiMastersRec  tt_service_gx_multi_masters;
#endif
    FT_Service_TTGlyfRec        tt_service_truetype_glyf;
    FT_Service_PropertiesRec    tt_service_properties;

  } TTModulePIC;


#define GET_PIC( lib )                                      \
          ( (TTModulePIC*)((lib)->pic_container.truetype) )
#define TT_SERVICES_GET                       \
          ( GET_PIC( library )->tt_services )
#define TT_SERVICE_GX_MULTI_MASTERS_GET                       \
          ( GET_PIC( library )->tt_service_gx_multi_masters )
#define TT_SERVICE_TRUETYPE_GLYF_GET                       \
          ( GET_PIC( library )->tt_service_truetype_glyf )
#define TT_SERVICE_PROPERTIES_GET                       \
          ( GET_PIC( library )->tt_service_properties )


  /* see ttpic.c for the implementation */
  void
  tt_driver_class_pic_free( FT_Library  library );

  FT_Error
  tt_driver_class_pic_init( FT_Library  library );

#endif /* FT_CONFIG_OPTION_PIC */

 /* */


FT_END_HEADER

#endif /* __TTPIC_H__ */


/* END */
