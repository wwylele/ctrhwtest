#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "vshader_shbin.h"

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct { float position[3]; float texcoord[2]; float normal[3]; } vertex;

static const vertex vertex_list[] =
{
	// First face (PZ)
	// First triangle
	{ {-0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },
	{ {+0.5f, -0.5f, +0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	// Second triangle
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	{ {-0.5f, +0.5f, +0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	{ {-0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },

};

#define vertex_list_count (sizeof(vertex_list)/sizeof(vertex_list[0]))

static PrintConsole bottomScreen;

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection, uLoc_modelView, uLoc_normal, uLoc_view;
static C3D_Mtx projection;

static C3D_LightEnv lightEnv;
static C3D_Light light[4];
static C3D_LightLut lut, lut2;

static void* vbo_data;

struct Vec4 {
    float x,y,z,w;
};

/*C3D_FVec*/ struct Vec4 lightVec[4] = {
{ 0.0, 0.0, 1.0, 0.0 }, 
{ 0.8, 0.0, 0.6, 0.0 }, 
{ 0.866025404, 0.0, 0.5, 0.0 }, 
{ 0.6, 0.0, 0.8, 0.0 }, 
};

bool light_enable[4] = {true, true ,true, true};

static float lut_func(float angle, float a)
{
    return 1.0;
}

static float lut2_func(float angle, float a)
{
    return angle;
}

static void sceneInit(void)
{
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	// Get the location of the uniforms
	uLoc_projection   = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	uLoc_modelView    = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
	uLoc_normal       = shaderInstanceGetUniformLocation(program.vertexShader, "normal");
	uLoc_view         = shaderInstanceGetUniformLocation(program.vertexShader, "view");

	// Configure attributes for use with the vertex shader
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
	AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3); // v2=normal

	// Create the VBO (vertex buffer object)
	vbo_data = linearAlloc(sizeof(vertex_list));
	memcpy(vbo_data, vertex_list, sizeof(vertex_list));

	// Configure buffers
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, vbo_data, sizeof(vertex), 3, 0x210);

	// Configure the first fragment shading substage to blend the fragment primary color
	// with the fragment secondary color.
	// See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_RGB, GPU_FRAGMENT_SECONDARY_COLOR, 0, 0);
	C3D_TexEnvSrc(env, C3D_Alpha, GPU_CONSTANT, 0, 0);
	C3D_TexEnvColor(env, 0xFFFFFFFF);
	C3D_TexEnvOp(env, C3D_RGB, GPU_TEVOP_RGB_SRC_ALPHA, 0, 0);
	C3D_TexEnvOp(env, C3D_Alpha, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	static const C3D_Material material =
	{
		{ 0.0f, 0.0f, 0.0f }, //ambient
		{ 0.0f, 0.0f, 0.0f }, //diffuse
		{ 1.0f, 1.0f, 1.0f }, //specular0
		{ 0.0f, 0.0f, 0.0f }, //specular1
		{ 0.0f, 0.0f, 0.0f }, //emission
	};

	C3D_LightEnvInit(&lightEnv);
	C3D_LightEnvBind(&lightEnv);
	C3D_LightEnvMaterial(&lightEnv, &material);
	C3D_LightEnvClampHighlights(&lightEnv, false);

	LightLut_FromFunc(&lut, lut_func, 0, true);
	C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_NH, true, &lut);
	
	LightLut_FromFunc(&lut2, lut2_func, 0, true);
	C3D_LightEnvLut(&lightEnv, GPU_LUT_FR, GPU_LUTINPUT_LN, true, &lut2);
	
	C3D_LightEnvFresnel(&lightEnv, 3);
	C3D_LightEnvClampHighlights(&lightEnv, false);
	
	for (int i = 0; i<4 ; ++i) {
	
	    C3D_LightInit(&light[i], &lightEnv);
	    C3D_LightColor(&light[i], 1.0, 1.0, 1.0);
	    C3D_FVec fuckyou;
	    fuckyou.x = lightVec[i].x;
	    fuckyou.y = lightVec[i].y;
	    fuckyou.z = lightVec[i].z;
	    fuckyou.w = lightVec[i].w;
	    C3D_LightPosition(&light[i], &fuckyou);
	    C3D_LightTwoSideDiffuse(&light[i], true);
	}
	
}

float normal[3] = {0.0, 0.0, 1.0};
float view[3] = {0.0, 0.0, 1.0};

static void sceneRender(float iod)
{
	// Compute the projection matrix
	Mtx_PerspStereoTilt(&projection, 40.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 1000.0f, iod, 2.0f, false);
	
	
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_normal, normal[0], normal[1], normal[2], 0.0);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_view, view[0], view[1], view[2], 0.0);
	    
    // 
	    
	C3D_Mtx modelView;
	Mtx_Identity(&modelView);
	Mtx_Translate(&modelView, 0.0, 0.0, -3.0, true);
	
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView,  &modelView);

	// Draw the VBO
	C3D_DrawArrays(GPU_TRIANGLES, 0, vertex_list_count);

}

static void sceneExit(void)
{
	// Free the VBO
	linearFree(vbo_data);

	// Free the shader program
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
}

void updateText() {
    consoleClear();
    printf("\x1b[0;0HHello!\n");
    for(int i = 0; i<4; ++i) {
        printf("light %d = %s,  Fresnel = %f\n", i, light_enable[i] ? "on": "off", lightVec[i].z);
    }
    
    u8* fb = (u8*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    u8* left = fb + 3 * (120 + 200 * 240);
    printf("fresnel = %f\n", (float)left[0]/0xFF);
}


int main()
{
	// Initialize graphics
	gfxInitDefault();
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&bottomScreen);
	gfxSetDoubleBuffering(GFX_BOTTOM, true);
	
	gfxSet3D(true); // Enable stereoscopic 3D
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);


	// Initialize the render targets
	C3D_RenderTarget* targetLeft  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTarget* targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetClear(targetLeft,   C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_RenderTargetSetClear(targetRight,  C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_RenderTargetSetOutput(targetLeft,  GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	// Initialize the scene
	sceneInit();

	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu
			
		if (kDown & KEY_A) light_enable[0] = !light_enable[0];
		if (kDown & KEY_B) light_enable[1] = !light_enable[1];
		if (kDown & KEY_X) light_enable[2] = !light_enable[2];
		if (kDown & KEY_Y) light_enable[3] = !light_enable[3];
			

		float slider = osGet3DSliderState();
		float iod = slider/3;
		
		for (int i =0;i<4;++i) C3D_LightEnable(&light[i], light_enable[i]);

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		{
			C3D_FrameDrawOn(targetLeft);
			sceneRender(-iod);

			if (iod > 0.0f)
			{
				C3D_FrameDrawOn(targetRight);
				sceneRender(iod);
			}
		}
		C3D_FrameEnd(0);
		
		static int k = 0;
	
		if (k == 4)updateText();
		
		++k;
		k %= 8;
	}

	// Deinitialize the scene
	sceneExit();

	// Deinitialize graphics
	C3D_Fini();
	gfxExit();
	return 0;
}
