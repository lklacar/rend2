/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// tr_shade.c

#include "tr_local.h"
#include "tr_quicksprite.h"
#include "tr_buffers.h"
#include "tr_glsl.h"
#include "tr_gpucontext.h"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

shaderCommands_t	tess;

color4ub_t	styleColors[MAX_LIGHT_STYLES];

extern bool g_bRenderGlowingObjects;

static void R_DrawElements( int numIndexes, const glIndex_t *indexes ) {
	qglDrawElements( GL_TRIANGLES,
					numIndexes,
					GL_INDEX_TYPE,
					reinterpret_cast<const void*>(tess.indexOffset) );
}

/*
=============================================================

SURFACE SHADERS

=============================================================
*/

/*
=================
R_BindAnimatedImage

=================
*/

// de-static'd because tr_quicksprite wants it
image_t* R_GetAnimatedImage( textureBundle_t *bundle ) {
	int		index;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return tr.scratchImage[bundle->videoMapHandle];
	}

	if ((r_fullbright->value /*|| tr.refdef.doFullbright */) && bundle->isLightmap)
	{
		return tr.whiteImage;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		return bundle->image;
	}

	if (backEnd.currentEntity->e.renderfx & RF_SETANIMINDEX )
	{
		index = backEnd.currentEntity->e.skinNum;
	}
	else
	{
		// it is necessary to do this messy calc to make sure animations line up
		// exactly with waveforms of the same frequency
		index = Q_ftol( tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
		index >>= FUNCTABLE_SIZE2;

		if ( index < 0 ) {
			index = 0;	// may happen with shader time offsets
		}
	}

	if ( bundle->oneShotAnimMap )
	{
		if ( index >= bundle->numImageAnimations )
		{
			// stick on last frame
			index = bundle->numImageAnimations - 1;
		}
	}
	else
	{
		// loop
		index %= bundle->numImageAnimations;
	}

	return *((image_t**)bundle->image + index);
}

/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris (shaderCommands_t *input) {
    if (input->numVertexes <= 0) {
        return;
    }

	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	qglDisableClientState (GL_COLOR_ARRAY);
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);

	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);	// padded for SIMD

	R_DrawElements( input->numIndexes, input->indexes );

	qglDepthRange( 0, 1 );
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals (shaderCommands_t *input) {
	int		i;
	vec3_t	temp;

	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);
	qglDepthRange( 0, 0 );	// never occluded
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	qglBegin (GL_LINES);
	for (i = 0 ; i < input->numVertexes ; i++) {
		qglVertex3fv (input->xyz[i]);
		VectorMA (input->xyz[i], 2, input->normal[i], temp);
		qglVertex3fv (temp);
	}
	qglEnd ();

	qglDepthRange( 0, 1 );
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum ) {
	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.fogNum = fogNum;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc = shader->sky ? RB_StageIteratorSky : RB_StageIteratorGeneric;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime) {
		tess.shaderTime = tess.shader->clampTime;
	}

	tess.fading = false;

	tess.registration++;
}

/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void DrawMultitextured( shaderCommands_t *input, DrawItem::Layer* drawItemLayer, int stage ) {
	shaderStage_t	*pStage;

	pStage = &tess.xstages[stage];

	drawItemLayer->shaderOptions |= MAIN_SHADER_MULTITEXTURE;
	drawItemLayer->enabledVertexAttributes |= 16;  // add texcoord1
	drawItemLayer->textures[0] = R_GetAnimatedImage(&pStage->bundle[0]);
	drawItemLayer->textures[1] = R_GetAnimatedImage(&pStage->bundle[1]);
	drawItemLayer->vertexBuffers[4] = GpuBuffers_AllocFrameVertexDataMemory(
		input->svars.texcoords[1], sizeof(input->svars.texcoords[1][0]) * input->numVertexes);
	drawItemLayer->stateGroup.stateBits = pStage->stateBits;
	drawItemLayer->modulateTextures = (tess.shader->multitextureEnv == GL_MODULATE);

	//if ( r_lightmap->integer ) {
		//GL_TexEnv( GL_REPLACE );
	//} else {
		//GL_TexEnv( tess.shader->multitextureEnv );
	//}
}

static bool IsNonLightmapOpaqueTexture(const textureBundle_t *bundle)
{
	return bundle->image &&
		!bundle->isLightmap &&
		!bundle->numTexMods &&
		bundle->tcGen != TCGEN_ENVIRONMENT_MAPPED &&
		bundle->tcGen != TCGEN_FOG;
}

/*
===================
RB_LightingPass

Perform dynamic lighting with another rendering pass
===================
*/
static void RB_LightingPass( DrawItem* drawItem, const VertexBuffer* positionsBuffer ) {
	if (backEnd.refdef.num_dlights == 0)
	{
		return;
	}

	textureBundle_t *textureBundle = nullptr;
	if (tess.shader)
	{
		int i = 0;
		shaderStage_t *dStage = tess.shader->stages;

		while (i++ < tess.shader->numUnfoggedPasses)
		{
			const int blendBits = (GLS_SRCBLEND_BITS+GLS_DSTBLEND_BITS);
			if ((dStage->stateBits & blendBits) != 0)
			{
				continue;
			}

			//only use non-lightmap opaque stages
			if (IsNonLightmapOpaqueTexture(&dStage->bundle[0]))
			{
				textureBundle = &dStage->bundle[0];
				break;
			}

			if (IsNonLightmapOpaqueTexture(&dStage->bundle[1]))
			{
				textureBundle = &dStage->bundle[1];
				break;
			}

			++dStage;
		}
	}

	if (textureBundle != nullptr)
	{
		DrawItem::Layer* layer = drawItem->layers + drawItem->layerCount++;

		layer->shaderProgram = GLSL_MainShader_GetHandle();
		layer->shaderOptions = MAIN_SHADER_RENDER_LIGHTS_WITH_TEXTURE;
		layer->enabledVertexAttributes = 9;  // position = 1, texcoord0 = 8
		layer->vertexBuffers[0] = *positionsBuffer;
		layer->vertexBuffers[3] = GpuBuffers_AllocFrameVertexDataMemory(
			tess.svars.texcoords[0], sizeof(tess.svars.texcoords[0][0]) * tess.numVertexes);
		layer->storageBuffersUsed = 1;
		layer->storageBuffers[0] = backEnd.modelsStorageBuffer;
		layer->constantBuffersUsed = 1;
		layer->constantBuffers[0] = backEnd.viewConstantsBuffer;
		layer->stateGroup.stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		layer->textures[0] = R_GetAnimatedImage(textureBundle);
		layer->lightBits = tess.dlightBits;

		// Draw the lit mesh
		// Pass tess.dlightBits and backEnd.refdef.num_dlights - shader can loop over the lights
	}
	else
	{
		uint64_t blendBits = GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
#if 0
		if ( dl->additive ) {
			blendBits |= GLS_SRCBLEND_ONE;
		}
		else {
			blendBits |= GLS_SRCBLEND_DST_COLOR;
		}
#endif

		//GL_State(blendBits);
		//R_DrawElements(numIndexes, hitIndexes);
	}

	backEnd.pc.c_totalIndexes += tess.numIndexes;
	backEnd.pc.c_dlightIndexes += tess.numIndexes;
}

/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( void ) {
	fog_t		*fog;
	int			i;

	qglEnableClientState( GL_COLOR_ARRAY );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );

	fog = tr.world->fogs + tess.fogNum;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		byteAlias_t *ba = (byteAlias_t *)&tess.svars.colors[i];
		ba->i = fog->colorInt;
	}

	RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[0] );

	GL_Bind( tr.fogImage );

	if ( tess.shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	R_DrawElements( tess.numIndexes, tess.indexes );
}

/*
===============
ComputeColors
===============
*/

static void ComputeColors( shaderStage_t *pStage, int forceRGBGen )
{
	int			i;
	color4ub_t	*colors = tess.svars.colors;
	qboolean killGen = qfalse;
	alphaGen_t forceAlphaGen = pStage->alphaGen;//set this up so we can override below

	if ( tess.shader != tr.projectionShadowShader && tess.shader != tr.shadowShader &&
			( backEnd.currentEntity->e.renderfx & (RF_DISINTEGRATE1|RF_DISINTEGRATE2)))
	{
		RB_CalcDisintegrateColors( (unsigned char *)tess.svars.colors );
		RB_CalcDisintegrateVertDeform();

		// We've done some custom alpha and color stuff, so we can skip the rest.  Let it do fog though
		killGen = qtrue;
	}

	//
	// rgbGen
	//
	if ( !forceRGBGen )
	{
		forceRGBGen = pStage->rgbGen;
	}

	if ( backEnd.currentEntity->e.renderfx & RF_VOLUMETRIC ) // does not work for rotated models, technically, this should also be a CGEN type, but that would entail adding new shader commands....which is too much work for one thing
	{
		int			i;
		float		*normal, dot;
		unsigned char *color;
		int			numVertexes;

		normal = tess.normal[0];
		color = tess.svars.colors[0];

		numVertexes = tess.numVertexes;

		for ( i = 0 ; i < numVertexes ; i++, normal += 4, color += 4)
		{
			dot = DotProduct( normal, backEnd.refdef.viewaxis[0] );

			dot *= dot * dot * dot;

			if ( dot < 0.2f ) // so low, so just clamp it
			{
				dot = 0.0f;
			}

			color[0] = color[1] = color[2] = color[3] = Q_ftol( backEnd.currentEntity->e.shaderRGBA[0] * (1-dot) );
		}

		killGen = qtrue;
	}

	if (killGen)
	{
		goto avoidGen;
	}

	//
	// rgbGen
	//
	switch ( forceRGBGen )
	{
		case CGEN_IDENTITY:
			memset( tess.svars.colors, 0xff, tess.numVertexes * 4 );
			break;
		default:
		case CGEN_IDENTITY_LIGHTING:
			memset( tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4 );
			break;
		case CGEN_LIGHTING_DIFFUSE:
			RB_CalcDiffuseColor( ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_LIGHTING_DIFFUSE_ENTITY:
			RB_CalcDiffuseEntityColor( ( unsigned char * ) tess.svars.colors );
			if ( forceAlphaGen == AGEN_IDENTITY &&
				 backEnd.currentEntity->e.shaderRGBA[3] == 0xff
				)
			{
				forceAlphaGen = AGEN_SKIP;	//already got it in this set since it does all 4 components
			}
			break;
		case CGEN_EXACT_VERTEX:
			memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			break;
		case CGEN_CONST:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				byteAlias_t *baDest = (byteAlias_t *)&tess.svars.colors[i],
					*baSource = (byteAlias_t *)&pStage->constantColor;
				baDest->i = baSource->i;
			}
			break;
		case CGEN_VERTEX:
			if ( tr.identityLight == 1 )
			{
				memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = tess.vertexColors[i][0] * tr.identityLight;
					tess.svars.colors[i][1] = tess.vertexColors[i][1] * tr.identityLight;
					tess.svars.colors[i][2] = tess.vertexColors[i][2] * tr.identityLight;
					tess.svars.colors[i][3] = tess.vertexColors[i][3];
				}
			}
			break;
		case CGEN_ONE_MINUS_VERTEX:
			if ( tr.identityLight == 1 )
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = 255 - tess.vertexColors[i][0];
					tess.svars.colors[i][1] = 255 - tess.vertexColors[i][1];
					tess.svars.colors[i][2] = 255 - tess.vertexColors[i][2];
				}
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = ( 255 - tess.vertexColors[i][0] ) * tr.identityLight;
					tess.svars.colors[i][1] = ( 255 - tess.vertexColors[i][1] ) * tr.identityLight;
					tess.svars.colors[i][2] = ( 255 - tess.vertexColors[i][2] ) * tr.identityLight;
				}
			}
			break;
		case CGEN_FOG:
			{
				fog_t		*fog;

				fog = tr.world->fogs + tess.fogNum;

				for ( i = 0; i < tess.numVertexes; i++ ) {
					byteAlias_t *ba = (byteAlias_t *)&tess.svars.colors[i];
					ba->i = fog->colorInt;
				}
			}
			break;
		case CGEN_WAVEFORM:
			RB_CalcWaveColor( &pStage->rgbWave, ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_ENTITY:
			RB_CalcColorFromEntity( ( unsigned char * ) tess.svars.colors );
			if ( forceAlphaGen == AGEN_IDENTITY &&
				 backEnd.currentEntity->e.shaderRGBA[3] == 0xff
				)
			{
				forceAlphaGen = AGEN_SKIP;	//already got it in this set since it does all 4 components
			}
			break;
		case CGEN_ONE_MINUS_ENTITY:
			RB_CalcColorFromOneMinusEntity( ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_LIGHTMAPSTYLE:
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				byteAlias_t *baDest = (byteAlias_t *)&tess.svars.colors[i],
					*baSource = (byteAlias_t *)&styleColors[pStage->lightmapStyle];
				baDest->i = baSource->i;
			}
			break;
	}

	//
	// alphaGen
	//
	switch ( pStage->alphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if ( forceRGBGen != CGEN_IDENTITY ) {
			if ( ( forceRGBGen == CGEN_VERTEX && tr.identityLight != 1 ) ||
				 forceRGBGen != CGEN_VERTEX ) {
				for ( i = 0; i < tess.numVertexes; i++ ) {
					tess.svars.colors[i][3] = 0xff;
				}
			}
		}
		break;
	case AGEN_CONST:
		if ( forceRGBGen != CGEN_CONST ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i][3] = pStage->constantColor[3];
			}
		}
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->alphaWave, ( unsigned char * ) tess.svars.colors );
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( ( unsigned char * ) tess.svars.colors );
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( ( unsigned char * ) tess.svars.colors );
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( ( unsigned char * ) tess.svars.colors );
		break;
    case AGEN_VERTEX:
		if ( forceRGBGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i][3] = tess.vertexColors[i][3];
			}
		}
        break;
    case AGEN_ONE_MINUS_VERTEX:
        for ( i = 0; i < tess.numVertexes; i++ )
        {
			tess.svars.colors[i][3] = 255 - tess.vertexColors[i][3];
        }
        break;
	case AGEN_PORTAL:
		{
			unsigned char alpha;

			for ( i = 0; i < tess.numVertexes; i++ )
			{
				float len;
				vec3_t v;

				VectorSubtract( tess.xyz[i], backEnd.viewParms.ori.origin, v );
				len = VectorLength( v );

				len /= tess.shader->portalRange;

				if ( len < 0 )
				{
					alpha = 0;
				}
				else if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	case AGEN_BLEND:
		if ( forceRGBGen != CGEN_VERTEX )
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				colors[i][3] = tess.vertexAlphas[i][pStage->index]; //rwwRMG - added support
			}
		}
		break;
	default:
		break;
	}
avoidGen:
	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum )
	{
		switch ( pStage->adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( ( unsigned char * ) tess.svars.colors );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( ( unsigned char * ) tess.svars.colors );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( ( unsigned char * ) tess.svars.colors );
			break;
		case ACFF_NONE:
			break;
		}
	}
}

/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords( shaderStage_t *pStage ) {
	int		i;
	int		b;
    float	*texcoords;

	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		int tm;

        texcoords = (float *)tess.svars.texcoords[b];
		//
		// generate the texture coordinates
		//
		switch ( pStage->bundle[b].tcGen )
		{
		case TCGEN_IDENTITY:
			memset( tess.svars.texcoords[b], 0, sizeof( float ) * 2 * tess.numVertexes );
			break;
		case TCGEN_TEXTURE:
			for ( i = 0 ; i < tess.numVertexes ; i++, texcoords += 2 ) {
				texcoords[0] = tess.texCoords[i][0][0];
				texcoords[1] = tess.texCoords[i][0][1];
			}
			break;
		case TCGEN_LIGHTMAP:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][1][0];
				texcoords[1] = tess.texCoords[i][1][1];
			}
			break;
		case TCGEN_LIGHTMAP1:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][2][0];
				texcoords[1] = tess.texCoords[i][2][1];
			}
			break;
		case TCGEN_LIGHTMAP2:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][3][0];
				texcoords[1] = tess.texCoords[i][3][1];
			}
			break;
		case TCGEN_LIGHTMAP3:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][4][0];
				texcoords[1] = tess.texCoords[i][4][1];
			}
			break;
		case TCGEN_VECTOR:
			for ( i = 0 ; i < tess.numVertexes ; i++, texcoords += 2 ) {
				texcoords[0] = DotProduct( tess.xyz[i], pStage->bundle[b].tcGenVectors[0] );
				texcoords[1] = DotProduct( tess.xyz[i], pStage->bundle[b].tcGenVectors[1] );
			}
			break;
		case TCGEN_FOG:
			RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[b] );
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
			if ( r_environmentMapping->integer ) {
				RB_CalcEnvironmentTexCoords( ( float * ) tess.svars.texcoords[b] );
			}
			else {
				memset( tess.svars.texcoords[b], 0, sizeof( float ) * 2 * tess.numVertexes );
			}
			break;
		case TCGEN_BAD:
			return;
		}

		//
		// alter texture coordinates
		//
		for ( tm = 0; tm < pStage->bundle[b].numTexMods ; tm++ ) {
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS;		// break out of for loop
				break;

			case TMOD_TURBULENT:
				RB_CalcTurbulentTexCoords( &pStage->bundle[b].texMods[tm].wave,
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord,
									 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords( pStage->bundle[b].texMods[tm].translate,	//scroll unioned
										 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords( pStage->bundle[b].texMods[tm].translate,
									 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_STRETCH:
				RB_CalcStretchTexCoords( &pStage->bundle[b].texMods[tm].wave,
						               ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_TRANSFORM:
				RB_CalcTransformTexCoords( &pStage->bundle[b].texMods[tm],
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords( pStage->bundle[b].texMods[tm].translate[0],
										( float * ) tess.svars.texcoords[b] );
				break;

			default:
				Com_Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'\n", pStage->bundle[b].texMods[tm].type, tess.shader->name );
				break;
			}
		}
	}
}

void ForceAlpha(unsigned char *dstColors, int TR_ForceEntAlpha)
{
	int	i;

	dstColors += 3;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 )
	{
		*dstColors = TR_ForceEntAlpha;
	}
}

/*
** RB_IterateStagesGeneric
*/
static vec4_t	GLFogOverrideColors[GLFOGOVERRIDE_MAX] =
{
	{ 0.0, 0.0, 0.0, 1.0 },	// GLFOGOVERRIDE_NONE
	{ 0.0, 0.0, 0.0, 1.0 },	// GLFOGOVERRIDE_BLACK
	{ 1.0, 1.0, 1.0, 1.0 }	// GLFOGOVERRIDE_WHITE
};

static const float logtestExp2 = (sqrt( -log( 1.0 / 255.0 ) ));
extern bool tr_stencilled; //tr_backend.cpp
static void RB_IterateStagesGeneric( DrawItem* drawItem, shaderCommands_t *input, const VertexBuffer* positionsBuffer )
{
	int stage;
	bool	UseGLFog = false;
	bool	FogColorChange = false;
	fog_t	*fog = NULL;

	if (tess.fogNum && tess.shader->fogPass && (tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs)
		&& r_drawfog->value == 2)
	{	// only gl fog global fog and the "special fog"
		fog = tr.world->fogs + tess.fogNum;

		if (tr.rangedFog)
		{ //ranged fog, used for sniper scope
			float fStart = fog->parms.depthForOpaque;

			if (tr.rangedFog < 0.0f)
			{ //special designer override
				fStart = -tr.rangedFog;
			}
			else
			{
				//the greater tr.rangedFog is, the more fog we will get between the view point and cull distance
				if ((tr.distanceCull-fStart) < tr.rangedFog)
				{ //assure a minimum range between fog beginning and cutoff distance
					fStart = tr.distanceCull-tr.rangedFog;

					if (fStart < 16.0f)
					{
						fStart = 16.0f;
					}
				}
			}

			qglFogi(GL_FOG_MODE, GL_LINEAR);

			qglFogf(GL_FOG_START, fStart);
			qglFogf(GL_FOG_END, tr.distanceCull);
		}
		else
		{
			qglFogi(GL_FOG_MODE, GL_EXP2);
			qglFogf(GL_FOG_DENSITY, logtestExp2 / fog->parms.depthForOpaque);
		}
		if ( g_bRenderGlowingObjects )
		{
			const float fogColor[3] = { 0.0f, 0.0f, 0.0f };
			qglFogfv(GL_FOG_COLOR, fogColor );
		}
		else
		{
			qglFogfv(GL_FOG_COLOR, fog->parms.color);
		}
		qglEnable(GL_FOG);
		UseGLFog = true;
	}

	const bool distortionEffect = ((tess.shader == tr.distortionShader) ||
		(backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_DISTORTION)));

	for ( stage = 0; stage < input->shader->numUnfoggedPasses; stage++ )
	{
		shaderStage_t *pStage = &tess.xstages[stage];

		if ( !pStage->active )
		{
			break;
		}

		// Reject this stage if it's not a glow stage but we are doing a glow pass.
		if ( g_bRenderGlowingObjects && !pStage->glow )
		{
			continue;
		}

		if ( stage && r_lightmap->integer && !( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}

		if (pStage->ss && pStage->ss->surfaceSpriteType)
		{
			// We check for surfacesprites AFTER drawing everything else
			continue;
		}

		if (UseGLFog)
		{
			if (pStage->mGLFogColorOverride)
			{
				qglFogfv(GL_FOG_COLOR, GLFogOverrideColors[pStage->mGLFogColorOverride]);
				FogColorChange = true;
			}
			else if (FogColorChange && fog)
			{
				FogColorChange = false;
				qglFogfv(GL_FOG_COLOR, fog->parms.color);
			}
		}

		if (!input->fading)
		{
			//this means ignore this, while we do a fade-out
			int forceRGBGen = 0;
			if ( backEnd.currentEntity )
			{
				assert(backEnd.currentEntity->e.renderfx >= 0);

				if ( backEnd.currentEntity->e.renderfx & RF_RGB_TINT )
				{//want to use RGBGen from ent
					forceRGBGen = CGEN_ENTITY;
				}
			}
			ComputeColors( pStage, forceRGBGen );

			if ( backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA) )
			{
				ForceAlpha((unsigned char*)tess.svars.colors, backEnd.currentEntity->e.shaderRGBA[3]);
			}
		}

		ComputeTexCoords( pStage );

		DrawItem::Layer* layer = drawItem->layers + drawItem->layerCount++;

		//
		// upload per-stage vertex data
		//
		layer->shaderProgram = GLSL_MainShader_GetHandle();
		if (!backEnd.projection2D)
		{
			layer->shaderOptions = MAIN_SHADER_RENDER_SCENE;
		}
		layer->enabledVertexAttributes = 13; // pos = 1, color = 4, texcoord = 8
		layer->vertexBuffers[0] = *positionsBuffer;
		layer->vertexBuffers[2] = GpuBuffers_AllocFrameVertexDataMemory(
			tess.svars.colors, sizeof(tess.svars.colors[0]) * input->numVertexes);
		layer->vertexBuffers[3] = GpuBuffers_AllocFrameVertexDataMemory(
			tess.svars.texcoords[0], sizeof(tess.svars.texcoords[0][0]) * input->numVertexes);

		if (!backEnd.projection2D)
		{
			layer->storageBuffersUsed = 1;
			layer->storageBuffers[0] = backEnd.modelsStorageBuffer;
		}

		layer->constantBuffersUsed = 1;
		layer->constantBuffers[0] = backEnd.viewConstantsBuffer;

		layer->alphaTestFunc = pStage->alphaTestFunc;
		layer->alphaTestValue = pStage->alphaTestValue;

		if ( pStage->bundle[1].image != 0 )
		{
			DrawMultitextured( input, layer, stage );
		}
		else
		{
			//
			// set textures
			//
			if (distortionEffect)
			{
				//special distortion effect -rww
				//tr.screenImage should have been set for this specific entity before we got in here.
				layer->textures[0] = tr.screenImage;
			}
			else if ( pStage->bundle[0].vertexLightmap && (r_vertexLight->integer && !r_uiFullScreen->integer) && r_lightmap->integer )
			{
				layer->textures[0] = tr.whiteImage;
			}
			else
			{
				layer->textures[0] = R_GetAnimatedImage(&pStage->bundle[0]);
			}

			//
			// set state
			//
			bool lStencilled = false;
			uint64_t stateBits = 0;
			if (tess.shader == tr.distortionShader && glConfig.stencilBits >= 4)
			{
				//draw it to the stencil buffer!
				tr_stencilled = true;
				lStencilled = true;
				qglEnable(GL_STENCIL_TEST);
				qglStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
				qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

				//don't depthmask, don't blend.. don't do anything
				stateBits = 0;
			}
			else if (backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA))
			{
				if (backEnd.currentEntity->e.renderfx & RF_ALPHA_DEPTH)
				{
					//depth write, so faces through the model will be stomped over by nearer ones. this works because
					//we draw RF_FORCE_ENT_ALPHA stuff after everything else, including standard alpha surfs.
					stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE;
				}
				else
				{
					stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
				}
			}
			else if (backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_DISINTEGRATE1))
			{
				// we want to be able to rip a hole in the thing being disintegrated, and by doing the depth-testing it avoids some kinds of artefacts, but will probably introduce others?
				stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE;
				layer->alphaTestFunc = AlphaTestFunc::GE;
				layer->alphaTestValue = 0.75f;
			}
			else
			{
				stateBits = pStage->stateBits;
			}

			//
			// shader-specific stateBits
			//
			if ( input->shader->polygonOffset )
			{
				stateBits |= GLS_POLYGON_OFFSET_TRUE;
			}

			if (!distortionEffect)
			{
				stateBits |= GL_GetCullState(input->shader->cullType);
			}

			layer->stateGroup.stateBits = stateBits;

			if (lStencilled)
			{
				//re-enable the color buffer, disable stencil test
				lStencilled = false;
				qglDisable(GL_STENCIL_TEST);
				qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
		}
	}

	if (FogColorChange)
	{
		qglFogfv(GL_FOG_COLOR, fog->parms.color);
	}
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;

	//
	// log this call
	//
	if ( r_logFile->integer )
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	input = &tess;

	RB_DeformTessGeometry();

	DrawItem drawItem = {};
	drawItem.drawType = DRAW_INDEXED;
	drawItem.primitiveType = PRIMITIVE_TRIANGLES;
	drawItem.count = input->numIndexes;
	drawItem.offset = 0;
	drawItem.minDepthRange = backEnd.minDepthRange;
	drawItem.maxDepthRange = backEnd.maxDepthRange;
	drawItem.indexBuffer = GpuBuffers_AllocFrameIndexDataMemory(input->indexes, input->numIndexes * sizeof(input->indexes[0]));

	drawItem.entityNum = 0;
	if (!backEnd.projection2D)
	{
		drawItem.isEntity = true;
		if (backEnd.currentEntity != &tr.worldEntity)
		{
			drawItem.entityNum = backEnd.currentEntity - backEnd.refdef.entities;
		}
		else
		{
			drawItem.entityNum = REFENTITYNUM_WORLD;
		}
	}

	const VertexBuffer positionsBuffer = GpuBuffers_AllocFrameVertexDataMemory(
		input->xyz, sizeof(input->xyz[0]) * input->numVertexes);

	//
	// call shader function
	//
	RB_IterateStagesGeneric( &drawItem, input, &positionsBuffer );

	//
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE
		&& !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		RB_LightingPass(&drawItem, &positionsBuffer);
	}

	RenderContext_AddDrawItem(drawItem);

#if defined(COOKIE)
	//
	// now do fog
	//
	if (tr.world && (tess.fogNum != tr.world->globalFog || r_drawfog->value != 2) && r_drawfog->value && tess.fogNum && tess.shader->fogPass)
	{
		RB_FogPass();
	}

	// Now check for surfacesprites.
	if (r_surfaceSprites->integer)
	{
		for ( stage = 1; stage < tess.shader->numUnfoggedPasses; stage++ )
		{
			if (tess.xstages[stage].ss && tess.xstages[stage].ss->surfaceSpriteType)
			{	// Draw the surfacesprite
				RB_DrawSurfaceSprites(&tess.xstages[stage], input);
			}
		}
	}

	//don't disable the hardware fog til after we do surface sprites
	if (r_drawfog->value == 2 &&
		tess.fogNum && tess.shader->fogPass &&
		(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
	{
		qglDisable(GL_FOG);
	}
#endif
}


/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0) {
		return;
	}

	if (input->indexes[SHADER_MAX_INDEXES-1] != 0) {
		Com_Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit");
	}
	if (input->xyz[SHADER_MAX_VERTEXES-1][0] != 0) {
		Com_Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit");
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	if ( skyboxportal )
	{
		// world
		if(!(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL))
		{
			if(tess.currentStageIteratorFunc == RB_StageIteratorSky)
			{	// don't process these tris at all
				return;
			}
		}
		// portal sky
		else
		{
			if(!drawskyboxportal)
			{
				if( !(tess.currentStageIteratorFunc == RB_StageIteratorSky))
				{	// /only/ process sky tris
					return;
				}
			}
		}
	}

	//
	// update performance counters
	//
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;
	if (tess.fogNum && tess.shader->fogPass && r_drawfog->value == 1)
	{
		backEnd.pc.c_totalIndexes += tess.numIndexes;
	}

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_showtris->integer ) {
		DrawTris (input);
	}
	if ( r_shownormals->integer ) {
		DrawNormals (input);
	}
	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;

	GLimp_LogComment( "----------\n" );
}

