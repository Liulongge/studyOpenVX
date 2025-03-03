/*
 *
 * Copyright (c) 2018 Texas Instruments Incorporated
 *
 * All rights reserved not granted herein.
 *
 * Limited License.
 *
 * Texas Instruments Incorporated grants a world-wide, royalty-free, non-exclusive
 * license under copyrights and patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this software subject to the
 * terms herein.  With respect to the foregoing patent license, such license is granted
 * solely to the extent that any such patent is necessary to Utilize the software alone.
 * The patent license shall not apply to any combinations which include this software,
 * other than combinations with devices manufactured by or for TI ("TI Devices").
 * No hardware patent is licensed hereunder.
 *
 * Redistributions must preserve existing copyright notices and reproduce this license
 * (including the above copyright notice and the disclaimer and (if applicable) source
 * code license limitations below) in the documentation and/or other materials provided
 * with the distribution
 *
 * Redistribution and use in binary form, without modification, are permitted provided
 * that the following conditions are met:
 *
 * *       No reverse engineering, decompilation, or disassembly of this software is
 * permitted with respect to any software provided in binary form.
 *
 * *       any redistribution and use are licensed by TI for use only with TI Devices.
 *
 * *       Nothing shall obligate TI to provide you with source code for the software
 * licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution of the
 * source code are permitted provided that the following conditions are met:
 *
 * *       any redistribution and use of the source code, including any resulting derivative
 * works, are licensed by TI for use only with TI Devices.
 *
 * *       any redistribution and use of any object code compiled from the source code
 * and any resulting derivative works, are licensed by TI for use only with TI Devices.
 *
 * Neither the name of Texas Instruments Incorporated nor the names of its suppliers
 *
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * DISCLAIMER.
 *
 * THIS SOFTWARE IS PROVIDED BY TI AND TI'S LICENSORS "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TI AND TI'S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "TI/tivx.h"
#include "TI/j7.h"
#include "VX/vx.h"
#include "TI/tivx_event.h"
#include "tivx_hwa_kernels.h"
#include "tivx_kernel_capture.h"
#include "TI/tivx_target_kernel.h"
#include "tivx_kernels_target_utils.h"
#include "tivx_hwa_capture_priv.h"

#include <TI/tivx_queue.h>
#include <ti/drv/fvid2/fvid2.h>
#include <ti/drv/csirx/csirx.h>

#define CAPTURE_FRAME_DROP_LEN                          (4096U*4U)

#define CAPTURE_INST_ID_INVALID                         (0xFFFFU)

typedef struct tivxCaptureParams_t tivxCaptureParams;

typedef struct
{
    uint32_t instId;
    /**< Csirx Drv Instance ID. */
    uint8_t numCh;
    /**< Number of channels processed on given CSIRX DRV instance. */
    uint32_t chVcMap[TIVX_CAPTURE_MAX_CH];
    /**< Virtual ID for channels for current capture instance. */
    Fvid2_Handle drvHandle;
    /**< FVID2 capture driver handle. */
    Csirx_CreateParams createPrms;
    /**< Csirx create time parameters */
    Csirx_CreateStatus createStatus;
    /**< Csirx create time status */
    Fvid2_CbParams drvCbPrms;
    /**< Capture callback params */
    uint8_t raw_capture;
    /**< flag indicating raw capture */
    Csirx_InstStatus captStatus;
    /**< CSIRX Capture status. */
    Csirx_DPhyCfg dphyCfg;
    /**< CSIRX DPHY configuration. */
    tivxCaptureParams *captParams;
    /**< Reference to capture node parameters. */
} tivxCaptureInstParams;

struct tivxCaptureParams_t
{
    tivxCaptureInstParams instParams[TIVX_CAPTURE_MAX_INST];
    /* Capture Instance parameters */
    uint32_t numOfInstUsed;
    /**< Number of CSIRX DRV instances used in current TIOVX Node. */
    uint8_t numCh;
    /**< Number of channels processed on given capture node instance. */
    tivx_obj_desc_t *img_obj_desc[TIVX_CAPTURE_MAX_CH];
    /* Captured Images */
    uint8_t steady_state_started;
    /**< Flag indicating whether or not steady state has begun. */
    tivx_event  frame_available;
    /**< Following Queues i.e. freeFvid2FrameQ, pendingFrameQ, fvid2_free_q_mem,
     *   fvid2Frames, and pending_frame_free_q_mem are for given instance of the
     *   Node. If Node instance contains more than 1 instances of the CSIRX DRV
     *   instances, then first 'n' channels are for first instance of the driver
     *   then n channels for next driver and so on... */
    /**< Event indicating when a frame is available. */
    tivx_queue freeFvid2FrameQ[TIVX_CAPTURE_MAX_CH];
    /**< Internal FVID2 queue */
    tivx_queue pendingFrameQ[TIVX_CAPTURE_MAX_CH];
    /**< Internal pending frame queue */
    uintptr_t fvid2_free_q_mem[TIVX_CAPTURE_MAX_CH][TIVX_CAPTURE_MAX_NUM_BUFS];
    /**< FVID2 queue mem */
    Fvid2_Frame fvid2Frames[TIVX_CAPTURE_MAX_CH][TIVX_CAPTURE_MAX_NUM_BUFS];
    /**< FVID2 frame structs */
    uintptr_t pending_frame_free_q_mem[TIVX_CAPTURE_MAX_CH][TIVX_CAPTURE_MAX_NUM_BUFS];
    /**< pending frame queue mem */
};

static tivx_target_kernel vx_capture_target_kernel1 = NULL;
static tivx_target_kernel vx_capture_target_kernel2 = NULL;

static vx_status captDrvCallback(Fvid2_Handle handle, void *appData, void *reserved);
static uint32_t tivxCaptureExtractInCsiDataType(uint32_t format);
static uint32_t tivxCaptureExtractCcsFormat(uint32_t format);
static uint32_t tivxCaptureExtractDataFormat(uint32_t format);
static vx_status tivxCaptureEnqueueFrameToDriver(
       tivx_obj_desc_object_array_t *output_desc,
       tivxCaptureParams *prms);
static void tivxCaptureSetCreateParams(
       tivxCaptureParams *prms,
       tivx_obj_desc_user_data_object_t *obj_desc);
static vx_status VX_CALLBACK tivxCaptureProcess(
       tivx_target_kernel_instance kernel,
       tivx_obj_desc_t *obj_desc[],
       uint16_t num_params, void *priv_arg);
static vx_status VX_CALLBACK tivxCaptureCreate(
       tivx_target_kernel_instance kernel,
       tivx_obj_desc_t *obj_desc[],
       uint16_t num_params, void *priv_arg);
static vx_status VX_CALLBACK tivxCaptureDelete(
       tivx_target_kernel_instance kernel,
       tivx_obj_desc_t *obj_desc[],
       uint16_t num_params, void *priv_arg);
static vx_status VX_CALLBACK tivxCaptureControl(
       tivx_target_kernel_instance kernel,
       uint32_t node_cmd_id, tivx_obj_desc_t *obj_desc[],
       uint16_t num_params, void *priv_arg);
static vx_status tivxCaptureGetStatistics(tivxCaptureParams *prms,
    tivx_obj_desc_user_data_object_t *usr_data_obj);
static void tivxCaptureCopyStatistics(tivxCaptureParams *prms,
    tivx_capture_statistics_t *capt_status_prms);
static void tivxCaptureGetChannelIndices(const tivxCaptureParams *prms,
                                         uint32_t instId,
                                         uint32_t *startChIdx,
                                         uint32_t *endChIdx);
static uint32_t tivxCaptureGetNodeChannelNum(const tivxCaptureParams *prms,
                                             uint32_t instId,
                                             uint32_t chId);
static uint32_t tivxCaptureMapInstId(uint32_t instId);
static void tivxCapturePrintStatus(tivxCaptureInstParams *prms);

/**
 *******************************************************************************
 *
 * \brief Callback function from driver to application
 *
 * Callback function gets called from Driver to application on reception of
 * a frame
 *
 * \param  handle       [IN] Driver handle for which callback has come.
 * \param  appData      [IN] Application specific data which is registered
 *                           during the callback registration.
 * \param  reserved     [IN] Reserved.
 *
 * \return  SYSTEM_LINK_STATUS_SOK on success
 *
 *******************************************************************************
 */
static vx_status captDrvCallback(Fvid2_Handle handle, void *appData, void *reserved)
{
    tivxCaptureParams *prms = (tivxCaptureParams*)appData;

    tivxEventPost(prms->frame_available);

    return (vx_status)VX_SUCCESS;
}

static vx_status tivxCaptureEnqueueFrameToDriver(
       tivx_obj_desc_object_array_t *output_desc,
       tivxCaptureParams *prms)
{
    vx_status status = (vx_status)VX_SUCCESS;
    int32_t fvid2_status = FVID2_SOK;
    void *output_image_target_ptr;
    uint64_t captured_frame;
    uint32_t chId = 0U;
    static Fvid2_FrameList frmList;
    Fvid2_Frame *fvid2Frame;
    uint32_t startChIdx, endChIdx, instIdx;
    tivxCaptureInstParams *instParams;

    tivxGetObjDescList(output_desc->obj_desc_id, (tivx_obj_desc_t **)prms->img_obj_desc,
                       prms->numCh);

    /* Prepare and queue frame-list for each instance */
    for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
    {
        instParams = &prms->instParams[instIdx];
        tivxCaptureGetChannelIndices(prms, instIdx, &startChIdx, &endChIdx);
        frmList.numFrames = instParams->numCh;
        for (chId = startChIdx ; chId < endChIdx ; chId++)
        {
            if ((uint32_t)TIVX_OBJ_DESC_RAW_IMAGE == prms->img_obj_desc[0]->type)
            {
                tivx_obj_desc_raw_image_t *raw_image;

                raw_image = (tivx_obj_desc_raw_image_t *)prms->img_obj_desc[chId];

                /* Question: is the fact that we are just using mem_ptr[0] and not remaining planes correct? */
                output_image_target_ptr = tivxMemShared2TargetPtr(&raw_image->mem_ptr[0]);


                captured_frame = ((uintptr_t)output_image_target_ptr +
                    (uint64_t)tivxComputePatchOffset(0, 0, &raw_image->imagepatch_addr[0U]));
            }
            else
            {
                tivx_obj_desc_image_t *image;
                image = (tivx_obj_desc_image_t *)prms->img_obj_desc[chId];

                /* Question: is the fact that we are just using mem_ptr[0] and not remaining exposures correct? */
                output_image_target_ptr = tivxMemShared2TargetPtr(&image->mem_ptr[0]);

                captured_frame = ((uintptr_t)output_image_target_ptr +
                    (uint64_t)tivxComputePatchOffset(0, 0, &image->imagepatch_addr[0U]));
            }

            tivxQueueGet(&prms->freeFvid2FrameQ[chId], (uintptr_t*)&fvid2Frame, TIVX_EVENT_TIMEOUT_NO_WAIT);

            if (NULL != fvid2Frame)
            {
                /* Put into frame list as it is for same driver instance */
                frmList.frames[(chId - startChIdx)]           = fvid2Frame;
                frmList.frames[(chId - startChIdx)]->chNum    = instParams->chVcMap[(chId - startChIdx)];
                frmList.frames[(chId - startChIdx)]->addr[0U] = captured_frame;
                frmList.frames[(chId - startChIdx)]->appData  = output_desc;
            }
            else
            {
                VX_PRINT(VX_ZONE_ERROR, " CAPTURE: Could not retrieve buffer from buffer queue!!!\n");
            }
        }

        /* All the frames from frame-list */
        fvid2_status = Fvid2_queue(instParams->drvHandle, &frmList, 0);
        if (FVID2_SOK != fvid2_status)
        {
            status = (vx_status)VX_FAILURE;
            VX_PRINT(VX_ZONE_ERROR, " CAPTURE: ERROR: Frame could not be queued for frame %d !!!\n", chId);
            break;
        }
    }

    return status;
}

static uint32_t tivxCaptureExtractInCsiDataType(uint32_t format)
{
    uint32_t inCsiDataType;

    switch (format)
    {
        case (vx_df_image)VX_DF_IMAGE_RGB:
            inCsiDataType = FVID2_CSI2_DF_RGB888;
            break;
        case (vx_df_image)VX_DF_IMAGE_RGBX:
            inCsiDataType = FVID2_CSI2_DF_RGB888;
            break;
        case (vx_df_image)VX_DF_IMAGE_U16:
        case (uint32_t)TIVX_RAW_IMAGE_P12_BIT:
            inCsiDataType = FVID2_CSI2_DF_RAW12;
            break;
        case (vx_df_image)VX_DF_IMAGE_UYVY:
        case (vx_df_image)VX_DF_IMAGE_YUYV:
            inCsiDataType = FVID2_CSI2_DF_YUV422_8B;
            break;
        default:
            inCsiDataType = 0xFFFFFFFFu;
            break;
    }

    return inCsiDataType;
}

static uint32_t tivxCaptureExtractCcsFormat(uint32_t format)
{
    uint32_t ccsFormat = FVID2_CCSF_BITS12_PACKED;

    switch (format)
    {
        case (uint32_t)TIVX_RAW_IMAGE_P12_BIT:
            ccsFormat = FVID2_CCSF_BITS12_PACKED;
            break;
        case (vx_enum)TIVX_RAW_IMAGE_16_BIT:
        case (vx_df_image)VX_DF_IMAGE_U16:
        case (vx_df_image)VX_DF_IMAGE_UYVY:
        case (vx_df_image)VX_DF_IMAGE_YUYV:
            ccsFormat = FVID2_CCSF_BITS12_UNPACKED16;
            break;
        default:
            ccsFormat = FVID2_CCSF_MAX;
            break;
    }

    return ccsFormat;
}

static uint32_t tivxCaptureMapInstId(uint32_t instId)
{
    uint32_t drvInstId = 0xFFFF;
    switch (instId)
    {
        case 0:
            drvInstId = CSIRX_INSTANCE_ID_0;
            break;
        case 1:
            drvInstId = CSIRX_INSTANCE_ID_1;
            break;
        default:
            /* do nothing */
            break;
    }

    return (drvInstId);
}

/* TODO: Complete this case statement */
static uint32_t tivxCaptureExtractDataFormat(uint32_t format)
{
    uint32_t dataFormat = FVID2_DF_BGRX32_8888;

    return dataFormat;
}

static void tivxCaptureSetCreateParams(
       tivxCaptureParams *prms,
       tivx_obj_desc_user_data_object_t *obj_desc)
{
    uint32_t loopCnt = 0U, i, format, width, height, planes, stride[TIVX_IMAGE_MAX_PLANES];
    void *capture_config_target_ptr;
    tivx_capture_params_t *params;
    uint32_t chIdx, instId = 0U, instIdx;
    Csirx_CreateParams *createParams;

    capture_config_target_ptr = tivxMemShared2TargetPtr(&obj_desc->mem_ptr);

    tivxMemBufferMap(capture_config_target_ptr, obj_desc->mem_size,
        (vx_enum)VX_MEMORY_TYPE_HOST, (vx_enum)VX_READ_ONLY);

    params = (tivx_capture_params_t *)capture_config_target_ptr;

    /* Scan through all the channels provided in the Node instance and prepare CSIRX DRV instance data/cfg */
    for (chIdx = 0U ; chIdx < params->numCh ; chIdx++)
    {
        instId = params->chInstMap[chIdx];
        prms->instParams[instId].chVcMap[prms->instParams[instId].numCh] = params->chVcNum[chIdx];
        prms->instParams[instId].numCh++;
    }

    if ((vx_enum)TIVX_OBJ_DESC_RAW_IMAGE == (vx_enum)prms->img_obj_desc[0]->type)
    {
        tivx_obj_desc_raw_image_t *raw_image;
        raw_image = (tivx_obj_desc_raw_image_t *)prms->img_obj_desc[0];
        format = raw_image->params.format[0].pixel_container; /* TODO: Question: what should be done when this is different per exposure */
        width = raw_image->params.width;
        height = raw_image->params.height + (raw_image->params.meta_height_before + raw_image->params.meta_height_after);
        planes = raw_image->params.num_exposures;
        for (i = 0; i < planes; i++)
        {
            stride[i] = (uint32_t)raw_image->imagepatch_addr[i].stride_y;
        }
        prms->instParams[instId].raw_capture = 1U;
    }
    else
    {
        tivx_obj_desc_image_t *image;
        image = (tivx_obj_desc_image_t *)prms->img_obj_desc[0];
        format = image->format;
        width = image->imagepatch_addr[0].dim_x;
        height = image->imagepatch_addr[0].dim_y;
        planes = image->planes;
        for (i = 0; i < planes; i++)
        {
            stride[i] = (uint32_t)image->imagepatch_addr[i].stride_y;
        }
        prms->instParams[instId].raw_capture = 0U;
    }

    /* Do following for each CSIRX DRV instance in the current Node */
    for (instIdx = 0U ; instIdx < params->numInst ; instIdx++)
    {
        prms->instParams[instIdx].captParams = prms;
        /* set instance configuration parameters */
        createParams = &prms->instParams[instIdx].createPrms;
        Csirx_createParamsInit(createParams);

        /* set module configuration parameters */
        createParams->instCfg.enableCsiv2p0Support = params->instCfg[instIdx].enableCsiv2p0Support;
        createParams->instCfg.enableErrbypass = (uint32_t)FALSE;
        createParams->instCfg.numDataLanes = params->instCfg[instIdx].numDataLanes;
        for (loopCnt = 0U ;
             loopCnt < createParams->instCfg.numDataLanes ;
             loopCnt++)
        {
            createParams->instCfg.dataLanesMap[loopCnt] = params->instCfg[instIdx].dataLanesMap[loopCnt];
        }

        createParams->numCh = prms->instParams[instIdx].numCh;
        for (loopCnt = 0U ; loopCnt < createParams->numCh ; loopCnt++)
        {
            createParams->chCfg[loopCnt].chId = loopCnt;
            createParams->chCfg[loopCnt].chType = CSIRX_CH_TYPE_CAPT;
            createParams->chCfg[loopCnt].vcNum = prms->instParams[instIdx].chVcMap[loopCnt];

            if ((uint32_t)TIVX_OBJ_DESC_RAW_IMAGE == prms->img_obj_desc[0]->type)
            {
                createParams->chCfg[loopCnt].inCsiDataType =
                    FVID2_CSI2_DF_RAW12;
            }
            else
            {
                createParams->chCfg[loopCnt].inCsiDataType =
                    tivxCaptureExtractInCsiDataType(format);
            }
            createParams->chCfg[loopCnt].outFmt.width =
                width;
            createParams->chCfg[loopCnt].outFmt.height =
                height;
            for (i = 0; i < planes; i ++)
            {
                createParams->chCfg[loopCnt].outFmt.pitch[i] =
                    stride[i];
            }

            createParams->chCfg[loopCnt].outFmt.dataFormat =
                tivxCaptureExtractDataFormat(format);
            createParams->chCfg[loopCnt].outFmt.ccsFormat =
                tivxCaptureExtractCcsFormat(format);
        }
        /* set frame drop buffer parameters */
        createParams->frameDropBufLen = CAPTURE_FRAME_DROP_LEN;
        createParams->frameDropBuf = (uint64_t)tivxMemAlloc(createParams->frameDropBufLen, (int32_t)TIVX_MEM_EXTERNAL);

        /* set instance to be used for capture */
        prms->instParams[instIdx].instId = tivxCaptureMapInstId(params->instId[instIdx]);
        prms->numOfInstUsed++;
    }

    tivxMemBufferUnmap(capture_config_target_ptr,
       obj_desc->mem_size, (vx_enum)VX_MEMORY_TYPE_HOST,
       (vx_enum)VX_READ_ONLY);
}

static vx_status VX_CALLBACK tivxCaptureProcess(
       tivx_target_kernel_instance kernel,
       tivx_obj_desc_t *obj_desc[],
       uint16_t num_params, void *priv_arg)
{
    vx_status status = (vx_status)VX_SUCCESS;
    int32_t fvid2_status = FVID2_SOK;
    tivxCaptureParams *prms = NULL;
    tivx_obj_desc_object_array_t *output_desc;
    static Fvid2_FrameList frmList;
    vx_uint32 size, frmIdx = 0U, chId = 0U;
    vx_enum state;
    Fvid2_Frame *fvid2Frame;
    tivx_obj_desc_object_array_t *desc;
    uint32_t instIdx;
    tivxCaptureInstParams *instParams;

    if ( (num_params != TIVX_KERNEL_CAPTURE_MAX_PARAMS)
        || (NULL == obj_desc[TIVX_KERNEL_CAPTURE_INPUT_ARR_IDX])
        || (NULL == obj_desc[TIVX_KERNEL_CAPTURE_OUTPUT_IDX])
    )
    {
        status = (vx_status)VX_FAILURE;
    }

    if((vx_status)VX_SUCCESS == status)
    {
        output_desc = (tivx_obj_desc_object_array_t *)obj_desc[TIVX_KERNEL_CAPTURE_OUTPUT_IDX];

        status = tivxGetTargetKernelInstanceContext(kernel,
            (void **)&prms, &size);

        if (((vx_status)VX_SUCCESS != status) || (NULL == prms) ||
            (sizeof(tivxCaptureParams) != size))
        {
            status = (vx_status)VX_FAILURE;
        }
        else
        {
            status = tivxGetTargetKernelInstanceState(kernel, &state);
        }
    }

    if((vx_status)VX_SUCCESS == status)
    {
        /* Steady state: receives a buffer and returns a buffer */
        if ((vx_enum)VX_NODE_STATE_STEADY == state)
        {
            /* Providing buffers to capture source */
            status = tivxCaptureEnqueueFrameToDriver(output_desc, prms);

            if ((vx_status)VX_SUCCESS != status)
            {
                status = (vx_status)VX_FAILURE;
                VX_PRINT(VX_ZONE_ERROR, " CAPTURE: ERROR: Enqueue Frame to Driver failed !!!\n");
            }

            /* Starts FVID2 on initial frame */
            if ((vx_status)VX_SUCCESS == status)
            {
                if (0U == prms->steady_state_started)
                {
                    /* start all driver instances in the node */
                    for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
                    {
                        fvid2_status = Fvid2_start(prms->instParams[instIdx].drvHandle, NULL);
                        if (FVID2_SOK != fvid2_status)
                        {
                            status = (vx_status)VX_FAILURE;
                            VX_PRINT(VX_ZONE_ERROR, " CAPTURE: ERROR: Could not start FVID2 !!!\n");
                            break;
                        }
                    }

                    if (status == (vx_status)VX_SUCCESS)
                    {
                        prms->steady_state_started = 1;
                    }
                }
            }

            /* Pends until a frame is available then dequeue frames from capture driver */
            if ((vx_status)VX_SUCCESS == status)
            {
                tivx_obj_desc_t *tmp_desc[TIVX_CAPTURE_MAX_CH] = {NULL};

                uint32_t is_all_ch_frame_available = 0;

                for(chId = 0U ; chId < prms->numCh ; chId++)
                {
                    tmp_desc[chId] = NULL;
                }

                while(is_all_ch_frame_available == 0U)
                {
                    is_all_ch_frame_available = 1;
                    for(chId = 0U ; chId < prms->numCh ; chId++)
                    {
                        tivxQueuePeek(&prms->pendingFrameQ[chId], (uintptr_t*)&tmp_desc[chId]);
                        if(NULL==tmp_desc[chId])
                        {
                            is_all_ch_frame_available = 0;
                        }
                    }

                    if(is_all_ch_frame_available == 0U)
                    {
                        tivxEventWait(prms->frame_available, TIVX_EVENT_TIMEOUT_WAIT_FOREVER);

                        for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
                        {
                            instParams = &prms->instParams[instIdx];
                            fvid2_status = Fvid2_dequeue(instParams->drvHandle,
                                                         &frmList,
                                                         0,
                                                         FVID2_TIMEOUT_NONE);

                            if(FVID2_SOK == fvid2_status)
                            {
                                for(frmIdx=0; frmIdx < frmList.numFrames; frmIdx++)
                                {
                                    fvid2Frame = frmList.frames[frmIdx];
                                    chId = tivxCaptureGetNodeChannelNum(
                                                        prms,
                                                        instIdx,
                                                        fvid2Frame->chNum);
                                    desc = (tivx_obj_desc_object_array_t *)fvid2Frame->appData;

                                    tivxQueuePut(&prms->freeFvid2FrameQ[chId], (uintptr_t)fvid2Frame, TIVX_EVENT_TIMEOUT_NO_WAIT);
                                    tivxQueuePut(&prms->pendingFrameQ[chId], (uintptr_t)desc, TIVX_EVENT_TIMEOUT_NO_WAIT);
                                }
                            }
                            else if (fvid2_status == FVID2_ENO_MORE_BUFFERS)
                            {
                                /* continue: move onto next driver instance
                                  within node as current driver instance did
                                  not generate this CB */
                            }
                            else
                            {
                                /* TIOVX-687: Note: disabling for now until investigated further */
                                if (FVID2_EAGAIN != fvid2_status)
                                {
                                    status = (vx_status)VX_FAILURE;
                                    VX_PRINT(VX_ZONE_ERROR,
                                        " CAPTURE: ERROR: FVID2 Dequeue failed !!!\n");
                                }
                            }
                        }
                    }
                }

                for(chId = 0U ; chId < prms->numCh ; chId++)
                {
                    tivxQueueGet(&prms->pendingFrameQ[chId], (uintptr_t*)&tmp_desc[chId], TIVX_EVENT_TIMEOUT_NO_WAIT);
                }
                /* all values in tmp_desc[] should be same */
                obj_desc[TIVX_KERNEL_CAPTURE_OUTPUT_IDX] = (tivx_obj_desc_t *)tmp_desc[0];
            }
        }
        /* Pipe-up state: only receives a buffer; does not return a buffer */
        else
        {
            status = tivxCaptureEnqueueFrameToDriver(output_desc, prms);
        }
    }

    return status;
}

static vx_status VX_CALLBACK tivxCaptureCreate(
       tivx_target_kernel_instance kernel,
       tivx_obj_desc_t *obj_desc[],
       uint16_t num_params, void *priv_arg)
{
    vx_status status = (vx_status)VX_SUCCESS;
    int32_t fvid2_status = FVID2_SOK;
    tivx_obj_desc_user_data_object_t *input_obj_desc;
    tivx_obj_desc_object_array_t *output_desc;
    tivxCaptureParams *prms = NULL;
    uint32_t chId, bufId, instIdx;
    tivxCaptureInstParams *instParams;

    if ( (num_params != TIVX_KERNEL_CAPTURE_MAX_PARAMS)
        || (NULL == obj_desc[TIVX_KERNEL_CAPTURE_INPUT_ARR_IDX])
        || (NULL == obj_desc[TIVX_KERNEL_CAPTURE_OUTPUT_IDX])
    )
    {
        status = (vx_status)VX_FAILURE;
    }
    else
    {
        input_obj_desc = (tivx_obj_desc_user_data_object_t *)obj_desc[TIVX_KERNEL_CAPTURE_INPUT_ARR_IDX];
        output_desc = (tivx_obj_desc_object_array_t *)obj_desc[TIVX_KERNEL_CAPTURE_OUTPUT_IDX];

        prms = tivxMemAlloc(sizeof(tivxCaptureParams), (vx_enum)TIVX_MEM_EXTERNAL);

        if (NULL != prms)
        {
            memset(prms, 0, sizeof(tivxCaptureParams));
        }
        else
        {
            VX_PRINT(VX_ZONE_ERROR, " CAPTURE: ERROR: Could allocate memory !!!\n");
            status = (vx_status)VX_ERROR_NO_MEMORY;
        }

        if ((vx_status)VX_SUCCESS == status)
        {
            /* Initialize steady_state_started to 0 */
            prms->steady_state_started = 0;
            /* Initialize raw capture to 0 */
            for (instIdx = 0U ; instIdx < TIVX_CAPTURE_MAX_INST ; instIdx++)
            {
                prms->instParams[instIdx].raw_capture = 0;
            }

            /* Set number of channels to number of items in object array */
            prms->numCh = (uint8_t)output_desc->num_items;

            if (prms->numCh > TIVX_CAPTURE_MAX_CH)
            {
                status = (vx_status)VX_ERROR_INVALID_PARAMETERS;
                VX_PRINT(VX_ZONE_ERROR, "Object descriptor number of channels exceeds max value allowed by capture!!!\r\n");
            }
        }

        /* Setting CSIRX capture parameters */
        if ((vx_status)VX_SUCCESS == status)
        {
            tivxGetObjDescList(output_desc->obj_desc_id, (tivx_obj_desc_t **)prms->img_obj_desc,
                           prms->numCh);

            tivxCaptureSetCreateParams(prms, input_obj_desc);
        }

        /* Creating frame available event */
        if ((vx_status)VX_SUCCESS == status)
        {
            status = tivxEventCreate(&prms->frame_available);

            if ((vx_status)VX_SUCCESS != status)
            {
                VX_PRINT(VX_ZONE_ERROR, "Event creation failed in capture!!!\r\n");
            }
        }

        /* Creating FVID2 handle */
        if ((vx_status)VX_SUCCESS == status)
        {
            for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
            {
                instParams = &prms->instParams[instIdx];
                Fvid2CbParams_init(&instParams->drvCbPrms);

                instParams->drvCbPrms.cbFxn   = (Fvid2_CbFxn) &captDrvCallback;
                instParams->drvCbPrms.appData = prms;

                instParams->drvHandle = Fvid2_create(CSIRX_CAPT_DRV_ID,
                                                     instParams->instId,
                                                     &instParams->createPrms,
                                                     &instParams->createStatus,
                                                     &instParams->drvCbPrms);

                if ((NULL == instParams->drvHandle) ||
                    (instParams->createStatus.retVal != FVID2_SOK))
                {
                    VX_PRINT(VX_ZONE_ERROR, ": Capture Create Failed!!!\r\n");
                    status = (vx_status)VX_FAILURE;
                }
                else
                {
                    /* Set CSIRX D-PHY configuration parameters */
                    Csirx_initDPhyCfg(&instParams->dphyCfg);
                    instParams->dphyCfg.inst = instParams->instId;
                    fvid2_status = Fvid2_control(
                        instParams->drvHandle, IOCTL_CSIRX_SET_DPHY_CONFIG,
                        &instParams->dphyCfg, NULL);
                    if (FVID2_SOK != fvid2_status)
                    {
                        status = (vx_status)VX_FAILURE;
                        VX_PRINT(VX_ZONE_ERROR, ": Failed to set PHY Parameters!!!\r\n");
                    }
                }
            }
        }

        /* Creating FVID2 frame Q */
        if ((vx_status)VX_SUCCESS == status)
        {
            for(chId = 0u ; chId < prms->numCh ; chId++)
            {
                status = tivxQueueCreate(&prms->freeFvid2FrameQ[chId], TIVX_CAPTURE_MAX_NUM_BUFS, prms->fvid2_free_q_mem[chId], 0);

                if ((vx_status)VX_SUCCESS != status)
                {
                    VX_PRINT(VX_ZONE_ERROR, ": Capture queue create failed!!!\r\n");
                    break;
                }

                for(bufId = 0u ; bufId < (TIVX_CAPTURE_MAX_NUM_BUFS) ; bufId++)
                {
                    tivxQueuePut(&prms->freeFvid2FrameQ[chId], (uintptr_t)&prms->fvid2Frames[chId][bufId], TIVX_EVENT_TIMEOUT_NO_WAIT);
                }
            }
        }

        /* Creating pending frame Q */
        if ((vx_status)VX_SUCCESS == status)
        {
            for(chId = 0U ; chId < prms->numCh ; chId++)
            {
                status = tivxQueueCreate(&prms->pendingFrameQ[chId], TIVX_CAPTURE_MAX_NUM_BUFS, prms->pending_frame_free_q_mem[chId], 0);

                if ((vx_status)VX_SUCCESS != status)
                {
                    VX_PRINT(VX_ZONE_ERROR, ": Capture create failed!!!\r\n");
                    break;
                }
            }
        }

        if ((vx_status)VX_SUCCESS == status)
        {
            tivxSetTargetKernelInstanceContext(kernel, prms, sizeof(tivxCaptureParams));
        }
        else if (NULL != prms)
        {
            for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
            {
                instParams = &prms->instParams[instIdx];
                if (NULL != instParams->drvHandle)
                {
                    Fvid2_delete(instParams->drvHandle, NULL);
                    instParams->drvHandle = NULL;
                }
            }

            if (NULL != prms->frame_available)
            {
                tivxEventDelete(&prms->frame_available);
            }

            tivxMemFree(prms, sizeof(tivxCaptureParams), (vx_enum)TIVX_MEM_EXTERNAL);
        }
        else
        {
            /* do nothing */
        }
    }

    return status;
}

static void tivxCapturePrintStatus(tivxCaptureInstParams *prms)
{
    int32_t fvid2_status;
    uint32_t cnt;

    if (NULL != prms)
    {
        fvid2_status = Fvid2_control(prms->drvHandle,
                                IOCTL_CSIRX_GET_INST_STATUS,
                                &prms->captStatus,
                                NULL);
        tivx_set_debug_zone((vx_enum)VX_ZONE_INFO);
        if (FVID2_SOK == fvid2_status)
        {
            VX_PRINT(VX_ZONE_INFO,
                "\n\r==========================================================\r\n");
            VX_PRINT(VX_ZONE_INFO,
                      ": Capture Status: Instance|%d\r\n", prms->instId);
            VX_PRINT(VX_ZONE_INFO,
                      "==========================================================\r\n");
            VX_PRINT(VX_ZONE_INFO,
                      ": FIFO Overflow Count: %d\r\n",
                      prms->captStatus.overflowCount);
            VX_PRINT(VX_ZONE_INFO,
                      ": Spurious UDMA interrupt count: %d\r\n",
                      prms->captStatus.spuriousUdmaIntrCount);

            VX_PRINT(VX_ZONE_INFO,
                "  [Channel No] | Frame Queue Count |"
                " Frame De-queue Count | Frame Drop Count |\n");
            for(cnt = 0U ; cnt < prms->numCh ; cnt ++)
            {
                VX_PRINT(VX_ZONE_INFO,
                      "\t\t%d|\t\t%d|\t\t%d|\t\t%d|\n",
                      cnt,
                      prms->captStatus.queueCount[cnt],
                      prms->captStatus.dequeueCount[cnt],
                      prms->captStatus.dropCount[cnt]);
            }
        }
        else
        {
            VX_PRINT(VX_ZONE_ERROR, " CAPTURE: ERROR: FVID2 Control failed !!!\n");
        }
        tivx_clr_debug_zone((vx_enum)VX_ZONE_INFO);
    }
}

static vx_status VX_CALLBACK tivxCaptureDelete(
       tivx_target_kernel_instance kernel,
       tivx_obj_desc_t *obj_desc[],
       uint16_t num_params, void *priv_arg)
{
    vx_status status = (vx_status)VX_SUCCESS;
    int32_t fvid2_status = FVID2_SOK;
    tivxCaptureParams *prms = NULL;
    static Fvid2_FrameList frmList;
    uint32_t size, chId, instIdx;
    tivxCaptureInstParams *instParams;

    if ( (num_params != TIVX_KERNEL_CAPTURE_MAX_PARAMS)
        || (NULL == obj_desc[TIVX_KERNEL_CAPTURE_INPUT_ARR_IDX])
        || (NULL == obj_desc[TIVX_KERNEL_CAPTURE_OUTPUT_IDX])
    )
    {
        status = (vx_status)VX_FAILURE;
    }
    else
    {
        status = tivxGetTargetKernelInstanceContext(kernel, (void **)&prms, &size);

        if ((vx_status)VX_SUCCESS != status)
        {
            VX_PRINT(VX_ZONE_ERROR, " CAPTURE: ERROR: Could not obtain kernel instance context !!!\n");
        }

        if(NULL == prms)
        {
            VX_PRINT(VX_ZONE_ERROR, "Kernel instance context is NULL!!!\n");
            status = (vx_status)VX_FAILURE;
        }

        if ((vx_status)VX_SUCCESS == status)
        {
            for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
            {
                instParams = &prms->instParams[instIdx];
                /* Stopping FVID2 Capture */
                if ((vx_status)VX_SUCCESS == status)
                {
                    fvid2_status = Fvid2_stop(instParams->drvHandle, NULL);

                    if (FVID2_SOK != fvid2_status)
                    {
                        status = (vx_status)VX_FAILURE;
                        VX_PRINT(VX_ZONE_ERROR, " CAPTURE: ERROR: FVID2 Capture not stopped !!!\n");
                    }
                }

                /* Dequeue all the request from the driver */
                if ((vx_status)VX_SUCCESS == status)
                {
                    Fvid2FrameList_init(&frmList);
                    do
                    {
                        fvid2_status = Fvid2_dequeue(
                            instParams->drvHandle,
                            &frmList,
                            0,
                            FVID2_TIMEOUT_NONE);
                    } while (FVID2_SOK == fvid2_status);

                    if (FVID2_ENO_MORE_BUFFERS != fvid2_status)
                    {
                        VX_PRINT(VX_ZONE_ERROR, " CAPTURE: ERROR: FVID2 Capture Dequeue Failed !!!\n");
                        status = (vx_status)VX_FAILURE;
                    }
                }

                if ((vx_status)VX_SUCCESS == status)
                {
                    tivxCapturePrintStatus(instParams);
                }

                /* Deleting FVID2 handle */
                if ((vx_status)VX_SUCCESS == status)
                {
                    fvid2_status = Fvid2_delete(instParams->drvHandle, NULL);

                    if (FVID2_SOK != fvid2_status)
                    {
                        status = (vx_status)VX_FAILURE;
                        VX_PRINT(VX_ZONE_ERROR, " CAPTURE: ERROR: FVID2 Delete Failed !!!\n");
                    }
                }

                /* Free-ing kernel instance params */
                if ( (vx_status)VX_SUCCESS == status)
                {
                    instParams->drvHandle = NULL;

                    if (sizeof(tivxCaptureParams) == size)
                    {
                        tivxMemFree(prms, sizeof(tivxCaptureParams), (vx_status)TIVX_MEM_EXTERNAL);
                    }
                }
            }
        }

        /* Deleting FVID2 frame Q */
        if ((vx_status)VX_SUCCESS == status)
        {
            for(chId = 0U; chId < prms->numCh ; chId++)
            {
                tivxQueueDelete(&prms->freeFvid2FrameQ[chId]);
            }
        }

        /* Deleting pending frame Q */
        if ((vx_status)VX_SUCCESS == status)
        {
            for(chId= 0U ; chId < prms->numCh ; chId++)
            {
                tivxQueueDelete(&prms->pendingFrameQ[chId]);
            }
        }

        /* Deleting event */
        if ((vx_status)VX_SUCCESS == status)
        {
            tivxEventDelete(&prms->frame_available);
        }
    }

    return status;
}

static void tivxCaptureCopyStatistics(tivxCaptureParams *prms,
    tivx_capture_statistics_t *capt_status_prms)
{
    uint32_t i, instIdx, strmIdx;
    tivxCaptureInstParams *instParams;

    for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
    {
        instParams = &prms->instParams[instIdx];
        for (i = 0U ; i < instParams->numCh ; i++)
        {
            capt_status_prms->queueCount[instIdx][i]     = instParams->captStatus.queueCount[i];
            capt_status_prms->dequeueCount[instIdx][i]   = instParams->captStatus.dequeueCount[i];
            capt_status_prms->dropCount[instIdx][i]      = instParams->captStatus.dropCount[i];
        }
        capt_status_prms->overflowCount[instIdx]         = instParams->captStatus.overflowCount;
        capt_status_prms->spuriousUdmaIntrCount[instIdx] = instParams->captStatus.spuriousUdmaIntrCount;
        capt_status_prms->frontFIFOOvflCount[instIdx]    = instParams->captStatus.frontFIFOOvflCount;
        capt_status_prms->crcCount[instIdx]              = instParams->captStatus.crcCount;
        capt_status_prms->eccCount[instIdx]              = instParams->captStatus.eccCount;
        capt_status_prms->correctedEccCount[instIdx]     = instParams->captStatus.correctedEccCount;
        capt_status_prms->dataIdErrorCount[instIdx]      = instParams->captStatus.dataIdErrorCount;
        capt_status_prms->invalidAccessCount[instIdx]    = instParams->captStatus.invalidAccessCount;
        capt_status_prms->invalidSpCount[instIdx]        = instParams->captStatus.invalidSpCount;
        for (strmIdx = 0U ; strmIdx < TIVX_CAPTURE_MAX_STRM ; strmIdx++)
        {
            capt_status_prms->strmFIFOOvflCount[instIdx][strmIdx] =
                            instParams->captStatus.strmFIFOOvflCount[strmIdx];
        }
    }
}

static vx_status tivxCaptureGetStatistics(tivxCaptureParams *prms,
    tivx_obj_desc_user_data_object_t *usr_data_obj)
{
    vx_status                             status = (vx_status)VX_SUCCESS;
    tivx_capture_statistics_t                 *capt_status_prms = NULL;
    void                                  *target_ptr;

    if (NULL != usr_data_obj)
    {
        target_ptr = tivxMemShared2TargetPtr(&usr_data_obj->mem_ptr);

        tivxMemBufferMap(target_ptr, usr_data_obj->mem_size,
            (vx_enum)VX_MEMORY_TYPE_HOST, (vx_enum)VX_WRITE_ONLY);

        if (sizeof(tivx_capture_statistics_t) ==
                usr_data_obj->mem_size)
        {
            capt_status_prms = (tivx_capture_statistics_t *)target_ptr;

            tivxCaptureCopyStatistics(prms, capt_status_prms);
        }
        else
        {
            VX_PRINT(VX_ZONE_ERROR,
                "tivxCaptureGetStatistics: Invalid Size \n");
            status = (vx_status)VX_ERROR_INVALID_PARAMETERS;
        }

        tivxMemBufferUnmap(target_ptr, usr_data_obj->mem_size,
            (vx_enum)VX_MEMORY_TYPE_HOST, (vx_enum)VX_WRITE_ONLY);
    }
    else
    {
        VX_PRINT(VX_ZONE_ERROR,
            "tivxCaptureGetStatistics: User Data Object is NULL \n");
        status = (vx_status)VX_ERROR_INVALID_PARAMETERS;
    }

    return (status);
}

static vx_status VX_CALLBACK tivxCaptureControl(
       tivx_target_kernel_instance kernel,
       uint32_t node_cmd_id, tivx_obj_desc_t *obj_desc[],
       uint16_t num_params, void *priv_arg)
{
    vx_status status = (vx_status)VX_SUCCESS;
    int32_t fvid2_status = FVID2_SOK;
    uint32_t             size, instIdx;
    tivxCaptureParams *prms = NULL;
    tivxCaptureInstParams *instParams;

    status = tivxGetTargetKernelInstanceContext(kernel, (void **)&prms, &size);

    if ((vx_status)VX_SUCCESS != status)
    {
        VX_PRINT(VX_ZONE_ERROR,
            "tivxCaptureControl: Failed to Get Target Kernel Instance Context\n");
    }
    else if ((NULL == prms) ||
        (sizeof(tivxCaptureParams) != size))
    {
        VX_PRINT(VX_ZONE_ERROR,
            "tivxCaptureControl: Invalid Object Size\n");
        status = (vx_status)VX_FAILURE;
    }
    else
    {
        /* do nothing */
    }

    if ((vx_status)VX_SUCCESS == status)
    {
        switch (node_cmd_id)
        {
            case TIVX_CAPTURE_PRINT_STATISTICS:
            {
                for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
                {
                    instParams = &prms->instParams[instIdx];
                    tivxCapturePrintStatus(instParams);
                }
                break;
            }
            case TIVX_CAPTURE_GET_STATISTICS:
            {
                if (NULL != obj_desc[0])
                {
                    for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
                    {
                        instParams = &prms->instParams[instIdx];
                        fvid2_status = Fvid2_control(instParams->drvHandle,
                                                IOCTL_CSIRX_GET_INST_STATUS,
                                                &instParams->captStatus,
                                                NULL);
                        if (FVID2_SOK != fvid2_status)
                        {
                            VX_PRINT(VX_ZONE_ERROR,
                                "tivxCaptureControl: Get status returned failure\n");
                            status = (vx_status)VX_FAILURE;
                            break;
                        }
                    }
                    if ((vx_status)VX_SUCCESS == status)
                    {
                        status = tivxCaptureGetStatistics(prms,
                            (tivx_obj_desc_user_data_object_t *)obj_desc[0U]);
                        if ((vx_status)VX_SUCCESS != status)
                        {
                            VX_PRINT(VX_ZONE_ERROR,
                                "tivxCaptureControl: Get status failed\n");
                            status = (vx_status)VX_FAILURE;
                        }

                    }
                }
                else
                {
                    VX_PRINT(VX_ZONE_ERROR,
                        "tivxCaptureControl: User data object was NULL\n");
                    status = (vx_status)VX_FAILURE;
                }
                break;
            }
            default:
            {
                VX_PRINT(VX_ZONE_ERROR,
                    "tivxCaptureControl: Invalid Command Id\n");
                status = (vx_status)VX_FAILURE;
                break;
            }
        }
    }
    return status;
}

void tivxAddTargetKernelCapture(void)
{
    char target_name[TIVX_TARGET_MAX_NAME];
    vx_enum self_cpu;

    self_cpu = tivxGetSelfCpuId();

    if((self_cpu == (vx_enum)TIVX_CPU_ID_IPU1_0) || (self_cpu == (vx_enum)TIVX_CPU_ID_IPU1_1))
    {
        strncpy(target_name, TIVX_TARGET_CAPTURE1, TIVX_TARGET_MAX_NAME);

        vx_capture_target_kernel1 = tivxAddTargetKernelByName(
                            TIVX_KERNEL_CAPTURE_NAME,
                            target_name,
                            tivxCaptureProcess,
                            tivxCaptureCreate,
                            tivxCaptureDelete,
                            tivxCaptureControl,
                            NULL);

        strncpy(target_name, TIVX_TARGET_CAPTURE2, TIVX_TARGET_MAX_NAME);

        vx_capture_target_kernel2 = tivxAddTargetKernelByName(
                            TIVX_KERNEL_CAPTURE_NAME,
                            target_name,
                            tivxCaptureProcess,
                            tivxCaptureCreate,
                            tivxCaptureDelete,
                            tivxCaptureControl,
                            NULL);
    }
}

void tivxRemoveTargetKernelCapture(void)
{
    vx_status status = (vx_status)VX_SUCCESS;

    status = tivxRemoveTargetKernel(vx_capture_target_kernel1);
    if(status == (vx_status)VX_SUCCESS)
    {
        vx_capture_target_kernel1 = NULL;
    }
    status = tivxRemoveTargetKernel(vx_capture_target_kernel2);
    if(status == (vx_status)VX_SUCCESS)
    {
        vx_capture_target_kernel2 = NULL;
    }
}

static void tivxCaptureGetChannelIndices(const tivxCaptureParams *prms,
                                         uint32_t instId,
                                         uint32_t *startChIdx,
                                         uint32_t *endChIdx)
{
    uint32_t instIdx;

    *startChIdx = 0U;
    *endChIdx   = 0U;
    for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
    {
        /* get start channel ID here */
        if (instIdx == instId)
        {
            break;
        }
        else
        {
            *startChIdx += prms->instParams[instIdx].numCh;
        }
    }
    /* Get last channel ID here */
    if (instIdx < prms->numOfInstUsed)
    {
        *endChIdx = *startChIdx + prms->instParams[instIdx].numCh;
    }
}

static uint32_t tivxCaptureGetNodeChannelNum(const tivxCaptureParams *prms,
                                             uint32_t instId,
                                             uint32_t chId)
{
    uint32_t instIdx, chIdx = 0U;

    /* Get addition of all the channels processed on all previous driver instances */
    for (instIdx = 0U ; instIdx < prms->numOfInstUsed ; instIdx++)
    {
        if (instIdx == instId)
        {
            break;
        }
        else
        {
            chIdx += prms->instParams[instIdx].numCh;
        }
    }
    chIdx += chId;

    return (chIdx);
}
