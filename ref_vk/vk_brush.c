#include "vk_brush.h"

#include "vk_core.h"
#include "vk_const.h"
#include "vk_buffer.h"
#include "vk_pipeline.h"
#include "vk_framectl.h"
#include "vk_math.h"
#include "vk_textures.h"
#include "vk_lightmap.h"
#include "vk_scene.h"

#include "ref_params.h"
#include "eiface.h"

#include <math.h>
#include <memory.h>

// TODO count these properly
#define MAX_BUFFER_VERTICES (1 * 1024 * 1024)
#define MAX_BUFFER_INDICES (MAX_BUFFER_VERTICES * 3)

typedef struct brush_vertex_s
{
	vec3_t pos;
	vec2_t gl_tc;
	vec2_t lm_tc;
} brush_vertex_t;

typedef struct vk_brush_model_surface_s {
	int texture_num;

	// Offset into gmap.index_buffer in vertices
	uint32_t index_offset;
	uint16_t index_count;
} vk_brush_model_surface_t;

typedef struct vk_brush_model_s {
	//model_t *model;

	// Offset into gmap.vertex_buffer in vertices
	uint32_t vertex_offset;

	int num_surfaces;
	vk_brush_model_surface_t surfaces[];
} vk_brush_model_t;

static struct {
	// TODO merge these into a single buffer
	vk_buffer_t vertex_buffer;
	uint32_t num_vertices;

	vk_buffer_t index_buffer;
	uint32_t num_indices;

	vk_buffer_t uniform_buffer;

	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
} gmap;

typedef struct {
	matrix4x4 mvp[MAX_SCENE_ENTITIES + 1];
} uniform_data_t;

/* static brush_vertex_t *allocVertices(int num_vertices) { */
/* 		if (num_vertices + gmap.num_vertices > MAX_BUFFER_VERTICES) */
/* 		{ */
/* 			gEngine.Con_Printf(S_ERROR "Ran out of buffer vertex space\n"); */
/* 			return NULL; */
/* 		} */
/* } */

static qboolean createPipelines( void )
{
	/* VkPushConstantRange push_const = { */
	/* 	.offset = 0, */
	/* 	.size = sizeof(AVec3f), */
	/* 	.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, */
	/* }; */

	VkDescriptorSetLayout descriptor_layouts[] = {
		vk_core.descriptor_pool.one_uniform_buffer_layout,
		vk_core.descriptor_pool.one_texture_layout,
		vk_core.descriptor_pool.one_texture_layout,
	};

	VkPipelineLayoutCreateInfo plci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = ARRAYSIZE(descriptor_layouts),
		.pSetLayouts = descriptor_layouts,
		/* .pushConstantRangeCount = 1, */
		/* .pPushConstantRanges = &push_const, */
	};

	// FIXME store layout separately
	XVK_CHECK(vkCreatePipelineLayout(vk_core.device, &plci, NULL, &gmap.pipeline_layout));

	{
		VkVertexInputAttributeDescription attribs[] = {
			{.binding = 0, .location = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(brush_vertex_t, pos)},
			{.binding = 0, .location = 1, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(brush_vertex_t, gl_tc)},
			{.binding = 0, .location = 2, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(brush_vertex_t, lm_tc)},
		};

		VkPipelineShaderStageCreateInfo shader_stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = loadShader("map.vert.spv"),
			.pName = "main",
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = loadShader("map.frag.spv"),
			.pName = "main",
		}};

		vk_pipeline_create_info_t ci = {
			.layout = gmap.pipeline_layout,
			.attribs = attribs,
			.num_attribs = ARRAYSIZE(attribs),

			.stages = shader_stages,
			.num_stages = ARRAYSIZE(shader_stages),

			.vertex_stride = sizeof(brush_vertex_t),

			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,

			.blendEnable = VK_FALSE,
		};

		gmap.pipeline = createPipeline(&ci);
		if (!gmap.pipeline)
			return false;

		for (int i = 0; i < (int)ARRAYSIZE(shader_stages); ++i)
			vkDestroyShaderModule(vk_core.device, shader_stages[i].module, NULL);
	}

	return true;
}

qboolean VK_BrushInit( void )
{
	const uint32_t vertex_buffer_size = MAX_BUFFER_VERTICES * sizeof(brush_vertex_t);
	const uint32_t index_buffer_size = MAX_BUFFER_INDICES * sizeof(uint16_t);

	// TODO device memory and friends (e.g. handle mobile memory ...)

	if (!createBuffer(&gmap.vertex_buffer, vertex_buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		return false;

	if (!createBuffer(&gmap.index_buffer, index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		return false;

	if (!createBuffer(&gmap.uniform_buffer, sizeof(uniform_data_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		return false;

	if (!createPipelines())
		return false;

	{
		VkDescriptorBufferInfo dbi = {
			.buffer = gmap.uniform_buffer.buffer,
			.offset = 0,
			.range = sizeof(matrix4x4),//VK_WHOLE_SIZE,
		};
		VkWriteDescriptorSet wds[] = { {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.pBufferInfo = &dbi,
			.dstSet = vk_core.descriptor_pool.ubo_sets[0], // FIXME
		}};
		vkUpdateDescriptorSets(vk_core.device, ARRAYSIZE(wds), wds, 0, NULL);
	}

	return true;
}

void VK_BrushShutdown( void )
{
	vkDestroyPipeline( vk_core.device, gmap.pipeline, NULL );
	vkDestroyPipelineLayout( vk_core.device, gmap.pipeline_layout, NULL );

	destroyBuffer( &gmap.vertex_buffer );
	destroyBuffer( &gmap.index_buffer );
	destroyBuffer( &gmap.uniform_buffer );
}

static float R_GetFarClip( void )
{
	/* FIXME
	if( WORLDMODEL && RI.drawWorld )
		return MOVEVARS->zmax * 1.73f;
	*/
	return 2048.0f;
}

#define max( a, b )                 (((a) > (b)) ? (a) : (b))
#define min( a, b )                 (((a) < (b)) ? (a) : (b))

static void R_SetupProjectionMatrix( const ref_viewpass_t *rvp, matrix4x4 m )
{
	float xMin, xMax, yMin, yMax, zNear, zFar;

	/*
	if( RI.drawOrtho )
	{
		const ref_overview_t *ov = gEngfuncs.GetOverviewParms();
		Matrix4x4_CreateOrtho( m, ov->xLeft, ov->xRight, ov->yTop, ov->yBottom, ov->zNear, ov->zFar );
		return;
	}
	*/

	const float farClip = R_GetFarClip();

	zNear = 4.0f;
	zFar = max( 256.0f, farClip );

	yMax = zNear * tan( rvp->fov_y * M_PI_F / 360.0f );
	yMin = -yMax;

	xMax = zNear * tan( rvp->fov_x * M_PI_F / 360.0f );
	xMin = -xMax;

	Matrix4x4_CreateProjection( m, xMax, xMin, yMax, yMin, zNear, zFar );
}

static void R_SetupModelviewMatrix( const ref_viewpass_t* rvp, matrix4x4 m )
{
	Matrix4x4_CreateModelview( m );
	Matrix4x4_ConcatRotate( m, -rvp->viewangles[2], 1, 0, 0 );
	Matrix4x4_ConcatRotate( m, -rvp->viewangles[0], 0, 1, 0 );
	Matrix4x4_ConcatRotate( m, -rvp->viewangles[1], 0, 0, 1 );
	Matrix4x4_ConcatTranslate( m, -rvp->vieworigin[0], -rvp->vieworigin[1], -rvp->vieworigin[2] );
}

static void drawBrushModel( const model_t *mod )
{
	// Expect all buffers to be bound
	const vk_brush_model_t *bmodel = mod->cache.data;
	int current_texture = -1;
	int index_count = 0;
	int index_offset = -1;

	if (!bmodel) {
		gEngine.Con_Printf( S_ERROR "Model %s wasn't loaded\n", mod->name);
		return;
	}

	if (vk_core.debug) {
		VkDebugUtilsLabelEXT label = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pLabelName = mod->name,
		};
		vkCmdBeginDebugUtilsLabelEXT(vk_core.cb, &label);
	}

	for (int i = 0; i < bmodel->num_surfaces; ++i) {
		const vk_brush_model_surface_t *bsurf = bmodel->surfaces + i;
		if (bsurf->texture_num < 0)
			continue;

		if (current_texture != bsurf->texture_num)
		{
			vk_texture_t *texture = findTexture(bsurf->texture_num);
			if (index_count)
				vkCmdDrawIndexed(vk_core.cb, index_count, 1, index_offset, bmodel->vertex_offset, 0);

			vkCmdBindDescriptorSets(vk_core.cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gmap.pipeline_layout, 1, 1, &texture->vk.descriptor, 0, NULL);

			current_texture = bsurf->texture_num;
			index_count = 0;
			index_offset = -1;
		}

		if (index_offset < 0)
			index_offset = bsurf->index_offset;
		// Make sure that all surfaces are concatenated in buffers
		ASSERT(index_offset + index_count == bsurf->index_offset);
		index_count += bsurf->index_count;
	}

	if (index_count)
		vkCmdDrawIndexed(vk_core.cb, index_count, 1, index_offset, bmodel->vertex_offset, 0);

	if (vk_core.debug)
		vkCmdEndDebugUtilsLabelEXT(vk_core.cb);
}

void R_RotateForEntity( matrix4x4 out, const cl_entity_t *e )
{
	float	scale = 1.0f;

	if( e == gEngine.GetEntityByIndex( 0 ) )
	{
		Matrix4x4_LoadIdentity(out);
		return;
	}

	if( e->model->type != mod_brush && e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	Matrix4x4_CreateFromEntity( out, e->angles, e->origin, scale );
}

void VK_BrushRender( const ref_viewpass_t *rvp, const draw_list_t *draw_list)
{
	matrix4x4 worldview={0}, projection={0}, mvp={0};
	uniform_data_t *ubo = gmap.uniform_buffer.mapped;

	{
		matrix4x4 tmp;
		// Vulkan has Y pointing down, and z should end up in (0, 1)
		const matrix4x4 vk_proj_fixup = {
			{1, 0, 0, 0},
			{0, -1, 0, 0},
			{0, 0, .5, 0},
			{0, 0, .5, 1}
		};
		R_SetupProjectionMatrix( rvp, tmp );
		Matrix4x4_Concat( projection, vk_proj_fixup, tmp );
	}

	R_SetupModelviewMatrix( rvp, worldview );
	Matrix4x4_Concat( mvp, projection, worldview);
	Matrix4x4_ToArrayFloatGL( mvp, (float*)ubo->mvp+0 );

		/*
		vkCmdUpdateBuffer(vk_core.cb, gmap.uniform_buffer.buffer, 0, sizeof(uniform_data), &uniform_data);

		VkMemoryBarrier mem_barrier = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		};
		vkCmdPipelineBarrier(vk_core.cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
				VK_DEPENDENCY_DEVICE_GROUP_BIT, 1, &mem_barrier, 0, NULL, 0, NULL);
		*/

	/* TODO
	if( RP_NORMALPASS( ))
	{
		int	x, x2, y, y2;

		// set up viewport (main, playersetup)
		x = floor( RI.viewport[0] * gpGlobals->width / gpGlobals->width );
		x2 = ceil(( RI.viewport[0] + RI.viewport[2] ) * gpGlobals->width / gpGlobals->width );
		y = floor( gpGlobals->height - RI.viewport[1] * gpGlobals->height / gpGlobals->height );
		y2 = ceil( gpGlobals->height - ( RI.viewport[1] + RI.viewport[3] ) * gpGlobals->height / gpGlobals->height );

		pglViewport( x, y2, x2 - x, y - y2 );
	}
	else
	{
		// envpass, mirrorpass
		pglViewport( RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3] );
	}
	*/

	// ...

	/*
		// TODO sort translucents entities by rendermode and distance
		qsort( tr.draw_list->trans_entities, tr.draw_list->num_trans_entities, sizeof( cl_entity_t* ), R_TransEntityCompare );
	*/

	{
		const VkDeviceSize offset = 0;
		vkCmdBindPipeline(vk_core.cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gmap.pipeline);
		vkCmdBindVertexBuffers(vk_core.cb, 0, 1, &gmap.vertex_buffer.buffer, &offset);
		vkCmdBindIndexBuffer(vk_core.cb, gmap.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);
	}

	if (!tglob.lightmapTextures[0])
	{
		gEngine.Con_Printf( S_ERROR "Don't have a lightmap texture\n");
		return;
	}

	vkCmdBindDescriptorSets(vk_core.cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gmap.pipeline_layout, 2, 1, &findTexture(tglob.lightmapTextures[0])->vk.descriptor, 0, NULL);

	// Draw world
	{
		const model_t *world = gEngine.pfnGetModelByIndex( 1 );
		if (world)
		{
			const uint32_t dynamic_offset[] = { sizeof(matrix4x4) * (0) };
			vkCmdBindDescriptorSets(vk_core.cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gmap.pipeline_layout, 0, 1, vk_core.descriptor_pool.ubo_sets, ARRAYSIZE(dynamic_offset), dynamic_offset);
			drawBrushModel( world );
		}
	}

	for (int i = 0; i < draw_list->num_solid_entities; ++i)
	{
		const cl_entity_t *ent = draw_list->solid_entities[i];
		const model_t *mod = ent->model;
		matrix4x4 model, ent_mvp;
		if (!mod)
			continue;

		if (mod->type != mod_brush )
			continue;

		R_RotateForEntity( model, ent );
		Matrix4x4_Concat( ent_mvp, mvp, model );
		Matrix4x4_ToArrayFloatGL( ent_mvp, (float*)(ubo->mvp+1+i) );

		{
			const uint32_t dynamic_offset[] = { sizeof(matrix4x4) * (1+i) };
			vkCmdBindDescriptorSets(vk_core.cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gmap.pipeline_layout, 0, 1, vk_core.descriptor_pool.ubo_sets, ARRAYSIZE(dynamic_offset), dynamic_offset);
		}
		drawBrushModel( mod );
	}
}

static int loadBrushSurfaces( const model_t *mod, vk_brush_model_surface_t *out_surfaces) {
	brush_vertex_t *bvert = gmap.vertex_buffer.mapped;
	uint16_t *bind = gmap.index_buffer.mapped;
	uint32_t vertex_offset = 0;
	int num_surfaces = 0;

	int num_indices = 0, num_vertices = 0, max_texture_id = 0;
	for( int i = 0; i < mod->nummodelsurfaces; ++i)
	{
		const msurface_t *surf = mod->surfaces + mod->firstmodelsurface + i;
		vk_brush_model_surface_t *bsurf = out_surfaces + num_surfaces;

		if( surf->flags & ( SURF_DRAWSKY | SURF_DRAWTURB | SURF_CONVEYOR | SURF_DRAWTURB_QUADS ) ) {
			gEngine.Con_Reportf("Skipping surface %d because of flags %08x\n", i, surf->flags);
			continue;
		}

		if( FBitSet( surf->flags, SURF_DRAWTILED )) {
			gEngine.Con_Reportf("Skipping surface %d because of tiled flag\n", i);
			continue;
		}

		num_vertices += surf->numedges;
		num_indices += 3 * (surf->numedges - 1);
		if (surf->texinfo->texture->gl_texturenum > max_texture_id)
			max_texture_id = surf->texinfo->texture->gl_texturenum;
	}

	// Load sorted by gl_texturenum
	for (int t = 0; t <= max_texture_id; ++t)
	{
		for( int i = 0; i < mod->nummodelsurfaces; ++i)
		{
			msurface_t *surf = mod->surfaces + mod->firstmodelsurface + i;
			vk_brush_model_surface_t *bsurf = out_surfaces + num_surfaces;
			mextrasurf_t	*info = surf->info;
			const float sample_size = gEngine.Mod_SampleSizeForFace( surf );

			if( surf->flags & ( SURF_DRAWSKY | SURF_DRAWTURB | SURF_CONVEYOR | SURF_DRAWTURB_QUADS ) )
				continue;

			if( FBitSet( surf->flags, SURF_DRAWTILED ))
				continue;

			if (t != surf->texinfo->texture->gl_texturenum)
				continue;

			++num_surfaces;

			//gEngine.Con_Reportf( "surface %d: numverts=%d numedges=%d\n", i, surf->polys ? surf->polys->numverts : -1, surf->numedges );

			if (surf->numedges + gmap.num_vertices > MAX_BUFFER_VERTICES)
			{
				gEngine.Con_Printf(S_ERROR "Ran out of buffer vertex space\n");
				return -1;
			}

			if ((surf->numedges-1) * 3 + gmap.num_indices > MAX_BUFFER_INDICES)
			{
				gEngine.Con_Printf(S_ERROR "Ran out of buffer index space\n");
				return -1;
			}

			if (vertex_offset + surf->numedges >= UINT16_MAX)
			{
				gEngine.Con_Printf(S_ERROR "Model %s indices don't fit into 16 bits\n", mod->name);
				return -1;
			}

			bsurf->texture_num = surf->texinfo->texture->gl_texturenum;
			bsurf->index_offset = gmap.num_indices;
			bsurf->index_count = 0;

			VK_CreateSurfaceLightmap( surf, mod );

			for( int k = 0; k < surf->numedges; k++ )
			{
				const int iedge = mod->surfedges[surf->firstedge + k];
				const medge_t *edge = mod->edges + (iedge >= 0 ? iedge : -iedge);
				const mvertex_t *in_vertex = mod->vertexes + (iedge >= 0 ? edge->v[0] : edge->v[1]);
				brush_vertex_t vertex = {
					{in_vertex->position[0], in_vertex->position[1], in_vertex->position[2]},
				};

				float s = DotProduct( in_vertex->position, surf->texinfo->vecs[0] ) + surf->texinfo->vecs[0][3];
				float t = DotProduct( in_vertex->position, surf->texinfo->vecs[1] ) + surf->texinfo->vecs[1][3];

				s /= surf->texinfo->texture->width;
				t /= surf->texinfo->texture->height;

				vertex.gl_tc[0] = s;
				vertex.gl_tc[1] = t;

				// lightmap texture coordinates
				s = DotProduct( in_vertex->position, info->lmvecs[0] ) + info->lmvecs[0][3];
				s -= info->lightmapmins[0];
				s += surf->light_s * sample_size;
				s += sample_size * 0.5f;
				s /= BLOCK_SIZE * sample_size; //fa->texinfo->texture->width;

				t = DotProduct( in_vertex->position, info->lmvecs[1] ) + info->lmvecs[1][3];
				t -= info->lightmapmins[1];
				t += surf->light_t * sample_size;
				t += sample_size * 0.5f;
				t /= BLOCK_SIZE * sample_size; //fa->texinfo->texture->height;

				vertex.lm_tc[0] = s;
				vertex.lm_tc[1] = t;

				//gEngine.Con_Printf("VERT %u %f %f %f\n", gmap.num_vertices, in_vertex->position[0], in_vertex->position[1], in_vertex->position[2]);

				bvert[gmap.num_vertices++] = vertex;

				// TODO contemplate triangle_strip (or fan?) + primitive restart
				if (k > 1) {
					bind[gmap.num_indices++] = (uint16_t)(vertex_offset + 0);
					bind[gmap.num_indices++] = (uint16_t)(vertex_offset + k - 1);
					bind[gmap.num_indices++] = (uint16_t)(vertex_offset + k);
					bsurf->index_count += 3;
				}
			}

			vertex_offset += surf->numedges;
		}
	}

	return num_surfaces;
}

qboolean VK_LoadBrushModel( model_t *mod, const byte *buffer )
{
	vk_brush_model_t *bmodel;

	if (mod->cache.data)
	{
		gEngine.Con_Reportf( S_WARN "Model %s was already loaded\n", mod->name );
		return true;
	}

	gEngine.Con_Reportf("%s: %s flags=%08x\n", __FUNCTION__, mod->name, mod->flags);

	bmodel = Mem_Malloc(vk_core.pool, sizeof(vk_brush_model_t) + sizeof(vk_brush_model_surface_t) * mod->nummodelsurfaces);
	mod->cache.data = bmodel;

	bmodel->vertex_offset = gmap.num_vertices;
	bmodel->num_surfaces = loadBrushSurfaces( mod, bmodel->surfaces );
	if (bmodel->num_surfaces < 0) {
		gEngine.Con_Reportf( S_ERROR "Model %s was not loaded\n", mod->name );
		return false;
	}

	/* gEngine.Con_Reportf("Model %s, vertex_offset=%d, first surface index_offset=%d\n", */
	/* 		mod->name, */
	/* 		bmodel->vertex_offset, bmodel->num_surfaces ? bmodel->surfaces[0].index_offset : -1); */
	/* for (int i = 0; i < bmodel->num_surfaces; ++i) */
	/* 	gEngine.Con_Reportf("\t%d: tex=%d, off=%d, cnt=%d\n", i, */
	/* 			bmodel->surfaces[i].texture_num, */
	/* 			bmodel->surfaces[i].index_offset, */
	/* 			bmodel->surfaces[i].index_count); */

	gEngine.Con_Reportf("Model %s loaded surfaces: %d (of %d); total vertices: %u, total indices: %u\n", mod->name, bmodel->num_surfaces, mod->nummodelsurfaces, gmap.num_vertices, gmap.num_indices);
	return true;
}

void VK_BrushClear( void )
{
	// Free previous map data
	gmap.num_vertices = 0;
	gmap.num_indices = 0;
}