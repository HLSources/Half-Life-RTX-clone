#pragma once

#include "vk_core.h"
#include "vk_rtx.h" // vk_buffer_region_t

qboolean XVK_DenoiserInit( void );
void XVK_DenoiserDestroy( void );

void XVK_DenoiserReloadPipeline( void );

typedef struct {
	VkCommandBuffer cmdbuf;
	uint32_t width, height;

	struct {
		vk_buffer_region_t primary_ray;
		VkImageView diffuse_gi_view;
		VkImageView specular_view;
		VkImageView additive_view;
		VkImageView normals_view;
		VkImageView position_t_view;
	} src;

	VkImageView dst_view;
} xvk_denoiser_args_t;

void XVK_DenoiserDenoise( const xvk_denoiser_args_t* args );
