#include "r_rpart.h"

#include "vk_render.h"
#include "vk_sprite.h" // R_GetSpriteTexture
#include "vk_textures.h"
#include "camera.h"
#include "vk_math.h"
#include "vk_common.h"
#include "vk_const.h"

#include "xash3d_types.h"
#include "const.h"
#include "r_efx.h"
#include "pm_movevars.h"

/* #include "event_flags.h" */
/* #include "entity_types.h" */
/* #include "triangleapi.h" */
/* #include "pm_local.h" */
/* #include "cl_tent.h" */
/* #include "studio.h" */

static float gTracerSize[11] = { 1.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
static color24 gTracerColors[] =
{
{ 255, 255, 255 },		// White
{ 255, 0, 0 },		// Red
{ 0, 255, 0 },		// Green
{ 0, 0, 255 },		// Blue
{ 0, 0, 0 },		// Tracer default, filled in from cvars, etc.
{ 255, 167, 17 },		// Yellow-orange sparks
{ 255, 130, 90 },		// Yellowish streaks (garg)
{ 55, 60, 144 },		// Blue egon streak
{ 255, 130, 90 },		// More Yellowish streaks (garg)
{ 255, 140, 90 },		// More Yellowish streaks (garg)
{ 200, 130, 90 },		// More red streaks (garg)
{ 255, 120, 70 },		// Darker red streaks (garg)
};

/* update particle color, position, free expired and draw it */
void CL_DrawParticles( double frametime, particle_t *cl_active_particles, float partsize )
{
	PRINT_NOT_IMPLEMENTED();

#if 0

	particle_t	*p;
	vec3_t		right, up;
	color24		*pColor;
	int		alpha;
	float		size;

	if( !cl_active_particles )
		return;	// nothing to draw?

	/* pglEnable( GL_BLEND ); */
	/* pglDisable( GL_ALPHA_TEST ); */
	/* pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ); */
  /*  */
	/* GL_Bind( XASH_TEXTURE0, tr.particleTexture ); */
	/* pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE ); */
	/* pglDepthMask( GL_FALSE ); */

	pglBegin( GL_QUADS );

	for( p = cl_active_particles; p; p = p->next )
	{
		if(( p->type != pt_blob ) || ( p->packedColor == 255 ))
		{
			size = partsize; // get initial size of particle

			// scale up to keep particles from disappearing
			size += (p->org[0] - RI.vieworg[0]) * RI.cull_vforward[0];
			size += (p->org[1] - RI.vieworg[1]) * RI.cull_vforward[1];
			size += (p->org[2] - RI.vieworg[2]) * RI.cull_vforward[2];

			if( size < 20.0f ) size = partsize;
			else size = partsize + size * 0.002f;

			// scale the axes by radius
			VectorScale( RI.cull_vright, size, right );
			VectorScale( RI.cull_vup, size, up );

			p->color = bound( 0, p->color, 255 );
			pColor = gEngfuncs.CL_GetPaletteColor( p->color );

			alpha = 255 * (p->die - gpGlobals->time) * 16.0f;
			if( alpha > 255 || p->type == pt_static )
				alpha = 255;

			pglColor4ub( gEngfuncs.LightToTexGamma( pColor->r ),
				gEngfuncs.LightToTexGamma( pColor->g ),
				gEngfuncs.LightToTexGamma( pColor->b ), alpha );

			pglTexCoord2f( 0.0f, 1.0f );
			pglVertex3f( p->org[0] - right[0] + up[0], p->org[1] - right[1] + up[1], p->org[2] - right[2] + up[2] );
			pglTexCoord2f( 0.0f, 0.0f );
			pglVertex3f( p->org[0] + right[0] + up[0], p->org[1] + right[1] + up[1], p->org[2] + right[2] + up[2] );
			pglTexCoord2f( 1.0f, 0.0f );
			pglVertex3f( p->org[0] + right[0] - up[0], p->org[1] + right[1] - up[1], p->org[2] + right[2] - up[2] );
			pglTexCoord2f( 1.0f, 1.0f );
			pglVertex3f( p->org[0] - right[0] - up[0], p->org[1] - right[1] - up[1], p->org[2] - right[2] - up[2] );
			r_stats.c_particle_count++;
		}

		gEngfuncs.CL_ThinkParticle( frametime, p );
	}

	pglEnd();
	pglDepthMask( GL_TRUE );
#endif
}

#if 0
// FIXME Vk
/* check tracer bbox */
static qboolean CL_CullTracer( particle_t *p, const vec3_t start, const vec3_t end )
{
	vec3_t	mins, maxs;
	int	i;

	// compute the bounding box
	for( i = 0; i < 3; i++ )
	{
		if( start[i] < end[i] )
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}

		// don't let it be zero sized
		if( mins[i] == maxs[i] )
		{
			maxs[i] += gTracerSize[p->type] * 2.0f;
		}
	}

	// check bbox
	return R_CullBox( mins, maxs );
}
#endif

static int getSpriteTexture( model_t *pSpriteModel, int frame ) {
	int	gl_texturenum;

	if(( gl_texturenum = R_GetSpriteTexture( pSpriteModel, frame )) == 0 )
		return -1;

	if( gl_texturenum <= 0 || gl_texturenum > MAX_TEXTURES )
		gl_texturenum = tglob.defaultTexture;

	return 1;
}

/* update tracer color, position, free expired and draw it */
void CL_DrawTracers( double frametime, particle_t *cl_active_tracers )
{
	float		scale, atten, gravity;
	vec3_t		screenLast, screen;
	vec3_t		start, end, delta;
	int texture_id;
	particle_t	*p;
	const struct movevars_s *const movevars = gEngine.pfnGetMoveVars();

	int num_tracers = 0;
	xvk_render_buffer_allocation_t vertex_buffer, index_buffer;
	vk_vertex_t *dst_vtx;
	uint16_t *dst_idx;

#if 0
	// FIXME VK
	// update tracer color if this is changed
	if( FBitSet( tracerred->flags|tracergreen->flags|tracerblue->flags|traceralpha->flags, FCVAR_CHANGED ))
	{
		color24 *customColors = &gTracerColors[4];
		customColors->r = (byte)(tracerred->value * traceralpha->value * 255);
		customColors->g = (byte)(tracergreen->value * traceralpha->value * 255);
		customColors->b = (byte)(tracerblue->value * traceralpha->value * 255);
		ClearBits( tracerred->flags, FCVAR_CHANGED );
		ClearBits( tracergreen->flags, FCVAR_CHANGED );
		ClearBits( tracerblue->flags, FCVAR_CHANGED );
		ClearBits( traceralpha->flags, FCVAR_CHANGED );
	}
#endif

	if( !cl_active_tracers )
		return;	// nothing to draw?

	texture_id = getSpriteTexture( gEngine.GetDefaultSprite( REF_DOT_SPRITE ), 0 );
	if (texture_id < 0)
		return;

	/* pglEnable( GL_BLEND ); */
	/* pglBlendFunc( GL_SRC_ALPHA, GL_ONE ); */
	/* pglDisable( GL_ALPHA_TEST ); */
	/* pglDepthMask( GL_FALSE ); */

	gravity = frametime * movevars->gravity;
	scale = 1.0 - (frametime * 0.9);
	if( scale < 0.0f ) scale = 0.0f;

	for( p = cl_active_tracers; p; p = p->next ) { ++num_tracers; }

	// Get buffer region for vertices and indices
	vertex_buffer = XVK_RenderBufferAllocAndLock( sizeof(vk_vertex_t), 4 * num_tracers );
	index_buffer = XVK_RenderBufferAllocAndLock( sizeof(uint16_t), 6 * num_tracers );
	if (!vertex_buffer.ptr || !index_buffer.ptr)
	{
		// TODO should we free one of the above if it still succeeded?
		gEngine.Con_Printf(S_ERROR "Ran out of buffer space\n");
		return;
	}

	dst_vtx = vertex_buffer.ptr;
	dst_idx = index_buffer.ptr;

	num_tracers = 0;
	for( p = cl_active_tracers; p; p = p->next, ++num_tracers, dst_vtx += 4, dst_idx += 6 )
	{
		atten = (p->die - gpGlobals->time);
		if( atten > 0.1f ) atten = 0.1f;

		VectorScale( p->vel, ( p->ramp * atten ), delta );
		VectorAdd( p->org, delta, end );
		VectorCopy( p->org, start );

		// FIXME VK if( !CL_CullTracer( p, start, end ))
		{
			vec3_t	verts[4], tmp2;
			vec3_t	tmp, normal;
			color24	*pColor;

			// Transform point into screen space
			TriWorldToScreen( start, screen );
			TriWorldToScreen( end, screenLast );

			// build world-space normal to screen-space direction vector
			VectorSubtract( screen, screenLast, tmp );

			// we don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );

			// build point along noraml line (normal is -y, x)
			VectorScale( g_camera.vup, tmp[0] * gTracerSize[p->type], normal );
			VectorScale( g_camera.vright, -tmp[1] * gTracerSize[p->type], tmp2 );
			VectorSubtract( normal, tmp2, normal );

			// compute four vertexes
			VectorSubtract( start, normal, verts[0] );
			VectorAdd( start, normal, verts[1] );
			VectorAdd( verts[0], delta, verts[2] );
			VectorAdd( verts[1], delta, verts[3] );

			if( p->color > sizeof( gTracerColors ) / sizeof( color24 ) )
			{
				gEngine.Con_Printf( S_ERROR "UserTracer with color > %d\n", sizeof( gTracerColors ) / sizeof( color24 ));
				p->color = 0;
			}

			pColor = &gTracerColors[p->color];

			// FIXME per-tracer!
			// FIXME VK pglColor4ub( pColor->r, pColor->g, pColor->b, p->packedColor );
			VK_RenderStateSetColor( pColor->r / 255.f, pColor->g / 255.f, pColor->b / 255.f, p->packedColor / 255.f);

			/* pglBegin( GL_QUADS ); */
			/* 	pglTexCoord2f( 0.0f, 0.8f ); */
			/* 	pglVertex3fv( verts[2] ); */
			/* 	pglTexCoord2f( 1.0f, 0.8f ); */
			/* 	pglVertex3fv( verts[3] ); */
			/* 	pglTexCoord2f( 1.0f, 0.0f ); */
			/* 	pglVertex3fv( verts[1] ); */
			/* 	pglTexCoord2f( 0.0f, 0.0f ); */
			/* 	pglVertex3fv( verts[0] ); */
			/* pglEnd(); */

			VectorCopy(verts[0], dst_vtx[0].pos);
			dst_vtx[0].gl_tc[0] = 0.f;
			dst_vtx[0].gl_tc[1] = 0.f;
			dst_vtx[0].lm_tc[0] = dst_vtx[0].lm_tc[1] = 0.f;

			VectorCopy(verts[1], dst_vtx[1].pos);
			dst_vtx[1].gl_tc[0] = 1.f;
			dst_vtx[1].gl_tc[1] = 0.f;
			dst_vtx[1].lm_tc[0] = dst_vtx[1].lm_tc[1] = 0.f;

			VectorCopy(verts[2], dst_vtx[2].pos);
			dst_vtx[2].gl_tc[0] = 0.f;
			dst_vtx[2].gl_tc[1] = 0.8f;
			dst_vtx[2].lm_tc[0] = dst_vtx[2].lm_tc[1] = 0.f;

			VectorCopy(verts[3], dst_vtx[3].pos);
			dst_vtx[3].gl_tc[0] = 1.f;
			dst_vtx[3].gl_tc[1] = 0.8f;
			dst_vtx[3].lm_tc[0] = dst_vtx[3].lm_tc[1] = 0.f;

			dst_idx[0] = 0 + num_tracers * 4;
			dst_idx[1] = 1 + num_tracers * 4;
			dst_idx[2] = 2 + num_tracers * 4;
			dst_idx[3] = 0 + num_tracers * 4;
			dst_idx[4] = 2 + num_tracers * 4;
			dst_idx[5] = 3 + num_tracers * 4;
		}

		XVK_RenderBufferUnlock( index_buffer.buffer );
		XVK_RenderBufferUnlock( vertex_buffer.buffer );

	{
		const vk_render_geometry_t geometry = {
			.texture = texture_id,
			.material = kXVkMaterialEmissive,

			.max_vertex = 4 * num_tracers,
			.vertex_offset = vertex_buffer.buffer.unit.offset,

			.element_count = 6 * num_tracers,
			.index_offset = index_buffer.buffer.unit.offset,

			// FIXME
			//.emissive = {pColor->r / 255.f, pColor->g / 255.f, pColor->b / 255.f},
			.emissive = {0, 0, 0},//pColor->r / 255.f, pColor->g / 255.f, pColor->b / 255.f},
		};

		VK_RenderModelDynamicBegin( kRenderTransAdd, "tracer %d", num_tracers );
		VK_RenderModelDynamicAddGeometry( &geometry );
		VK_RenderModelDynamicCommit();
	}

		// evaluate position
		VectorMA( p->org, frametime, p->vel, p->org );

		if( p->type == pt_grav )
		{
			p->vel[0] *= scale;
			p->vel[1] *= scale;
			p->vel[2] -= gravity;

			p->packedColor = 255 * (p->die - gpGlobals->time) * 2;
			if( p->packedColor > 255 ) p->packedColor = 255;
		}
		else if( p->type == pt_slowgrav )
		{
			p->vel[2] = gravity * 0.05f;
		}
	}
}

#if 0
// FIXME Vk
/*
===============
CL_DrawParticlesExternal

allow to draw effects from custom renderer
===============
*/
void CL_DrawParticlesExternal( const ref_viewpass_t *rvp, qboolean trans_pass, float frametime )
{
	ref_instance_t	oldRI = RI;

	memcpy( &oldRI, &RI, sizeof( ref_instance_t ));
	R_SetupRefParams( rvp );
	R_SetupFrustum();
	R_SetupGL( false );	// don't touch GL-states
	tr.frametime = frametime;

	gEngfuncs.CL_DrawEFX( frametime, trans_pass );

	// restore internal state
	memcpy( &RI, &oldRI, sizeof( ref_instance_t ));
}
#endif
