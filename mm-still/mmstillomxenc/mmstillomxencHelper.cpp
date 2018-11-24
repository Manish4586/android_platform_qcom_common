/* ========================================================================= * 
   Purpose:  Shared object Helper library used for fuzzing APIs

           -------------------------------------------------------
      Copyright © 2012 Qualcomm Technologies, Inc. All Rights Reserved.
             Qualcomm Technologies Proprietary and Confidential.
* ========================================================================= */
#include <stdio.h>
#include <string.h>
#include "cFuzzerLog.h"
#include "FuzzerLibs.h"
extern cFuzzerLog cFL ;
#include "mmstillomxencHelper.h"
#include "mmstill_jpeg_omx_enc.h"

/*==========================================================================*
	Name: int init_module_helper()

	Input:
	       None   

	Return: 
           SUCCESS : RET_SUCCESS ( 0) 
           FAILURE : RET_FAILURE (-1)

	Purpose:
	  This is the init function. Make all initializations here 
*==========================================================================*/
extern "C" int init_module_helper()
{
     
    //
    //    Add your initialization code here
    //
    //
    //MMStillJpegOMX_Init();
    printf("after OMX_Init\n");
    return RET_SUCCESS;
}

/*==========================================================================*
	Name: int deinit_module_helper()

	Input:
	       None   

	Return: 
           SUCCESS : RET_SUCCESS ( 0) 
           FAILURE : RET_FAILURE (-1)

	Purpose:
	  This is the de-init function. Free all resources here 
*==========================================================================*/
extern "C" int deinit_module_helper()
{
     
    //
    //    Free resources here
    //
    //
   // MMStillJpegOMX_DeInit();
    return RET_SUCCESS;
}

