/*
*
* Copyright (c) 2017-2019 Texas Instruments Incorporated
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



#include <TI/tivx.h>
#include <VX/vx.h>
#include <tivx_openvx_core_kernels.h>
#include <tivx_kernel_convert_depth.h>
#include <TI/tivx_target_kernel.h>
#include <ti/vxlib/vxlib.h>
#include <tivx_kernels_target_utils.h>
#include <tivx_bam_kernel_wrapper.h>
#include "tivx_target_kernels_priv.h"

typedef struct
{
    tivx_bam_graph_handle graph_handle;
    uint8_t bam_node_num;
} tivxConvertDepthParams;

static tivx_target_kernel vx_convert_depth_target_kernel = NULL;

static vx_status VX_CALLBACK tivxKernelBamConvertDepthProcess(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, void *priv_arg);

static vx_status VX_CALLBACK tivxKernelBamConvertDepthCreate(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, void *priv_arg);

static vx_status VX_CALLBACK tivxKernelBamConvertDepthDelete(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, void *priv_arg);

/* Supernode Callbacks */
static vx_status VX_CALLBACK tivxKernelConvertDepthCreateInBamGraph(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, void *priv_arg, BAM_NodeParams node_list[],
    tivx_bam_kernel_details_t kernel_details[],
    int32_t * bam_node_cnt, void * scratch, int32_t *size);

static vx_status VX_CALLBACK tivxKernelConvertDepthGetNodePort(
    tivx_target_kernel_instance kernel, uint8_t ovx_port, uint8_t plane,
    uint8_t *bam_node, uint8_t *bam_port);

static vx_status VX_CALLBACK tivxKernelConvertDepthPreprocessInBamGraph(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, tivx_bam_graph_handle *g_handle, void *priv_arg);


static vx_status VX_CALLBACK tivxKernelBamConvertDepthProcess(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, void *priv_arg)
{
    vx_status status = (vx_status)VX_SUCCESS;
    tivxConvertDepthParams *prms = NULL;
    tivx_obj_desc_image_t *src, *dst;
    uint8_t *src_addr, *dst_addr;
    uint32_t size;
    tivx_obj_desc_scalar_t *sc_desc[2];
    BAM_VXLIB_convertDepth_i16s_o8u_params prms_1;
    BAM_VXLIB_convertDepth_i8u_o16s_params prms_2;

    status = tivxCheckNullParams(obj_desc, num_params,
            TIVX_KERNEL_CONVERT_DEPTH_MAX_PARAMS);

    if ((vx_status)VX_SUCCESS == status)
    {
        src = (tivx_obj_desc_image_t *)obj_desc[TIVX_KERNEL_CONVERT_DEPTH_INPUT_IDX];
        dst = (tivx_obj_desc_image_t *)obj_desc[TIVX_KERNEL_CONVERT_DEPTH_OUTPUT_IDX];
        sc_desc[0] = (tivx_obj_desc_scalar_t *)
            obj_desc[TIVX_KERNEL_CONVERT_DEPTH_POLICY_IDX];
        sc_desc[1] = (tivx_obj_desc_scalar_t *)
            obj_desc[TIVX_KERNEL_CONVERT_DEPTH_SHIFT_IDX];

        status = tivxGetTargetKernelInstanceContext(kernel,
            (void **)&prms, &size);

        if (((vx_status)VX_SUCCESS != status) || (NULL == prms) ||
            (sizeof(tivxConvertDepthParams) != size))
        {
            status = (vx_status)VX_FAILURE;
        }
    }

    if ((vx_status)VX_SUCCESS == status)
    {
        void *img_ptrs[2];
        void *src_target_ptr;
        void *dst_target_ptr;

        src_target_ptr = tivxMemShared2TargetPtr(&src->mem_ptr[0]);
        tivxSetPointerLocation(src, &src_target_ptr, &src_addr);

        dst_target_ptr = tivxMemShared2TargetPtr(&dst->mem_ptr[0]);
        tivxSetPointerLocation(dst, &dst_target_ptr, &dst_addr);

        if ((vx_df_image)VX_DF_IMAGE_S16 == dst->format)
        {
            prms_2.shift = sc_desc[1]->data.s32;

            status = tivxBamControlNode(prms->graph_handle, 0,
                BAM_VXLIB_CONVERTDEPTH_I8U_O16S_SET_PARAMS, &prms_2);
        }
        else
        {
            prms_1.shift = sc_desc[1]->data.s32;
            if ((vx_enum)VX_CONVERT_POLICY_SATURATE == sc_desc[0]->data.enm)
            {
                prms_1.overflow_policy = VXLIB_CONVERT_POLICY_SATURATE;
            }
            else
            {
                prms_1.overflow_policy = VXLIB_CONVERT_POLICY_WRAP;
            }

            status = tivxBamControlNode(prms->graph_handle, 0,
                BAM_VXLIB_CONVERTDEPTH_I16S_O8U_SET_PARAMS, &prms_1);
        }

        if ((vx_status)VX_SUCCESS == status)
        {
            img_ptrs[0] = src_addr;
            img_ptrs[1] = dst_addr;
            tivxBamUpdatePointers(prms->graph_handle, 1U, 1U, img_ptrs);

            status  = tivxBamProcessGraph(prms->graph_handle);
        }
    }
    else
    {
        status = (vx_status)VX_ERROR_NO_MEMORY;
    }

    return (status);
}

static vx_status VX_CALLBACK tivxKernelBamConvertDepthCreate(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, void *priv_arg)
{
    vx_status status = (vx_status)VX_SUCCESS;
    tivx_obj_desc_image_t *src, *dst;
    tivxConvertDepthParams *prms = NULL;
    tivx_bam_kernel_details_t kernel_details;
    tivx_obj_desc_scalar_t *sc_desc[2];

    /* Check number of buffers and NULL pointers */
    status = tivxCheckNullParams(obj_desc, num_params,
            TIVX_KERNEL_CONVERT_DEPTH_MAX_PARAMS);

    if ((vx_status)VX_SUCCESS == status)
    {
        status = tivxBamInitKernelDetails(&kernel_details, 1, kernel);
    }

    if ((vx_status)VX_SUCCESS == status)
    {
        src = (tivx_obj_desc_image_t *)obj_desc[
            TIVX_KERNEL_CONVERT_DEPTH_INPUT_IDX];
        dst = (tivx_obj_desc_image_t *)obj_desc[
            TIVX_KERNEL_CONVERT_DEPTH_OUTPUT_IDX];
        sc_desc[0] = (tivx_obj_desc_scalar_t *)
            obj_desc[TIVX_KERNEL_CONVERT_DEPTH_POLICY_IDX];
        sc_desc[1] = (tivx_obj_desc_scalar_t *)
            obj_desc[TIVX_KERNEL_CONVERT_DEPTH_SHIFT_IDX];

        prms = tivxMemAlloc(sizeof(tivxConvertDepthParams), (vx_enum)TIVX_MEM_EXTERNAL);

        if (NULL != prms)
        {
            VXLIB_bufParams2D_t vxlib_src, vxlib_dst;
            VXLIB_bufParams2D_t *buf_params[2];

            memset(prms, 0, sizeof(tivxConvertDepthParams));

            tivxInitBufParams(src, &vxlib_src);
            tivxInitBufParams(dst, &vxlib_dst);

            /* Fill in the frame level sizes of buffers here. If the port
             * is optionally disabled, put NULL */
            buf_params[0] = &vxlib_src;
            buf_params[1] = &vxlib_dst;

            if ((uint32_t)VXLIB_INT16 == vxlib_dst.data_type)
            {
                BAM_VXLIB_convertDepth_i8u_o16s_params params;
                params.shift = sc_desc[1]->data.s32;

                kernel_details.compute_kernel_params = (void *)&params;

                BAM_VXLIB_convertDepth_i8u_o16s_getKernelInfo(NULL,
                    &kernel_details.kernel_info);

                status = tivxBamCreateHandleSingleNode(
                    BAM_KERNELID_VXLIB_CONVERTDEPTH_I8U_O16S, buf_params,
                    &kernel_details, &prms->graph_handle);
            }
            else
            {
                BAM_VXLIB_convertDepth_i16s_o8u_params params;
                params.shift = sc_desc[1]->data.s32;

                if ((vx_enum)VX_CONVERT_POLICY_SATURATE == sc_desc[0]->data.enm)
                {
                    params.overflow_policy = VXLIB_CONVERT_POLICY_SATURATE;
                }
                else
                {
                    params.overflow_policy = VXLIB_CONVERT_POLICY_WRAP;
                }

                kernel_details.compute_kernel_params = (void *)&params;

                BAM_VXLIB_convertDepth_i16s_o8u_getKernelInfo(NULL,
                    &kernel_details.kernel_info);

                status = tivxBamCreateHandleSingleNode(
                    BAM_KERNELID_VXLIB_CONVERTDEPTH_I16S_O8U, buf_params,
                    &kernel_details, &prms->graph_handle);
            }
        }
        else
        {
            status = (vx_status)VX_ERROR_NO_MEMORY;
        }

        if ((vx_status)VX_SUCCESS == status)
        {
            tivxSetTargetKernelInstanceContext(kernel, prms,
                sizeof(tivxConvertDepthParams));
        }
        else
        {
            if (NULL != prms)
            {
                tivxMemFree(prms, sizeof(tivxConvertDepthParams), (vx_enum)TIVX_MEM_EXTERNAL);
            }
        }
    }

    return status;
}

static vx_status VX_CALLBACK tivxKernelBamConvertDepthDelete(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, void *priv_arg)
{
    vx_status status = (vx_status)VX_SUCCESS;
    uint32_t size;
    tivxConvertDepthParams *prms = NULL;

    /* Check number of buffers and NULL pointers */
    status = tivxCheckNullParams(obj_desc, num_params,
            TIVX_KERNEL_CONVERT_DEPTH_MAX_PARAMS);

    if ((vx_status)VX_SUCCESS == status)
    {
        status = tivxGetTargetKernelInstanceContext(kernel,
            (void **)&prms, &size);

        if (((vx_status)VX_SUCCESS == status) && (NULL != prms) &&
            (sizeof(tivxConvertDepthParams) == size))
        {
            if(NULL != prms->graph_handle)
            {
                tivxBamDestroyHandle(prms->graph_handle);
            }
            tivxMemFree(prms, sizeof(tivxConvertDepthParams), (vx_enum)TIVX_MEM_EXTERNAL);
        }
    }

    return (status);
}

void tivxAddTargetKernelBamConvertDepth(void)
{
    char target_name[TIVX_TARGET_MAX_NAME];
    vx_enum self_cpu;

    self_cpu = tivxGetSelfCpuId();

    if ((self_cpu == (vx_enum)TIVX_CPU_ID_DSP1) || (self_cpu == (vx_enum)TIVX_CPU_ID_DSP2))
    {
        if (self_cpu == (vx_enum)TIVX_CPU_ID_DSP1)
        {
            strncpy(target_name, TIVX_TARGET_DSP1,
                TIVX_TARGET_MAX_NAME);
        }
        else
        {
            strncpy(target_name, TIVX_TARGET_DSP2,
                TIVX_TARGET_MAX_NAME);
        }

        vx_convert_depth_target_kernel = tivxAddTargetKernel(
            (vx_enum)VX_KERNEL_CONVERTDEPTH,
            target_name,
            tivxKernelBamConvertDepthProcess,
            tivxKernelBamConvertDepthCreate,
            tivxKernelBamConvertDepthDelete,
            NULL,
            NULL);

        tivxEnableKernelForSuperNode(vx_convert_depth_target_kernel,
            tivxKernelConvertDepthCreateInBamGraph,
            tivxKernelConvertDepthGetNodePort,
            NULL,
            tivxKernelConvertDepthPreprocessInBamGraph,
            NULL,
            MAX2((int32_t)sizeof(BAM_VXLIB_convertDepth_i8u_o16s_params),
                 (int32_t)sizeof(BAM_VXLIB_convertDepth_i16s_o8u_params)),
            NULL);
    }
}


void tivxRemoveTargetKernelBamConvertDepth(void)
{
    tivxRemoveTargetKernel(vx_convert_depth_target_kernel);
}

static vx_status VX_CALLBACK tivxKernelConvertDepthCreateInBamGraph(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, void *priv_arg, BAM_NodeParams node_list[],
    tivx_bam_kernel_details_t kernel_details[],
    int32_t * bam_node_cnt, void * scratch, int32_t *size)
{

    vx_status status = (vx_status)VX_SUCCESS;
    tivx_obj_desc_image_t *dst;
    tivxConvertDepthParams *prms = NULL;
    tivx_obj_desc_scalar_t *sc_desc[2];

    /* Check number of buffers and NULL pointers */
    status = tivxCheckNullParams(obj_desc, num_params,
            TIVX_KERNEL_CONVERT_DEPTH_MAX_PARAMS);

    if ((vx_status)VX_SUCCESS == status)
    {
        dst = (tivx_obj_desc_image_t *)obj_desc[
            TIVX_KERNEL_CONVERT_DEPTH_OUTPUT_IDX];
        sc_desc[0] = (tivx_obj_desc_scalar_t *)
            obj_desc[TIVX_KERNEL_CONVERT_DEPTH_POLICY_IDX];
        sc_desc[1] = (tivx_obj_desc_scalar_t *)
            obj_desc[TIVX_KERNEL_CONVERT_DEPTH_SHIFT_IDX];

        prms = tivxMemAlloc(sizeof(tivxConvertDepthParams), (vx_enum)TIVX_MEM_EXTERNAL);

        if (NULL != prms)
        {
            memset(prms, 0, sizeof(tivxConvertDepthParams));

            node_list[*bam_node_cnt].nodeIndex = (uint8_t)*bam_node_cnt;
            node_list[*bam_node_cnt].kernelArgs = NULL;

            if (dst->format == (vx_df_image)VX_DF_IMAGE_S16)
            {
                BAM_VXLIB_convertDepth_i8u_o16s_params *kernel_params = (BAM_VXLIB_convertDepth_i8u_o16s_params*)scratch;

                if ((NULL != kernel_params) &&
                    (*size >= (int32_t)sizeof(BAM_VXLIB_convertDepth_i8u_o16s_params)))
                {
                    kernel_params->shift = sc_desc[1]->data.s32;

                    node_list[*bam_node_cnt].kernelId = (uint32_t)BAM_KERNELID_VXLIB_CONVERTDEPTH_I8U_O16S;

                    kernel_details[*bam_node_cnt].compute_kernel_params = (void *)kernel_params;

                    BAM_VXLIB_convertDepth_i8u_o16s_getKernelInfo( NULL,
                                                                     &kernel_details[*bam_node_cnt].kernel_info);
                }
                else
                {
                    status = (vx_status)VX_FAILURE;
                }
            }
            else
            {
                BAM_VXLIB_convertDepth_i16s_o8u_params *kernel_params = (BAM_VXLIB_convertDepth_i16s_o8u_params*)scratch;

                if ((NULL != kernel_params) &&
                    (*size >= (int32_t)sizeof(BAM_VXLIB_convertDepth_i16s_o8u_params)))
                {

                    kernel_params->shift = sc_desc[1]->data.s32;

                    node_list[*bam_node_cnt].kernelId = (uint32_t)BAM_KERNELID_VXLIB_CONVERTDEPTH_I16S_O8U;

                    if ((vx_enum)VX_CONVERT_POLICY_SATURATE == sc_desc[0]->data.enm)
                    {
                        kernel_params->overflow_policy = VXLIB_CONVERT_POLICY_SATURATE;
                    }
                    else
                    {
                        kernel_params->overflow_policy = VXLIB_CONVERT_POLICY_WRAP;
                    }

                    kernel_details[*bam_node_cnt].compute_kernel_params = (void *)kernel_params;

                    BAM_VXLIB_convertDepth_i16s_o8u_getKernelInfo( NULL,
                                                                 &kernel_details[*bam_node_cnt].kernel_info);
                }
                else
                {
                    status = (vx_status)VX_FAILURE;
                }
            }

            prms->bam_node_num = (uint8_t)*bam_node_cnt;
        }
        else
        {
            status = (vx_status)VX_ERROR_NO_MEMORY;
        }

        if ((vx_status)VX_SUCCESS == status)
        {
            tivxSetTargetKernelInstanceContext(kernel, prms,
                sizeof(tivxConvertDepthParams));
        }
        else
        {
            if (NULL != prms)
            {
                tivxMemFree(prms, sizeof(tivxConvertDepthParams), (vx_enum)TIVX_MEM_EXTERNAL);
            }
        }
    }

    return status;
}

static vx_status VX_CALLBACK tivxKernelConvertDepthGetNodePort(
    tivx_target_kernel_instance kernel,
    uint8_t ovx_port, uint8_t plane, uint8_t *bam_node, uint8_t *bam_port)
{
    tivxConvertDepthParams *prms = NULL;
    uint32_t size;

    vx_status status = tivxGetTargetKernelInstanceContext(kernel,
                        (void **)&prms, &size);

    if (((vx_status)VX_SUCCESS == status) && (NULL != prms) &&
        (sizeof(tivxConvertDepthParams) == size))
    {
        switch (ovx_port)
        {
            case TIVX_KERNEL_CONVERT_DEPTH_INPUT_IDX:
                *bam_node = prms->bam_node_num;
                *bam_port = (uint8_t)BAM_VXLIB_CONVERTDEPTH_I8U_O16S_INPUT_IMAGE_PORT;
                break;
            case TIVX_KERNEL_CONVERT_DEPTH_OUTPUT_IDX:
                *bam_node = prms->bam_node_num;
                *bam_port = (uint8_t)BAM_VXLIB_CONVERTDEPTH_I8U_O16S_OUTPUT_PORT;
                break;
            default:
                VX_PRINT(VX_ZONE_ERROR,"tivxKernelConvertDepthGetNodePort: non existing index queried by tivxKernelSupernodeCreate.tivxGetNodePort()\n");
                status = (vx_status)VX_FAILURE;
                break;
        }
    }

    return status;
}

static vx_status VX_CALLBACK tivxKernelConvertDepthPreprocessInBamGraph(
    tivx_target_kernel_instance kernel, tivx_obj_desc_t *obj_desc[],
    uint16_t num_params, tivx_bam_graph_handle *g_handle, void *priv_arg)
{
    vx_status status = (vx_status)VX_SUCCESS;
    tivxConvertDepthParams *prms = NULL;
    tivx_obj_desc_image_t *dst;
    uint32_t size;
    tivx_obj_desc_scalar_t *sc_desc[2];
    BAM_VXLIB_convertDepth_i16s_o8u_params prms_1;
    BAM_VXLIB_convertDepth_i8u_o16s_params prms_2;

    memset(&prms_1, 0, sizeof(BAM_VXLIB_convertDepth_i16s_o8u_params));

    dst = (tivx_obj_desc_image_t *)obj_desc[TIVX_KERNEL_CONVERT_DEPTH_OUTPUT_IDX];
    sc_desc[0] = (tivx_obj_desc_scalar_t *)
        obj_desc[TIVX_KERNEL_CONVERT_DEPTH_POLICY_IDX];
    sc_desc[1] = (tivx_obj_desc_scalar_t *)
        obj_desc[TIVX_KERNEL_CONVERT_DEPTH_SHIFT_IDX];

    status = tivxGetTargetKernelInstanceContext(kernel,
        (void **)&prms, &size);

    if (((vx_status)VX_SUCCESS != status) || (NULL == prms) || (NULL == g_handle) ||
        (sizeof(tivxConvertDepthParams) != size))
    {
        status = (vx_status)VX_FAILURE;
    }

    if ((vx_status)VX_SUCCESS == status)
    {
        if ((vx_df_image)VX_DF_IMAGE_S16 == dst->format)
        {
            prms_2.shift = sc_desc[1]->data.s32;

            status = tivxBamControlNode(*g_handle, prms->bam_node_num,
                BAM_VXLIB_CONVERTDEPTH_I8U_O16S_SET_PARAMS, &prms_2);
        }
        else
        {
            prms_1.shift = sc_desc[1]->data.s32;
            if ((vx_enum)VX_CONVERT_POLICY_SATURATE == sc_desc[0]->data.enm)
            {
                prms_1.overflow_policy = VXLIB_CONVERT_POLICY_SATURATE;
            }
            else
            {
                prms_1.overflow_policy = VXLIB_CONVERT_POLICY_WRAP;
            }

            status = tivxBamControlNode(*g_handle, prms->bam_node_num,
                BAM_VXLIB_CONVERTDEPTH_I16S_O8U_SET_PARAMS, &prms_1);
        }
    }
    else
    {
        status = (vx_status)VX_ERROR_NO_MEMORY;
    }

    return status;
}
