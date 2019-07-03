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

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

shaderCommands_t	tess;
static qboolean	setArraysOnce;

color4ub_t	styleColors[MAX_LIGHT_STYLES];

extern bool g_bRenderGlowingObjects;


static VkCullModeFlags GetVkCullMode(cullType_t cullType)
{
	switch (cullType)
	{
		case CT_FRONT_SIDED: return VK_CULL_MODE_BACK_BIT;
		case CT_BACK_SIDED: return VK_CULL_MODE_FRONT_BIT;
		case CT_TWO_SIDED: return VK_CULL_MODE_NONE;
	}
}

static VkBlendFactor GetVkSrcBlendFactor(int stateBits)
{
	switch (stateBits & GLS_SRCBLEND_BITS)
	{
		case GLS_SRCBLEND_ZERO:
			return VK_BLEND_FACTOR_ZERO;
		case GLS_SRCBLEND_ONE:
			return VK_BLEND_FACTOR_ONE;
		case GLS_SRCBLEND_DST_COLOR:
			return VK_BLEND_FACTOR_DST_COLOR;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
			return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case GLS_SRCBLEND_SRC_ALPHA:
			return VK_BLEND_FACTOR_SRC_ALPHA;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case GLS_SRCBLEND_DST_ALPHA:
			return VK_BLEND_FACTOR_DST_ALPHA;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
			return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case GLS_SRCBLEND_ALPHA_SATURATE:
			return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	}
}

static VkBlendFactor GetVkDstBlendFactor(int stateBits)
{
	switch ( stateBits & GLS_DSTBLEND_BITS )
	{
		case GLS_DSTBLEND_ZERO:
			return VK_BLEND_FACTOR_ZERO;
		case GLS_DSTBLEND_ONE:
			return VK_BLEND_FACTOR_ONE;
		case GLS_DSTBLEND_SRC_COLOR:
			return VK_BLEND_FACTOR_SRC_COLOR;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case GLS_DSTBLEND_SRC_ALPHA:
			return VK_BLEND_FACTOR_SRC_ALPHA;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case GLS_DSTBLEND_DST_ALPHA:
			return VK_BLEND_FACTOR_DST_ALPHA;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
			return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	}
}

/*
==================
R_DrawElements

Optionally performs our own glDrawElements that looks for strip conditions
instead of using the single glDrawElements call that may be inefficient
without compiled vertex arrays.
==================
*/
static void R_DrawElements( int numIndexes, const glIndex_t *indexes ) {
	// STRAWB draw everythingggg
	//qglDrawElements(GL_TRIANGLES, numIndexes, GL_INDEX_TYPE, indexes);
#if 0
	const int stateBits = glState.stateBits;

	//
	// Descriptor set layout
	//
	VkDescriptorSetLayoutBinding bindings[1] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VL_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {};
	setLayoutCreateInfo.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCreateInfo.bindingCount = 1;
	setLayoutCreateInfo.pBindings = bindings;

	VkDesciptorSetLayout setLayout;
	if (vkCreateDescriptorSetLayout(
			gpuContext.device,
			&setLayoutCreateInfo,
			nullptr,
			&setLayout) != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_RED "Derp\n");
	}

	//
	// Descriptor set
	//
	VkDescriptorPoolSize poolSizes[1] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = 4096;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.maxSets = 2048;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = poolSizes;

	VkDescriptorPool descriptorPool;
	if (vkCreateDescriptorPool(
			gpuContext.device,
			&descriptorPoolCreateInfo,
			nullptr,
			&descriptorPool) != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_RED "Failed to create descriptor pool\n");
	}

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; 
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &setLayout;

	VkDescriptorSet descriptorSet;
	if (vkAllocateDescriptorSets(
			gpuContext.device,
			&descriptorSetAllocateInfo,
			&descriptorSet) != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_RED "Failed to create descriptor set\n");
	}

	VkDescriptorImageInfo descriptorImageInfo = {};
	descriptorImageInfo.sampler = VK_NULL_HANDLE;
	descriptorImageInfo.imageView = imageView;
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.pImageInfo = &descriptorImageInfo;
	vkUpdateDescriptorSets(gpuContext.device, 1, &descriptorWrite, 0, nullptr);

	//
	// pipeline layout
	//
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &setLayout;

	VkPipelineLayout pipelineLayout;
	if (vkCreatePipelineLayout(
			gpuContext.device,
			&pipelineLayoutCreateInfo,
			nullptr,
			&pipelineLayout) != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_RED "Failed to create pipeline layout\n");
	}

	//
	// load fake shader
	//
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = 0;
	shaderModuleCreateInfo.pCode = nullptr;

	VkShaderModule renderModule;
	if (vkCreateShaderModule(
			gpuContext.device,
			&shaderModuleCreateInfo,
			nullptr,
			&renderModule) != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_WARNING "Failed to create shader module\n");
	}

	//
	// creating the graphics pipeline 
	//
	VkPipelineShaderStageCreateInfo stageCreateInfos[2] = {};
	stageCreateInfos[0].sType =
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stageCreateInfos[0].module = renderModule;
	stageCreateInfos[0].pName = "vertexMain";
	stageCreateInfos[1].sType =
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stageCreateInfos[1].module = renderModule;
	stageCreateInfos[1].pName = "fragmentMain";

	std::array<VkVertexInputBindingDescription, 3> vertexBindings;
	vertexBindings[0].binding = 0;
	vertexBindings[0].stride = 0;
	vertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertexBindings[1].binding = 1;
	vertexBindings[1].stride = 0;
	vertexBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertexBindings[2].binding = 2;
	vertexBindings[2].stride = 0;
	vertexBindings[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::array<VkVertexInputAttributeDescription, 3> vertexAttributes;
	vertexAttributes[0].location = 0;
	vertexAttributes[0].binding = 0;
	vertexAttributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexAttributes[0].offset = 0;
	vertexAttributes[1].location = 0;
	vertexAttributes[1].binding = 1;
	vertexAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;
	vertexAttributes[1].offset = 0;
	vertexAttributes[2].location = 0;
	vertexAttributes[2].binding = 2;
	vertexAttributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	vertexAttributes[2].offset = 0;

	VkPipelineVertexInputStateCreateInfo viCreateInfo = {};
	viCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	viCreateInfo.vertexBindingDescriptionCount = 3;
	viCreateInfo.pVertexBindingDescriptions = vertexBindings.data();
	viCreateInfo.vertexAttributeDescriptionCount = 3;
	viCreateInfo.pVertexAttributeDescriptions = vertexAttributes.data();

	VkPipelineInputAssemblyStateCreateInfo iaCreateInfo = {};
	iaCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	iaCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport viewport = {};
	viewport.x = glState.viewport.x;
	viewport.y = glState.viewport.y;
	viewport.width = glState.viewport.width;
	viewport.height = glState.viewport.height;
	viewport.minDepth = glState.depthRangeMin;
	viewport.maxDepth = glState.depthRangeMax;

	VkRect2D scissor = {};
	scissor.offset = {glState.viewport.x, glState.viewport.y};
	scissor.extent = {glState.viewport.width, glState.viewport.height};

	VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
	viewportCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = &viewport;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rsCreateInfo = {};
	rsCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rsCreateInfo.polygonMode =
		(stateBits & GLS_POLYMODE_LINE)
			? VK_POLYGON_MODE_LINE
			: VK_POLYGON_MODE_FILL;
	rsCreateInfo.cullMode = GetVkCullMode(glState.cullType);
	rsCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineDepthStencilStateCreateInfo dsCreateInfo = {};
	dsCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	dsCreateInfo.depthTestEnable = !(stateBits & GLS_DEPTHTEST_DISABLE);
	dsCreateInfo.depthWriteEnable =
		(stateBits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
	dsCreateInfo.depthCompareOp =
		(stateBits & GLS_DEPTHFUNC_EQUAL)
			? VK_COMPARE_OP_EQUAL
			: VK_COMPARE_OP_LESS_OR_EQUAL;

	VkPipelineColorBlendAttachmentState cbAttachment = {};
	cbAttachment.blendEnable =
		(stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;
	cbAttachment.srcColorBlendFactor = GetVkSrcBlendFactor(stateBits);
	cbAttachment.dstColorBlendFactor = GetVkDstBlendFactor(stateBits);
	cbAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	cbAttachment.srcAlphaBlendFactor = GetVkSrcBlendFactor(stateBits);
	cbAttachment.dstAlphaBlendFactor = GetVkDstBlendFactor(stateBits);
	cbAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	cbAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo cbCreateInfo = {};
	cbCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cbCreateInfo.attachmentCount = 1;
	cbCreateInfo.pAttachments = cbAttachment;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.flags = 0;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = stageCreateInfos;
	pipelineCreateInfo.pVertexInputState = &viCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &iaCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rsCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &dsCreateInfo;
	pipelineCreateInfo.pColorBlendState = &cbCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subPass = 0;

	VkPipeline graphicsPipeline;
	if (vkCreateGraphicsPipelines(
			gpuContext.device,
			VK_NULL_HANDLE,
			1,
			&pipelineCreateInfo,
			nullptr,
			&graphicsPipeline) != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_RED "Failed to create graphics pipeline\n");
	}

	vkCmdBindPipeline(
		cmdBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		graphicsPipeline);

	vkCmdBindDescriptorSets(
		cmdBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelineLayout,
		0,
		1,
		descriptorSet,
		0,
		nullptr);

	vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

	const VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdDrawIndexed(cmdBuffer, numIndexes, 1, 0, 0, 0);
#endif
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
void R_BindAnimatedImage( textureBundle_t *bundle ) {
	int		index;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if ((r_fullbright->value /*|| tr.refdef.doFullbright */) && bundle->isLightmap)
	{
		GL_Bind( tr.whiteImage );
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		GL_Bind( bundle->image );
		return;
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

	GL_Bind( *((image_t**)bundle->image + index) );
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

	if (qglLockArraysEXT) {
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	if (qglUnlockArraysEXT) {
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
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
void RB_BeginSurface(shader_t *shader, int fogNum)
{
	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.fogNum = fogNum;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc =
		shader->sky ? RB_StageIteratorSky : RB_StageIteratorGeneric;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime)
	{
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
static void DrawMultitextured( shaderCommands_t *input, int stage ) {
	shaderStage_t	*pStage;

	pStage = &tess.xstages[stage];

	GL_State(pStage->stateBits);

	//
	// base
	//
	GL_SelectTexture( 0 );
	// STRAWB upload texture arrays input->svars.texcoords[0] to tex0
	R_BindAnimatedImage( &pStage->bundle[0] );

	//
	// lightmap/secondary pass
	//
	GL_SelectTexture( 1 );

	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( tess.shader->multitextureEnv );
	}

	// STRAWB upload texture arrays input->svars.texcoords1 to tex1
	R_BindAnimatedImage( &pStage->bundle[1] );

	R_DrawElements( input->numIndexes, input->indexes );

	GL_SelectTexture( 0 );
}

/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
static void ProjectDlightTexture2( void ) {
	int		i, l;
	vec3_t	origin;
	byte	clipBits[SHADER_MAX_VERTEXES];
	float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	float	oldTexCoordsArray[SHADER_MAX_VERTEXES][2];
	float	vertCoordsArray[SHADER_MAX_VERTEXES][4];
	unsigned int		colorArray[SHADER_MAX_VERTEXES];
	glIndex_t	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	radius;
	int		fogging;
	shaderStage_t *dStage;
	vec3_t	posa;
	vec3_t	posb;
	vec3_t	posc;
	vec3_t	dist;
	vec3_t	e1;
	vec3_t	e2;
	vec3_t	normal;
	float	fac,modulate;
	vec3_t	floatColor;
	byte colorTemp[4];

	int		needResetVerts=0;

	if ( !backEnd.refdef.num_dlights )
	{
		return;
	}

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ )
	{
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;

		int		clipall = 63;
		for ( i = 0 ; i < tess.numVertexes ; i++)
		{
			int		clip;
			VectorSubtract( origin, tess.xyz[i], dist );

			clip = 0;
			if (  dist[0] < -radius )
			{
				clip |= 1;
			}
			else if ( dist[0] > radius )
			{
				clip |= 2;
			}
			if (  dist[1] < -radius )
			{
				clip |= 4;
			}
			else if ( dist[1] > radius )
			{
				clip |= 8;
			}
			if (  dist[2] < -radius )
			{
				clip |= 16;
			}
			else if ( dist[2] > radius )
			{
				clip |= 32;
			}

			clipBits[i] = clip;
			clipall &= clip;
		}
		if ( clipall )
		{
			continue;	// this surface doesn't have any of this light
		}
		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 )
		{
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] )
			{
				continue;	// not lighted
			}

			// copy the vertex positions
			VectorCopy(tess.xyz[a],posa);
			VectorCopy(tess.xyz[b],posb);
			VectorCopy(tess.xyz[c],posc);

			VectorSubtract( posa, posb,e1);
			VectorSubtract( posc, posb,e2);
			CrossProduct(e1,e2,normal);
// rjr - removed for hacking 			if ( (!r_dlightBacks->integer && DotProduct(normal,origin)-DotProduct(normal,posa) <= 0.0f) || // backface
			if ( DotProduct(normal,origin)-DotProduct(normal,posa) <= 0.0f || // backface
				DotProduct(normal,normal) < 1E-8f) // junk triangle
			{
				continue;
			}
			VectorNormalize(normal);
			fac=DotProduct(normal,origin)-DotProduct(normal,posa);
			if (fac >= radius)  // out of range
			{
				continue;
			}
			modulate = 1.0f-((fac*fac) / (radius*radius));
			fac = 0.5f/sqrtf(radius*radius - fac*fac);

			// save the verts
			VectorCopy(posa,vertCoordsArray[numIndexes]);
			VectorCopy(posb,vertCoordsArray[numIndexes+1]);
			VectorCopy(posc,vertCoordsArray[numIndexes+2]);

			// now we need e1 and e2 to be an orthonormal basis
			if (DotProduct(e1,e1) > DotProduct(e2,e2))
			{
				VectorNormalize(e1);
				CrossProduct(e1,normal,e2);
			}
			else
			{
				VectorNormalize(e2);
				CrossProduct(normal,e2,e1);
			}
			VectorScale(e1,fac,e1);
			VectorScale(e2,fac,e2);

			VectorSubtract( posa, origin,dist);
			texCoordsArray[numIndexes][0]=DotProduct(dist,e1)+0.5f;
			texCoordsArray[numIndexes][1]=DotProduct(dist,e2)+0.5f;

			VectorSubtract( posb, origin,dist);
			texCoordsArray[numIndexes+1][0]=DotProduct(dist,e1)+0.5f;
			texCoordsArray[numIndexes+1][1]=DotProduct(dist,e2)+0.5f;

			VectorSubtract( posc, origin,dist);
			texCoordsArray[numIndexes+2][0]=DotProduct(dist,e1)+0.5f;
			texCoordsArray[numIndexes+2][1]=DotProduct(dist,e2)+0.5f;

			if ((texCoordsArray[numIndexes][0] < 0.0f && texCoordsArray[numIndexes+1][0] < 0.0f && texCoordsArray[numIndexes+2][0] < 0.0f) ||
				(texCoordsArray[numIndexes][0] > 1.0f && texCoordsArray[numIndexes+1][0] > 1.0f && texCoordsArray[numIndexes+2][0] > 1.0f) ||
				(texCoordsArray[numIndexes][1] < 0.0f && texCoordsArray[numIndexes+1][1] < 0.0f && texCoordsArray[numIndexes+2][1] < 0.0f) ||
				(texCoordsArray[numIndexes][1] > 1.0f && texCoordsArray[numIndexes+1][1] > 1.0f && texCoordsArray[numIndexes+2][1] > 1.0f) )
			{
				continue; // didn't end up hitting this tri
			}
			/* old code, get from the svars = wrong
			oldTexCoordsArray[numIndexes][0]=tess.svars.texcoords[0][a][0];
			oldTexCoordsArray[numIndexes][1]=tess.svars.texcoords[0][a][1];
			oldTexCoordsArray[numIndexes+1][0]=tess.svars.texcoords[0][b][0];
			oldTexCoordsArray[numIndexes+1][1]=tess.svars.texcoords[0][b][1];
			oldTexCoordsArray[numIndexes+2][0]=tess.svars.texcoords[0][c][0];
			oldTexCoordsArray[numIndexes+2][1]=tess.svars.texcoords[0][c][1];
			*/
			oldTexCoordsArray[numIndexes][0]=tess.texCoords[a][0][0];
			oldTexCoordsArray[numIndexes][1]=tess.texCoords[a][0][1];
			oldTexCoordsArray[numIndexes+1][0]=tess.texCoords[b][0][0];
			oldTexCoordsArray[numIndexes+1][1]=tess.texCoords[b][0][1];
			oldTexCoordsArray[numIndexes+2][0]=tess.texCoords[c][0][0];
			oldTexCoordsArray[numIndexes+2][1]=tess.texCoords[c][0][1];

			colorTemp[0] = Q_ftol(floatColor[0] * modulate);
			colorTemp[1] = Q_ftol(floatColor[1] * modulate);
			colorTemp[2] = Q_ftol(floatColor[2] * modulate);
			colorTemp[3] = 255;

			byteAlias_t *ba = (byteAlias_t *)&colorTemp;
			colorArray[numIndexes + 0] = ba->ui;
			colorArray[numIndexes + 1] = ba->ui;
			colorArray[numIndexes + 2] = ba->ui;

			hitIndexes[numIndexes] = numIndexes;
			hitIndexes[numIndexes+1] = numIndexes+1;
			hitIndexes[numIndexes+2] = numIndexes+2;
			numIndexes += 3;

			if (numIndexes>=SHADER_MAX_VERTEXES-3)
			{
				break; // we are out of space, so we are done :)
			}
		}

		if ( !numIndexes ) {
			continue;
		}

		//don't have fog enabled when we redraw with alpha test, or it will double over
		//and screw the tri up -rww
		if (r_drawfog->value == 2 &&
			tr.world &&
			(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
		{
			fogging = qglIsEnabled(GL_FOG);

			if (fogging)
			{
				qglDisable(GL_FOG);
			}
		}
		else
		{
			fogging = 0;
		}


		dStage = NULL;
		if (tess.shader)
		{
			int i = 0;
			while (i < tess.shader->numUnfoggedPasses)
			{
				const int blendBits = (GLS_SRCBLEND_BITS+GLS_DSTBLEND_BITS);
				if (((tess.shader->stages[i].bundle[0].image && !tess.shader->stages[i].bundle[0].isLightmap && !tess.shader->stages[i].bundle[0].numTexMods && tess.shader->stages[i].bundle[0].tcGen != TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i].bundle[0].tcGen != TCGEN_FOG) ||
					 (tess.shader->stages[i].bundle[1].image && !tess.shader->stages[i].bundle[1].isLightmap && !tess.shader->stages[i].bundle[1].numTexMods && tess.shader->stages[i].bundle[1].tcGen != TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i].bundle[1].tcGen != TCGEN_FOG)) &&
					(tess.shader->stages[i].stateBits & blendBits) == 0 )
				{ //only use non-lightmap opaque stages
                    dStage = &tess.shader->stages[i];
					break;
				}
				i++;
			}
		}
		if (!needResetVerts)
		{
			needResetVerts=1;
			if (qglUnlockArraysEXT)
			{
				qglUnlockArraysEXT();
				GLimp_LogComment( "glUnlockArraysEXT\n" );
			}
		}
		qglVertexPointer (3, GL_FLOAT, 16, vertCoordsArray);	// padded for SIMD

		if (dStage)
		{
			GL_SelectTexture( 0 );
			GL_State(0);
			qglTexCoordPointer( 2, GL_FLOAT, 0, oldTexCoordsArray[0] );
			if (dStage->bundle[0].image && !dStage->bundle[0].isLightmap && !dStage->bundle[0].numTexMods && dStage->bundle[0].tcGen != TCGEN_ENVIRONMENT_MAPPED && dStage->bundle[0].tcGen != TCGEN_FOG)
			{
				R_BindAnimatedImage( &dStage->bundle[0] );
			}
			else
			{
				R_BindAnimatedImage( &dStage->bundle[1] );
			}

			GL_SelectTexture( 1 );
			qglEnable( GL_TEXTURE_2D );
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
			GL_Bind( tr.dlightImage );
			GL_TexEnv( GL_MODULATE );


			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);// | GLS_ATEST_GT_0);

			R_DrawElements( numIndexes, hitIndexes );

			qglDisable( GL_TEXTURE_2D );
			GL_SelectTexture(0);
		}
		else
		{
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );

			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

			GL_Bind( tr.dlightImage );
			// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
			// where they aren't rendered
			if ( dl->additive ) {
				GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}
			else {
				GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}

			R_DrawElements( numIndexes, hitIndexes );
		}

		if (fogging)
		{
			qglEnable(GL_FOG);
		}

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
	if (needResetVerts)
	{
		qglVertexPointer (3, GL_FLOAT, 16, tess.xyz);	// padded for SIMD
		if (qglLockArraysEXT)
		{
			qglLockArraysEXT(0, tess.numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
	}
}

static void ProjectDlightTexture( void ) {
	int		i, l;
	vec3_t	origin;
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
	glIndex_t	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	scale;
	float	radius;
	int		fogging;
	vec3_t	floatColor;
	shaderStage_t *dStage;

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}

		texCoords = texCoordsArray[0];
		colors = colorArray[0];

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		scale = 1.0f / radius;

		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;

		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords += 2, colors += 4 ) {
			vec3_t	dist;
			int		clip;
			float	modulate;

			backEnd.pc.c_dlightVertexes++;

			VectorSubtract( origin, tess.xyz[i], dist );

			int l = 1;
			int bestIndex = 0;
			float greatest = tess.normal[i][0];
			if (greatest < 0.0f)
			{
				greatest = -greatest;
			}

			if (VectorCompare(tess.normal[i], vec3_origin))
			{ //damn you terrain!
				bestIndex = 2;
			}
			else
			{
				while (l < 3)
				{
					if ((tess.normal[i][l] > greatest && tess.normal[i][l] > 0.0f) ||
						(tess.normal[i][l] < -greatest && tess.normal[i][l] < 0.0f))
					{
						greatest = tess.normal[i][l];
						if (greatest < 0.0f)
						{
							greatest = -greatest;
						}
						bestIndex = l;
					}
					l++;
				}
			}

			float dUse = 0.0f;
			const float maxScale = 1.5f;
			const float maxGroundScale = 1.4f;
			const float lightScaleTolerance = 0.1f;

			if (bestIndex == 2)
			{
				dUse = origin[2]-tess.xyz[i][2];
				if (dUse < 0.0f)
				{
					dUse = -dUse;
				}
				dUse = (radius*0.5f)/dUse;
				if (dUse > maxGroundScale)
				{
					dUse = maxGroundScale;
				}
				else if (dUse < 0.1f)
				{
					dUse = 0.1f;
				}

				if (VectorCompare(tess.normal[i], vec3_origin) ||
					tess.normal[i][0] > lightScaleTolerance ||
					tess.normal[i][0] < -lightScaleTolerance ||
					tess.normal[i][1] > lightScaleTolerance ||
					tess.normal[i][1] < -lightScaleTolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius*dUse);
				}

				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[1] * scale;
			}
			else if (bestIndex == 1)
			{
				dUse = origin[1]-tess.xyz[i][1];
				if (dUse < 0.0f)
				{
					dUse = -dUse;
				}
				dUse = (radius*0.5f)/dUse;
				if (dUse > maxScale)
				{
					dUse = maxScale;
				}
				else if (dUse < 0.1f)
				{
					dUse = 0.1f;
				}
				if (tess.normal[i][0] > lightScaleTolerance ||
					tess.normal[i][0] < -lightScaleTolerance ||
					tess.normal[i][2] > lightScaleTolerance ||
					tess.normal[i][2] < -lightScaleTolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius*dUse);
				}

				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}
			else
			{
				dUse = origin[0]-tess.xyz[i][0];
				if (dUse < 0.0f)
				{
					dUse = -dUse;
				}
				dUse = (radius*0.5f)/dUse;
				if (dUse > maxScale)
				{
					dUse = maxScale;
				}
				else if (dUse < 0.1f)
				{
					dUse = 0.1f;
				}
				if (tess.normal[i][2] > lightScaleTolerance ||
					tess.normal[i][2] < -lightScaleTolerance ||
					tess.normal[i][1] > lightScaleTolerance ||
					tess.normal[i][1] < -lightScaleTolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius*dUse);
				}

				texCoords[0] = 0.5f + dist[1] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}

			clip = 0;
			if ( texCoords[0] < 0.0f ) {
				clip |= 1;
			} else if ( texCoords[0] > 1.0f ) {
				clip |= 2;
			}
			if ( texCoords[1] < 0.0f ) {
				clip |= 4;
			} else if ( texCoords[1] > 1.0f ) {
				clip |= 8;
			}
			// modulate the strength based on the height and color
			if ( dist[bestIndex] > radius ) {
				clip |= 16;
				modulate = 0.0f;
			} else if ( dist[bestIndex] < -radius ) {
				clip |= 32;
				modulate = 0.0f;
			} else {
				dist[bestIndex] = Q_fabs(dist[bestIndex]);
				if ( dist[bestIndex] < radius * 0.5f ) {
					modulate = 1.0f;
				} else {
					modulate = 2.0f * (radius - dist[bestIndex]) * scale;
				}
			}
			clipBits[i] = clip;

			colors[0] = Q_ftol(floatColor[0] * modulate);
			colors[1] = Q_ftol(floatColor[1] * modulate);
			colors[2] = Q_ftol(floatColor[2] * modulate);
			colors[3] = 255;
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		//don't have fog enabled when we redraw with alpha test, or it will double over
		//and screw the tri up -rww
		if (r_drawfog->value == 2 &&
			tr.world &&
			(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
		{
			fogging = qglIsEnabled(GL_FOG);

			if (fogging)
			{
				qglDisable(GL_FOG);
			}
		}
		else
		{
			fogging = 0;
		}


		dStage = NULL;
		if (tess.shader && qglActiveTextureARB)
		{
			int i = 0;
			while (i < tess.shader->numUnfoggedPasses)
			{
				const int blendBits = (GLS_SRCBLEND_BITS+GLS_DSTBLEND_BITS);
				if (((tess.shader->stages[i].bundle[0].image && !tess.shader->stages[i].bundle[0].isLightmap && !tess.shader->stages[i].bundle[0].numTexMods) ||
					 (tess.shader->stages[i].bundle[1].image && !tess.shader->stages[i].bundle[1].isLightmap && !tess.shader->stages[i].bundle[1].numTexMods)) &&
					(tess.shader->stages[i].stateBits & blendBits) == 0 )
				{ //only use non-lightmap opaque stages
                    dStage = &tess.shader->stages[i];
					break;
				}
				i++;
			}
		}

		if (dStage)
		{
			GL_SelectTexture( 0 );
			GL_State(0);
			qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );
			if (dStage->bundle[0].image && !dStage->bundle[0].isLightmap && !dStage->bundle[0].numTexMods)
			{
				R_BindAnimatedImage( &dStage->bundle[0] );
			}
			else
			{
				R_BindAnimatedImage( &dStage->bundle[1] );
			}

			GL_SelectTexture( 1 );
			qglEnable( GL_TEXTURE_2D );
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
			GL_Bind( tr.dlightImage );
			GL_TexEnv( GL_MODULATE );

			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);// | GLS_ATEST_GT_0);

			R_DrawElements( numIndexes, hitIndexes );

			qglDisable( GL_TEXTURE_2D );
			GL_SelectTexture(0);
		}
		else
		{
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );

			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

			GL_Bind( tr.dlightImage );
			// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
			// where they aren't rendered
			if ( dl->additive ) {
				GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}
			else {
				GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}

			R_DrawElements( numIndexes, hitIndexes );
		}

		if (fogging)
		{
			qglEnable(GL_FOG);
		}

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
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
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][0][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][0][1];
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
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = DotProduct( tess.xyz[i], pStage->bundle[b].tcGenVectors[0] );
				tess.svars.texcoords[b][i][1] = DotProduct( tess.xyz[i], pStage->bundle[b].tcGenVectors[1] );
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
static void RB_IterateStagesGeneric( shaderCommands_t *input )
{
	bool UseGLFog = false;
	bool FogColorChange = false;
	fog_t *fog = NULL;

#ifdef STRAWB
	if (tess.fogNum &&
		tess.shader->fogPass &&
		(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs) &&
		r_drawfog->value == 2)
	{
		// only gl fog global fog and the "special fog"
		fog = tr.world->fogs + tess.fogNum;

		if (tr.rangedFog)
		{
			//ranged fog, used for sniper scope
			float fStart = fog->parms.depthForOpaque;

			if (tr.rangedFog < 0.0f)
			{
				//special designer override
				fStart = -tr.rangedFog;
			}
			else
			{
				// the greater tr.rangedFog is, the more fog we will get
				// between the view point and cull distance
				if ((tr.distanceCull - fStart) < tr.rangedFog)
				{
					// ensure a minimum range between fog beginning and cutoff
					// distance
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

		if (g_bRenderGlowingObjects)
		{
			const float fogColor[3] = { 0.0f, 0.0f, 0.0f };
			qglFogfv(GL_FOG_COLOR, fogColor);
		}
		else
		{
			qglFogfv(GL_FOG_COLOR, fog->parms.color);
		}

		qglEnable(GL_FOG);
		UseGLFog = true;
	}
#endif

	for (int stage = 0; stage < input->shader->numUnfoggedPasses; stage++)
	{
		shaderStage_t *pStage = &tess.xstages[stage];
		int forceRGBGen = 0;
		int stateBits = 0;

		if (!pStage->active)
		{
			break;
		}

		// Reject this stage if it's not a glow stage but we are doing a glow
		// pass.
		if (g_bRenderGlowingObjects && !pStage->glow)
		{
			continue;
		}

		if (stage &&
			r_lightmap->integer &&
			!(pStage->bundle[0].isLightmap ||
				pStage->bundle[1].isLightmap ||
				pStage->bundle[0].vertexLightmap))
		{
			break;
		}

		stateBits = pStage->stateBits;

		assert(backEnd.currentEntity->e.renderfx >= 0);

		if (backEnd.currentEntity->e.renderfx & RF_DISINTEGRATE1)
		{
			// we want to be able to rip a hole in the thing being
			// disintegrated, and by doing the depth-testing it avoids some
			// kinds of artefacts, but will probably introduce others?
			stateBits =
				GLS_SRCBLEND_SRC_ALPHA |
				GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA |
				GLS_DEPTHMASK_TRUE |
				GLS_ATEST_GE_C0;
		}

		if (backEnd.currentEntity->e.renderfx & RF_RGB_TINT)
		{
			//want to use RGBGen from ent
			forceRGBGen = CGEN_ENTITY;
		}

		if (pStage->ss && pStage->ss->surfaceSpriteType)
		{
			// We check for surfacesprites AFTER drawing everything else
			continue;
		}

#ifdef STRAWB
		if (UseGLFog)
		{
			if (pStage->mGLFogColorOverride)
			{
				qglFogfv(
					GL_FOG_COLOR,
					GLFogOverrideColors[pStage->mGLFogColorOverride]);
				FogColorChange = true;
			}
			else if (FogColorChange && fog)
			{
				FogColorChange = false;
				qglFogfv(GL_FOG_COLOR, fog->parms.color);
			}
		}
#endif

		if (!input->fading)
		{
			//this means ignore this, while we do a fade-out
			ComputeColors(pStage, forceRGBGen);
		}

		ComputeTexCoords(pStage);

		if (!setArraysOnce)
		{
			// STRAWB upload color arrays
		}

		//
		// do multitexture
		//
		if (pStage->bundle[1].image != 0)
		{
			DrawMultitextured(input, stage);
		}
		else
		{
			bool lStencilled = false;

			if (!setArraysOnce)
			{
				// STRAWB upload texcoord arrays
			}

			//
			// set state
			//
			if ((tess.shader == tr.distortionShader) ||
				 (backEnd.currentEntity &&
				  (backEnd.currentEntity->e.renderfx & RF_DISTORTION)))
			{
				GL_Bind(tr.screenImage);
				GL_Cull(CT_TWO_SIDED);
			}
			else if (pStage->bundle[0].vertexLightmap &&
					 (r_vertexLight->integer && !r_uiFullScreen->integer) &&
					 r_lightmap->integer)
			{
				GL_Bind(tr.whiteImage);
			}
			else
			{
				R_BindAnimatedImage( &pStage->bundle[0] );
			}

			if (tess.shader == tr.distortionShader &&
				glConfig.stencilBits >= 4)
			{
#ifdef STRAWB
				// draw it to the stencil buffer!
				tr_stencilled = true;
				lStencilled = true;
				qglEnable(GL_STENCIL_TEST);
				qglStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
				qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

				//don't depthmask, don't blend.. don't do anything
				GL_State(0);
#endif
			}
			else if (backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA)
			{
				ForceAlpha(
					(unsigned char *)tess.svars.colors,
					backEnd.currentEntity->e.shaderRGBA[3]);
				if (backEnd.currentEntity->e.renderfx & RF_ALPHA_DEPTH)
				{
					// depth write, so faces through the model will be
					// stomped over by nearer ones. this works because we
					// draw RF_FORCE_ENT_ALPHA stuff after everything else,
					// including standard alpha surfs.
					GL_State(
						GLS_SRCBLEND_SRC_ALPHA |
						GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA |
						GLS_DEPTHMASK_TRUE);
				}
				else
				{
					GL_State(
						GLS_SRCBLEND_SRC_ALPHA |
						GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
				}
			}
			else
			{
				GL_State( stateBits );
			}

			//
			// draw
			//
			R_DrawElements( input->numIndexes, input->indexes );

			if (lStencilled)
			{
#ifdef STRAWB
				// re-enable the color buffer, disable stencil test
				lStencilled = false;
				qglDisable(GL_STENCIL_TEST);
				qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif
			}
		}
	}

#ifdef STRAWB
	if (FogColorChange)
	{
		qglFogfv(GL_FOG_COLOR, fog->parms.color);
	}
#endif
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;
	int stage;

	input = &tess;

	RB_DeformTessGeometry();

	//
	// log this call
	//
	if ( r_logFile->integer )
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	// set polygon offset if necessary
	if ( input->shader->polygonOffset )
	{
		GL_PolygonOffset(true);
#if 0
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
#endif
	}

	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	if ( tess.numPasses > 1 || input->shader->multitextureEnv )
	{
		setArraysOnce = qfalse;
	}
	else
	{
		setArraysOnce = qtrue;
		// STRAWB upload color and texcoords here
		// tess.svars.colors
		// tess.svars.texcoords[0]
	}

	//
	// lock XYZ
	//
	// STRAWB upload position data from tess.xyz

	//
	// call shader function
	//
	RB_IterateStagesGeneric( input );

	//
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE
		&& !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		if (r_dlightStyle->integer>0)
		{
			ProjectDlightTexture2();
		}
		else
		{
			ProjectDlightTexture();
		}
	}

	//
	// now do fog
	//
	if (tr.world &&
		(tess.fogNum != tr.world->globalFog || r_drawfog->value != 2) &&
		r_drawfog->value &&
		tess.fogNum &&
		tess.shader->fogPass)
	{
		RB_FogPass();
	}

	//
	// unlock arrays
	//
	if (qglUnlockArraysEXT)
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}

	//
	// reset polygon offset
	//
	if ( input->shader->polygonOffset )
	{
		GL_PolygonOffset(false);
#if 0
		qglDisable( GL_POLYGON_OFFSET_FILL );
#endif
	}

#if 0
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

	if (skyboxportal)
	{
		const bool isTessUsingSkyFunc =
			(tess.currentStageIteratorFunc == RB_StageIteratorSky);
		if (!(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL))
		{
			// world
			if (isTessUsingSkyFunc)
			{
				// don't process these tris at all
				return;
			}
		}
		else
		{
			// portal sky
			if (!drawskyboxportal)
			{
				if (isTessUsingSkyFunc)
				{
					// /only/ process sky tris
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
